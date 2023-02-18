#include "main.h"

#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uart_resend.h"

static int fd = -1;
static hio_t *mio;
static char ota_power_state = 0;
static int ecb_msg_get_count = 0;
static pthread_mutex_t lock;
LIST_HEAD(ECB_LIST_RESEND);

void ecb_resend_list_add(void *resend)
{
    uart_resend_t *uart_resend = (uart_resend_t *)resend;
    resend_list_add(&ECB_LIST_RESEND, &uart_resend->node);
}

void ecb_resend_list_del_by_id(const int resend_seq_id)
{
    resend_list_del_by_id(&ECB_LIST_RESEND, resend_seq_id);
}

void ecb_resend_list_clear()
{
    resend_list_clear(&ECB_LIST_RESEND);
}

void set_ecb_ota_power_state(char state)
{
    ota_power_state = state;
}
char get_ecb_ota_power_state()
{
    return ota_power_state;
}

int ecb_uart_resend_cb(const unsigned char *in, int in_len);
int ecb_uart_send(const unsigned char *in, int in_len, unsigned char resend_flag, unsigned char iscopy)
{
    if (mio == NULL)
    {
        LOGW("%s,mio NULL", __func__);
        return -1;
    }
    if (in == NULL || in_len <= 0)
    {
        return -1;
    }

    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        if (resend_flag)
        {
            uart_resend_t *resend = (uart_resend_t *)malloc(sizeof(uart_resend_t));
            if (resend == NULL)
            {
                LOGE("malloc error\n");
                resend_flag = -1;
                goto resend_fail;
            }
            resend->send_len = in_len;
            if (iscopy)
            {
                resend->send_data = (unsigned char *)malloc(resend->send_len);
                if (resend->send_data == NULL)
                {
                    LOGE("malloc error\n");
                    free(resend);
                    resend_flag = -1;
                    goto resend_fail;
                }
                memcpy(resend->send_data, in, resend->send_len);
            }
            else
                resend->send_data = (unsigned char *)in;

            if (resend->send_len >= 4)
                resend->resend_seq_id = resend->send_data[2] * 256 + resend->send_data[3];
            resend->resend_cnt = RESEND_CNT;
            resend->resend_cb = ecb_uart_resend_cb;
            resend->wait_tick = resend_tick_set(get_systime_ms(), RESEND_WAIT_TICK);
            ecb_resend_list_add(resend);
        }
    resend_fail:
        hio_write(mio, in, in_len);
        if (resend_flag < 0)
        {
            res = -1;
        }
        else
        {
            if (resend_flag)
                usleep(50000);
        }
        pthread_mutex_unlock(&lock);
    }
    else
    {
        LOGE("pthread_mutex_lock error\n");
    }
    return res;
}

int ecb_uart_resend_cb(const unsigned char *in, int in_len)
{
    printf("%s,resend send...start\n", __func__);
    mlogHex(in, in_len);
    printf("%s,resend send...end\n", __func__);
    return ecb_uart_send(in, in_len, 0, 0);
}

static void on_uart_read(hio_t *io, void *buf, int readbytes)
{
    static unsigned char uart_read_buf[512];
    static int uart_read_buf_index = 0;
    int uart_read_len = readbytes;

    if (uart_read_len > 0)
    {
        if (uart_read_buf_index + uart_read_len >= sizeof(uart_read_buf))
        {
            memcpy(uart_read_buf, buf, uart_read_len);
            uart_read_buf_index = uart_read_len;
        }
        else
        {
            memcpy(&uart_read_buf[uart_read_buf_index], buf, uart_read_len);
            uart_read_buf_index += uart_read_len;
        }
        LOGW("recv from ecb--------------------------uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        mlogHex(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg);
        LOGW("uart_read_buf_index:%d", uart_read_buf_index);
        //  mlogHex(uart_read_buf, uart_read_buf_index);
    }
}

static void on_idle(hidle_t *idle)
{
    resend_list_each(&ECB_LIST_RESEND);
    if (ota_power_state != 0)
        return;
    static int ecb_msg_get_timeout = MSG_GET_SHORT_TIME;

    int msg_get_status = ecb_uart_msg_get(false);

    if (ecb_msg_get_timeout == MSG_GET_SHORT_TIME && ECB_UART_CONNECTED == msg_get_status)
    {
        ecb_msg_get_timeout = MSG_GET_LONG_TIME;
    }
    else if (ecb_msg_get_timeout == MSG_GET_LONG_TIME && ECB_UART_DISCONNECT == msg_get_status)
    {
        ecb_msg_get_timeout = MSG_GET_SHORT_TIME;
    }

    if (++ecb_msg_get_count > ecb_msg_get_timeout)
    {
        ecb_msg_get_count = 0;
        ecb_uart_msg_get(true);
        LOGW("ecb_uart_msg_get\n");
    }
    if (ecb_uart_heart_timeout(false) < MSG_HEART_TIME)
    {
        if (ecb_uart_heart_timeout(true) == MSG_HEART_TIME)
        {
            send_error_to_cloud(POWER_BOARD_ERROR_CODE);
        }
    }
}
/*********************************************************************************
 *Function:  ecb_uart_init
 *Description： ecb任务函数
 *Input:
 *Return:
 **********************************************************************************/
void ecb_uart_init(void)
{
    fd = uart_init("/dev/ttyS4", BAUDRATE_115200, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        LOGE("ecb_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    LOGI("ecb_uart,fd:%d", fd);
    pthread_mutex_init(&lock, NULL);

    hidle_add(g_loop, on_idle, INFINITE);
    static char uart_buf[256];
    mio = hread(g_loop, fd, uart_buf, sizeof(uart_buf), on_uart_read);

    ecb_uart_msg_get(true);
}
void ecb_uart_deinit(void)
{
    close(fd);
    pthread_mutex_destroy(&lock);
}

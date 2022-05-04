#include "main.h"

#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uart_resend.h"
#include "uart_task.h"

enum msg_get_time_t
{
    MSG_GET_SHORT_TIME = 3 * 5,
    MSG_GET_LONG_TIME = 2 * 30 * 5,
    MSG_HEART_TIME = 15 * 5,
};

static unsigned short ecb_seq_id = 0;
static int ecb_msg_get_count = 0;
static struct Select_Client_Event select_client_event;

static int fd = -1;
static pthread_mutex_t lock;
// LIST_HEAD(ECB_LIST_RESEND);
static struct list_head ECB_LIST_RESEND;

void ecb_resend_list_add(void *resend)
{
    uart_resend_t *uart_resend = (uart_resend_t *)resend;
    resend_list_add(&ECB_LIST_RESEND, &uart_resend->node);
}

void ecb_resend_list_del_by_id(const int resend_seq_id)
{
    resend_list_del_by_id(&ECB_LIST_RESEND, resend_seq_id);
}

static void InvertUint16(unsigned short *dBuf, unsigned short *srcBuf)
{
    int i;
    unsigned short tmp[4] = {0};

    for (i = 0; i < 16; i++)
    {
        if (srcBuf[0] & (1 << i))
            tmp[0] |= 1 << (15 - i);
    }
    dBuf[0] = tmp[0];
}

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen)
{
    unsigned short wCRCin = 0x0000;
    unsigned short wCPoly = 0x8005;

    InvertUint16(&wCPoly, &wCPoly);
    while (datalen--)
    {
        wCRCin ^= *(data++);
        for (int i = 0; i < 8; i++)
        {
            if (wCRCin & 0x01)
                wCRCin = (wCRCin >> 1) ^ wCPoly;
            else
                wCRCin = wCRCin >> 1;
        }
    }
    return (wCRCin ^ 0xFFFF);
}
int ecb_uart_resend_cb(const unsigned char *in, int in_len);
int ecb_uart_send(const unsigned char *in, int in_len, unsigned char resend, unsigned char iscopy)
{
    dzlog_warn("uart send to ecb--------------------------");
    hdzlog_info(in, in_len);
    if (fd < 0)
    {
        dzlog_error("ecb_uart_send fd error\n");
        return -1;
    }
    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        if (in == NULL || in_len <= 0)
        {
            res = 0;
            goto fail;
        }

        res = write(fd, in, in_len);
        if (resend)
        {
            uart_resend_t *resend = (uart_resend_t *)malloc(sizeof(uart_resend_t));
            resend->send_len = in_len;
            if (iscopy)
            {
                resend->send_data = (unsigned char *)malloc(resend->send_len);
                memcpy(resend->send_data, in, resend->send_len);
            }
            else
                resend->send_data = (unsigned char *)in;

            // resend->fd = fd;
            if (resend->send_len >= 4)
                resend->resend_seq_id = resend->send_data[2] * 256 + resend->send_data[3];
            resend->resend_cnt = RESEND_CNT;
            resend->resend_cb = ecb_uart_resend_cb;
            resend->wait_tick = resend_tick_set(get_systime_ms(), RESEND_WAIT_TICK);
            ecb_resend_list_add(resend);
        }
    fail:
        pthread_mutex_unlock(&lock);
        return res;
    }
    else
    {
        dzlog_error("pthread_mutex_lock error\n");
    }
    return res;
}

int ecb_uart_resend_cb(const unsigned char *in, int in_len)
{
    return ecb_uart_send(in, in_len, 0, 0);
}

int ecb_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id)
{
    int index = 0;
    unsigned char *send_msg = (unsigned char *)malloc(ECB_MSG_MIN_LEN + msg_len);
    send_msg[index++] = 0xe6;
    send_msg[index++] = 0xe6;
    if (seq_id < 0)
        seq_id = ecb_seq_id++;

    send_msg[index++] = seq_id >> 8;
    send_msg[index++] = seq_id & 0xff;
    send_msg[index++] = command;
    send_msg[index++] = msg_len >> 8;
    send_msg[index++] = msg_len & 0xff;
    if (msg_len > 0 && msg != NULL)
    {
        memcpy(&send_msg[index], msg, msg_len);
        index += msg_len;
    }
    unsigned short crc16 = CRC16_MAXIM((const unsigned char *)(send_msg + 2), index - 2);
    send_msg[index++] = crc16 >> 8;
    send_msg[index++] = crc16 & 0xff;
    send_msg[index++] = 0x6e;
    send_msg[index++] = 0x6e;
    int res = ecb_uart_send(send_msg, ECB_MSG_MIN_LEN + msg_len, resend, 0);
    if (resend == 0)
    {
        free(send_msg);
    }
    return res;
}

static int ecb_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[512];
    int uart_read_len;
    static int uart_read_buf_index = 0;

    uart_read_len = read(fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
    if (uart_read_len > 0)
    {
        uart_read_buf_index += uart_read_len;
        dzlog_warn("recv from ecb-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg);
        dzlog_warn("uart_read_buf_index:%d", uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}

static int ecb_except_cb(void *arg)
{
    return 0;
}

static int ecb_timeout_cb(void)
{
    resend_list_each(&ECB_LIST_RESEND);

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
        dzlog_warn("ecb_uart_msg_get\n");
    }
    if (ecb_uart_heart_timeout(false) < MSG_HEART_TIME)
    {
        if (ecb_uart_heart_timeout(true) == MSG_HEART_TIME)
        {
            send_error_to_cloud(POWER_BOARD_ERROR_CODE);
        }
    }

    return 0;
}

void ecb_uart_deinit(void)
{
    close(fd);
    pthread_mutex_destroy(&lock);
}
/*********************************************************************************
 *Function:  ecb_uart_init
 *Description： ecb任务函数
 *Input:
 *Return:
 **********************************************************************************/
void ecb_uart_init(void)
{
    fd = uart_init("/dev/ttyS0", BAUDRATE_115200, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        dzlog_error("ecb_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("ecb_uart,fd:%d", fd);
    pthread_mutex_init(&lock, NULL);
    resend_list_init(&ECB_LIST_RESEND);
    register_select_uart_timeout_cb(ecb_timeout_cb);

    select_client_event.fd = fd;
    select_client_event.read_cb = ecb_recv_cb;
    select_client_event.except_cb = ecb_except_cb;

    add_select_client_uart(&select_client_event);
    ecb_uart_msg_get(true);
}

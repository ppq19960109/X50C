#include "main.h"

#include "uart_ecb_task.h"
#include "uart_ecb_parse.h"
#include "uart_resend.h"

static int running = 0;
static int ecb_fd = 0;
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

int uart_send_ecb(unsigned char *in, int in_len, unsigned char resend, unsigned char iscopy)
{
    hdzlog_info(in, in_len);
    if (ecb_fd <= 0)
    {
        dzlog_error("uart_send_ecb fd error\n");
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

        res = write(ecb_fd, in, in_len);
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
                resend->send_data = in;

            resend->fd = ecb_fd;
            resend->resend_cnt = RESEND_CNT;
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
void uart_ecb_task_close(void)
{
    running = 0;
}

/*********************************************************************************
  *Function:  uart_ecb_task
  *Description： ecb任务函数，接收ecb的数据并处理
  *Input:  
  *Return:
**********************************************************************************/
void *uart_ecb_task(void *arg)
{
    static int ecb_get_count = 0;
    static int ecb_get_timeout = 0;
    unsigned char boot_flag = 1;
    static unsigned char uart_read_buf[512];
    int uart_read_len, uart_read_buf_index = 0;

    ecb_fd = uart_init("/dev/ttyS0", BAUDRATE_115200, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE);
    if (ecb_fd <= 0)
    {
        dzlog_error("uart_ecb_task uart init error:%d,%s", errno, strerror(errno));
        return NULL;
    }
    dzlog_info("uart_ecb_task,fd:%d", ecb_fd);
    pthread_mutex_init(&lock, NULL);
    fd_set rfds, copy_fds;
    FD_ZERO(&rfds);
    FD_SET(ecb_fd, &rfds);
    int maxfd = ecb_fd;
    copy_fds = rfds;

    struct timeval timeout;

    uart_ecb_get_msg();
    int n;
    running = 1;
    while (running)
    {
        resend_list_each(&ECB_LIST_RESEND);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        rfds = copy_fds;
        n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        if (n < 0)
        {
            perror("select error");
            continue;
        }
        else if (n == 0) // 没有准备就绪的文件描述符  就进入下一次循环
        {
            unsigned char disconnect_count = get_ecb_disconnect_count();
            if (boot_flag > 0 && disconnect_count == 0)
            {
                boot_flag = 0;
            }
            if (boot_flag == 0 && disconnect_count < ECB_DISCONNECT_COUNT)
            {
                ecb_get_timeout = 5 * 60 * 2;
            }
            else
            {
                ecb_get_timeout = 5 * 3;
            }
            if (++ecb_get_count > ecb_get_timeout)
            {
                ecb_get_count = 0;
                // uart_ecb_get_msg();
                // dzlog_warn("select timeout:uart_ecb_get_msg\n");
            }
            // dzlog_warn("select timeout:%ld\n", timeout.tv_usec);
            continue;
        }
        else
        {
            if (FD_ISSET(ecb_fd, &rfds))
            {
                ecb_get_count = 0;

                uart_read_len = read(ecb_fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
                if (uart_read_len > 0)
                {
                    dzlog_warn("uart_read_len:%d\n", uart_read_len);
                    uart_read_buf_index += uart_read_len;
                    hdzlog_info(uart_read_buf, uart_read_buf_index);
                    uart_read_parse(uart_read_buf, &uart_read_buf_index);
                    hdzlog_info(uart_read_buf, uart_read_buf_index);
                }
            }
        }
    }
    close(ecb_fd);
    pthread_mutex_destroy(&lock);
    return NULL;
}

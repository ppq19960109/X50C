#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "logFunc.h"

#include "linkkit_solo.h"
#include "UartCfg.h"
#include "uart_ecb_task.h"
#include "uart_ecb_parse.h"
#include "uart_resend.h"
#include "cloud_process.h"

static int ecb_fd;
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

int uart_send_to_ecb(unsigned char *in, int in_len, unsigned char resend, unsigned char iscopy)
{
    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        if (in == NULL || in_len <= 0)
        {
            res = 0;
            goto fail;
        }
        MLOG_HEX("uart_send_to_ecb:", in, in_len);
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
        MLOG_ERROR("pthread_mutex_lock error\n");
    }
    return res;
}

void *uart_ecb_task(void *arg)
{
    fd_set rfds, copy_fds;
    FD_ZERO(&rfds);

    unsigned char uart_read_buf[256];
    int uart_read_len, uart_read_buf_index = 0;
    cloud_init();
    uart_ecb_parse_init();
    pthread_mutex_init(&lock, NULL);
    ecb_fd = uart_init("/dev/ttyS0", BAUDRATE_115200, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE);
    FD_SET(ecb_fd, &rfds);
    int maxfd = ecb_fd;
    copy_fds = rfds;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    while (1)
    {
        resend_list_each(&ECB_LIST_RESEND);

        rfds = copy_fds;
        int n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        if (n < 0)
        {
            perror("select error");
            continue;
        }
        else if (n == 0) // 没有准备就绪的文件描述符  就进入下一次循环
        {
            // printf("select timeout\n");
            continue;
        }
        else
        {
            if (FD_ISSET(ecb_fd, &rfds))
            {
                uart_read_len = read(ecb_fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf));
                if (uart_read_len > 0)
                {
                    uart_read_buf_index += uart_read_len;
                    MLOG_HEX("uart_read_parse before:", uart_read_buf, uart_read_buf_index);
                    uart_read_parse(uart_read_buf, &uart_read_buf_index);
                    MLOG_HEX("uart_read_parse after:", uart_read_buf, uart_read_buf_index);
                }
            }
        }
    }

    cloud_deinit();
    pthread_mutex_destroy(&lock);
}

#include "main.h"

#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"
#include "uart_task.h"

static struct Select_Client_Event select_client_event;
static int fd = -1;
static pthread_mutex_t lock;

int ecb_uart_send(const unsigned char *in, int in_len)
{
    if (fd < 0)
    {
        dzlog_error("ecb_uart_send fd error\n");
        return -1;
    }
    if (in == NULL || in_len <= 0)
    {
        return -1;
    }

    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        write(fd, in, in_len);
        usleep(60000);
        pthread_mutex_unlock(&lock);
    }
    else
    {
        dzlog_error("pthread_mutex_lock error\n");
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
        // dzlog_warn("recv from ecb-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg);
        // dzlog_warn("uart_read_buf_index:%d", uart_read_buf_index);
        //  hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}

static int ecb_except_cb(void *arg)
{
    return 0;
}

static int ecb_timeout_cb(void *arg)
{

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
    fd = uart_init("/dev/ttyS4", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        dzlog_error("ecb_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("ecb_uart,fd:%d", fd);
    pthread_mutex_init(&lock, NULL);
    register_select_uart_timeout_cb(ecb_timeout_cb);

    select_client_event.fd = fd;
    select_client_event.read_cb = ecb_recv_cb;
    select_client_event.except_cb = ecb_except_cb;

    add_select_client_uart(&select_client_event);
}

#include "main.h"

#include "ice_uart.h"
#include "ice_parse.h"
#include "ecb_uart_parse_msg.h"
#include "uart_task.h"
#include "uds_protocol.h"

// static int ice_msg_get_count = 0;
static struct Select_Client_Event select_client_event;
static int fd = -1;
static pthread_mutex_t lock;

int ice_uart_send(const unsigned char *in, int in_len)
{
    if (fd < 0)
    {
        dzlog_error("ice_uart_send fd error\n");
        return -1;
    }
    if (in == NULL || in_len <= 0)
    {
        return -1;
    }

    int res = 0;
    if (pthread_mutex_lock(&lock) == 0)
    {
        ssize_t nwrite = write(fd, in, in_len);
        dzlog_warn("ice_uart_send-------------nwrite:%ld", nwrite);
        hdzlog_warn(in, in_len);
        pthread_mutex_unlock(&lock);
    }
    else
    {
        dzlog_error("pthread_mutex_lock error\n");
    }
    return res;
}

static int ice_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[128];
    int uart_read_len;
    static int uart_read_buf_index = 0;

    uart_read_len = read(fd, &uart_read_buf[uart_read_buf_index], sizeof(uart_read_buf) - uart_read_buf_index);
    if (uart_read_len > 0)
    {
        uart_read_buf_index += uart_read_len;
        dzlog_warn("recv from ice_uart-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);
        hdzlog_info(uart_read_buf, uart_read_buf_index);

        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ice_uart_parse_msg);
        dzlog_warn("ice uart_read_buf_index:%d", uart_read_buf_index);
        //  hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}

static int ice_except_cb(void *arg)
{
    return 0;
}
// static int ice_timeout_cb(void *arg)
// {
//     static int ice_msg_get_timeout = MSG_GET_SHORT_TIME;

//     int msg_get_status = ice_uart_msg_get(false);

//     if (ice_msg_get_timeout == MSG_GET_SHORT_TIME && ECB_UART_CONNECTED == msg_get_status)
//     {
//         ice_msg_get_timeout = MSG_GET_LONG_TIME;
//     }
//     else if (ice_msg_get_timeout == MSG_GET_LONG_TIME && ECB_UART_DISCONNECT == msg_get_status)
//     {
//         ice_msg_get_timeout = MSG_GET_SHORT_TIME;
//     }

//     if (++ice_msg_get_count > ice_msg_get_timeout)
//     {
//         ice_msg_get_count = 0;
//         ice_uart_msg_get(true);
//         dzlog_warn("ice_uart_msg_get\n");
//     }
//     return 0;
// }
void ice_uart_deinit(void)
{
    close(fd);
    pthread_mutex_destroy(&lock);
}

void ice_uart_init(void)
{
    fd = uart_init("/dev/ttyS0", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        dzlog_error("ice_uart uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("ice_uart,fd:%d", fd);

    select_client_event.fd = fd;
    select_client_event.read_cb = ice_recv_cb;
    select_client_event.except_cb = ice_except_cb;
    // select_client_event.timeout_cb = ice_timeout_cb;

    add_select_client_uart(&select_client_event);
    ice_uart_msg_get(true);
}

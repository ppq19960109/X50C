#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"
#include "ecb_uart_parse_msg.h"

static pthread_mutex_t mutex;

int send_to_uds(unsigned char *data, unsigned int len) // uds发送接口
{
    app_select_client_tcp_server_send((char *)data, len);
    return 0;
}

static int uds_recv_cb(char *data, unsigned int uart_read_len) // uds接受回调函数，初始化时注册
{
    static unsigned char uart_read_buf[1024];
    static int uart_read_buf_index = 0;

    if (data == NULL || uart_read_len <= 0)
        return -1;
    dzlog_warn("recv from comm board-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);

    if (uart_read_buf_index > 0)
    {
        memcpy(&uart_read_buf[uart_read_buf_index], data, uart_read_len);
        uart_read_buf_index += uart_read_len;
        hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg);
    }
    else
    {
        uart_read_buf_index += uart_read_len;
        hdzlog_info(data, uart_read_buf_index);
        uart_parse_msg((unsigned char *)data, &uart_read_buf_index, ecb_uart_parse_msg);
    }
    dzlog_warn("after uart_read_buf_index:%d", uart_read_buf_index);
    if (uart_read_buf_index > 0)
    {
        memcpy(uart_read_buf, data, uart_read_buf_index);
        //  hdzlog_info(uart_read_buf, uart_read_buf_index);
    }

    return 0;
}

int uds_protocol_init(void) // uds协议相关初始化
{
    pthread_mutex_init(&mutex, NULL);
    register_uds_recv_cb(uds_recv_cb);
    return 0;
}
void uds_protocol_deinit(void) // uds协议相关反初始化
{
    uds_tcp_server_task_deinit();
    pthread_mutex_destroy(&mutex);
}
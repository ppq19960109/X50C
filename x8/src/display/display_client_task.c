#include "main.h"
#ifdef ICE_ENABLE
#include "thread2Client.h"
#include "ecb_uart_parse_msg.h"

#define UNIX_DOMAIN "/tmp/display.domain"
static ThreadTcp threadTcp;
int display_send(unsigned char *data, unsigned int len)
{
    // LOGW("display_send--------------------------len:%d", len);
    // hdzlog_info(data, len);
    return thread2ClientSend(&threadTcp, data, len);
}
static int display_recv(char *data, unsigned int uart_read_len)
{
    // thread2ClientSend(&threadTcp, data, uart_read_len);
    static unsigned char uart_read_buf[1024];
    static int uart_read_buf_index = 0;

    if (data == NULL || uart_read_len <= 0)
        return -1;
    // LOGW("recv from ecb-------------------------- uart_read_len:%d uart_read_buf_index:%d", uart_read_len, uart_read_buf_index);

    if (uart_read_buf_index > 0)
    {
        memcpy(&uart_read_buf[uart_read_buf_index], data, uart_read_len);
        // hdzlog_info(uart_read_buf, uart_read_buf_index);
        uart_read_buf_index += uart_read_len;
        uart_parse_msg(uart_read_buf, &uart_read_buf_index, ecb_uart_parse_msg);
    }
    else
    {
        uart_read_buf_index += uart_read_len;
        uart_parse_msg((unsigned char *)data, &uart_read_buf_index, ecb_uart_parse_msg);
    }
    // LOGW("after uart_read_buf_index:%d", uart_read_buf_index);
    if (uart_read_buf_index > 0)
    {
        memcpy(uart_read_buf, data, uart_read_buf_index);
        //  hdzlog_info(uart_read_buf, uart_read_buf_index);
    }
    return 0;
}
static int display_disconnect(void)
{
    printf("---disconnect...\n");
    return 0;
}
static int display_connect(void)
{
    printf("---connect...\n");
    ecb_uart_msg_get(true);
    return 0;
}
void display_client_close(void)
{
    thread2ClientClose(&threadTcp);
}
void *display_client_task(void *arg)
{
    printf("%s UNIX_DOMAIN:%s\r\n", __func__, UNIX_DOMAIN);

    struct sockaddr_un server_addr = {0};
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, UNIX_DOMAIN);

    tcpEvent2Set(&threadTcp, (struct sockaddr *)&server_addr, AF_UNIX, display_recv, display_disconnect, display_connect, 0);
    thread2ClientOpen(&threadTcp);
    return 0;
}
#endif
#ifndef _UART_TASK_H_
#define _UART_TASK_H_


#define UART_BUF_SIZE (256)

typedef enum
{
    ECB_UART_SEND_DATA_NO_REPLY = 0, /* 不需要应答的数据发送 */
    ECB_UART_SEND_DATA_WAIT_REPLY,   /* 等待应答的数据发送 */
} ecb_uart_msg_type_t;

typedef struct
{
    unsigned char *send_data; /* 发送数据指针 */
    unsigned int send_len;    /* 发送长度 */
    int send_seq_id;
    ecb_uart_msg_type_t msg_type;
} ecb_uart_queue_msg_t;


int ecb_uart_queue_send(ecb_uart_queue_msg_t ecb_uart_queue_msg);
int ecb_uart_queue_read(ecb_uart_queue_msg_t *ecb_uart_queue_msg);

void esp_uart_init(uart_port_t uart_num, int baud_rate, int tx_io_num, int rx_io_num);
void app_ecb_task(void *arg);
int uart_send_data(uart_port_t uart_num, unsigned char *in, int in_len, int resend, int iscopy);
#endif
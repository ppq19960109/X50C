#ifndef _ECB_PARSE_H_
#define _ECB_PARSE_H_

#define ECB_MSG_MIN_LEN (11)

typedef enum
{
    ECB_UART_COMMAND_GET = 0x01, /* GET */
    ECB_UART_COMMAND_SET,     /* SET */
    ECB_UART_COMMAND_EVENT,
    ECB_UART_COMMAND_KEYPRESS,
    ECB_UART_COMMAND_STORE,
    ECB_UART_COMMAND_GETACK = 0X0d,
    ECB_UART_COMMAND_ACK,
    ECB_UART_COMMAND_NAK,
} ecb_uart_command_type_t;

typedef enum
{
    ECB_NAK_CHECKSUM = 0x01, /*  */
    ECB_NAK_TAILER,     /*  */
    ECB_NAK_COMMAND_VERSION,
    ECB_NAK_COMMAND_UNKNOWN,
    ECB_NAK_COMMAND_ATTR_MISMATCH,
    ECB_NAK_ATTR_UNKNOWN,
    ECB_NAK_ATTR_OVERRANGE,
    ECB_NAK_NO_ALLOW,
    ECB_NAK_RECV_ABNORMAL,
    ECB_NAK_UNDEFINED_ERROR,
} ecb_uart_nak_type_t;

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen);
int ecb_uart_send_msg(unsigned char command, unsigned short msg_len, unsigned char *msg);
void uart_read_parse(unsigned char *in, int *in_len);
#endif
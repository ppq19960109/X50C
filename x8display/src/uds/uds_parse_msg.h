#ifndef _UDS_UART_PARSE_H_
#define _UDS_UART_PARSE_H_

#define ECB_MSG_MIN_LEN (11)
#include <stdbool.h>
typedef enum
{
    ECB_UART_READ_VALID = 0,
    ECB_UART_READ_NO_HEADER,
    ECB_UART_READ_LEN_SMALL,
    ECB_UART_READ_LEN_ERR,
    ECB_UART_READ_CHECK_ERR,
    ECB_UART_READ_TAILER_ERR
} ecb_uart_read_status_t;

typedef enum
{
    ECB_UART_COMMAND_GET = 0x01, /* GET */
    ECB_UART_COMMAND_SET,        /* SET */
    ECB_UART_COMMAND_EVENT,
    ECB_UART_COMMAND_KEYPRESS,
    ECB_UART_COMMAND_STORE,
    ECB_UART_COMMAND_OTA = 0X0A,
    ECB_UART_COMMAND_OTAACK,
    ECB_UART_COMMAND_HEART = 0X0C,
    ECB_UART_COMMAND_GETACK,
    ECB_UART_COMMAND_ACK,
    ECB_UART_COMMAND_NAK,
} ecb_uart_command_type_t;

typedef enum
{
    ECB_NAK_CHECKSUM = 0x01, /*  */
    ECB_NAK_TAILER,          /*  */
    ECB_NAK_COMMAND_VERSION,
    ECB_NAK_COMMAND_UNKNOWN,
    ECB_NAK_COMMAND_ATTR_MISMATCH,
    ECB_NAK_ATTR_UNKNOWN,
    ECB_NAK_ATTR_OVERRANGE,
    ECB_NAK_NO_ALLOW,
    ECB_NAK_RECV_ABNORMAL,
    ECB_NAK_UNDEFINED_ERROR,
} ecb_uart_nak_type_t;

enum ecb_uart_status_t
{
    ECB_UART_CONNECTED = 0,
    ECB_UART_CONNECTINT,
    ECB_UART_DISCONNECT,

    ECB_UART_DISCONNECT_COUNT = 4,
};

int uds_uart_parse_msg(const unsigned char *in, const int in_len, int *end);
void uart_parse_msg(unsigned char *in, int *in_len, int(func)(const unsigned char *, const int, int *));
int uds_uart_send_event_msg(unsigned char *msg, const int msg_len);

unsigned short crc16_maxim_single(const unsigned char *ptr, int len);
#endif
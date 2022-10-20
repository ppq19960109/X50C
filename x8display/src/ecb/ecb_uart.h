#ifndef _ECB_UART_H_
#define _ECB_UART_H_

enum msg_get_time_t
{
    MSG_GET_SHORT_TIME = 3 * 9,
    MSG_GET_LONG_TIME = 4 * 30 * 8,
    MSG_HEART_TIME = 15 * 9,
};

// unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen);

void ecb_uart_init(void);
void ecb_uart_deinit(void);

int ecb_uart_send(const unsigned char *in, int in_len);

#endif
#ifndef _ECB_UART_H_
#define _ECB_UART_H_

// unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen);

void ecb_uart_init(void);
void ecb_uart_deinit(void);

int ecb_uart_send(const unsigned char *in, int in_len);

#endif
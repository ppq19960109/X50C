#ifndef _ICE_UART_H_
#define _ICE_UART_H_

void ice_uart_init(void);
void ice_uart_deinit(void);

int ice_uart_send(const unsigned char *in, int in_len);

#endif
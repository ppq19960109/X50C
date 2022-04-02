#ifndef _ECB_UART_H_
#define _ECB_UART_H_

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen);

void ecb_resend_list_add(void *resend);
void ecb_resend_list_del_by_id(const int resend_seq_id);

void ecb_uart_init(void);
void ecb_uart_deinit(void);

int ecb_uart_send(const unsigned char *in, int in_len, unsigned char resend, unsigned char iscopy);
int ecb_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id);
#endif
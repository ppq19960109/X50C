#ifndef _ECB_UART_H_
#define _ECB_UART_H_

enum msg_get_time_t
{
    MSG_GET_SHORT_TIME = 3 * 7,
    MSG_GET_LONG_TIME = 3 * 30 * 6,
    MSG_HEART_TIME = 15 * 7,
};

unsigned short CRC16_MAXIM(const unsigned char *data, unsigned int datalen);

void ecb_resend_list_add(void *resend);
void ecb_resend_list_del_by_id(const int resend_seq_id);

void ecb_uart_init(void);
void ecb_uart_deinit(void);

int ecb_uart_send(const unsigned char *in, int in_len, unsigned char resend, unsigned char iscopy);
int ecb_uart_send_msg(const unsigned char command, unsigned char *msg, const int msg_len, unsigned char resend, int seq_id);
#endif
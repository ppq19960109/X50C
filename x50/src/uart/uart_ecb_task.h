#ifndef _UART_ECB_TASK_H_
#define _UART_ECB_TASK_H_

void ecb_resend_list_add(void *resend);
void ecb_resend_list_del_by_id(const int resend_seq_id);

void * uart_ecb_task(void *arg);
int uart_send_ecb(unsigned char *in, int in_len,unsigned char resend,unsigned char iscopy);
#endif
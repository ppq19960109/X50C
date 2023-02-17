#ifndef _UART_RESEND_H_
#define _UART_RESEND_H_

#include <stdbool.h>
#include "list.h"

// #define OTA_RESEND
#define RESEND_CNT (2)
extern unsigned short RESEND_WAIT_TICK;

typedef struct
{
    unsigned char resend_cnt; /* 等待的时序号 */
    unsigned char *send_data; /* 发送数据指针 */
    int resend_seq_id;

    unsigned int send_len;   /* 发送长度 */
    unsigned long wait_tick; /* 等待结束tick */
    int (*resend_cb)(const unsigned char *, int);
    struct list_head node;
} uart_resend_t;

unsigned long get_systime_ms(void);
unsigned long resend_tick_set(unsigned long current_tick, unsigned long set_tick);
bool resend_tick_compare(unsigned long current_tick, unsigned long compare_tick);

void resend_list_init(struct list_head *head);
void resend_list_add(struct list_head *head, struct list_head *node);
int resend_list_del_by_id(struct list_head *head, const int resend_seq_id);
void resend_list_each(struct list_head *head);
void resend_list_clear(struct list_head *head);
void set_resend_wait_tick(short tick);
#endif
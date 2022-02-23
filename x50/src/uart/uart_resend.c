#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "uart_resend.h"
#include "ecb_uart.h"

unsigned long get_systime_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

unsigned long resend_tick_set(unsigned long current_tick, unsigned long set_tick)
{
    return current_tick + set_tick;
}

bool resend_tick_compare(unsigned long current_tick, unsigned long compare_tick)
{
    if (current_tick >= compare_tick)
    {
        return true;
    }
    return false;
}

void resend_list_init(struct list_head *head)
{
    if (head == NULL)
    {
        return;
    }
    INIT_LIST_HEAD(head);
}

void resend_list_add(struct list_head *head, struct list_head *node)
{
    if (head == NULL)
    {
        return;
    }
    list_add(node, head);
}

void resend_list_del(uart_resend_t *ptr)
{
    list_del(&ptr->node);
    if (ptr->send_data != NULL)
    {
        free(ptr->send_data);
    }
    free(ptr);
    ptr = NULL;
}

uart_resend_t *resend_list_get_by_id(struct list_head *head, const int resend_seq_id)
{
    if (head == NULL)
    {
        return NULL;
    }
    uart_resend_t *ptr = NULL;

    list_for_each_entry(ptr, head, node)
    {
        if (ptr->resend_seq_id == resend_seq_id)
        {
            return ptr;
        }
    }
    return NULL;
}

int resend_list_del_by_id(struct list_head *head, const int resend_seq_id)
{
    uart_resend_t *ptr = resend_list_get_by_id(head, resend_seq_id);
    if (ptr == NULL)
    {
        return -1;
    }
    resend_list_del(ptr);
    return 0;
}

void resend_list_each(struct list_head *head)
{
    uart_resend_t *ptr, *next;
    if (head == NULL)
    {
        return;
    }
    unsigned long current_tick = get_systime_ms();
    list_for_each_entry_safe(ptr, next, head, node)
    {
        if (ptr->resend_cnt)
        {
            if (resend_tick_compare(current_tick, ptr->wait_tick))
            {
                ptr->wait_tick = resend_tick_set(current_tick, RESEND_WAIT_TICK);
                --ptr->resend_cnt;
                
                if (ptr->resend_cb != NULL)
                    ptr->resend_cb(ptr->send_data, ptr->send_len);
            }
        }
        if (ptr->resend_cnt == 0)
        {
            resend_list_del(ptr);
        }
    }
}

#ifndef _UART_TASK_H_
#define _UART_TASK_H_

#include "uds_tcp_server.h"

void *uart_task(void *arg);
void uart_task_init(void);
void uart_task_deinit(void);
int add_select_client_uart(struct Select_Client_Event *select_client_event);
void register_select_uart_timeout_cb(int (*cb)());
#endif
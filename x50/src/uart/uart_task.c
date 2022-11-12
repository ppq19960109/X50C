#include "main.h"
#include "uart_task.h"
#include "ecb_uart.h"
#include "gesture_uart.h"
#include "pangu_uart.h"

static struct Select_Server_Event select_server_event;

int (*select_uart_timeout_cb)();
void register_select_uart_timeout_cb(int (*cb)())
{
    select_uart_timeout_cb = cb;
}

int add_select_client_uart(struct Select_Client_Event *select_client_event)
{
    return add_select_client_event(&select_server_event, select_client_event);
}
/*********************************************************************************
 *Function:  uart_task
 *Description： uart任务函数
 *Input:
 *Return:
 **********************************************************************************/
void *uart_task(void *arg)
{
    if (select_uart_timeout_cb != NULL)
        select_server_event.timeout_cb = select_uart_timeout_cb;
    select_server_task(&select_server_event, 150);

    ecb_uart_deinit();
    pangu_uart_deinit();
    // gesture_uart_deinit();
    return NULL;
}
void uart_task_init(void)
{
    select_server_init(&select_server_event);
    ecb_uart_init();
    pangu_uart_init();
    // gesture_uart_init();
}
void uart_task_deinit(void)
{
    select_server_deinit(&select_server_event);
}
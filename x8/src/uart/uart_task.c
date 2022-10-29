#include "main.h"
#include "uart_task.h"
#include "ecb_uart.h"
#include "cook_assist.h"

static struct Select_Server_Event select_server_event;

static int select_uart_timeout_cb(void *arg)
{
    ergodic_select_client_timeout(&select_server_event);
    return 0;
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
    select_server_event.timeout_cb = select_uart_timeout_cb;
    select_server_task(&select_server_event, 120);

    ecb_uart_deinit();
    cook_assist_deinit();
    return NULL;
}
void uart_task_init(void)
{
    select_server_init(&select_server_event);
    ecb_uart_init();
    cook_assist_init();
}
void uart_task_deinit(void)
{
    select_server_deinit(&select_server_event);
}
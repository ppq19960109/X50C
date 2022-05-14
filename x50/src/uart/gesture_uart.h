#ifndef _GESTURE_UART_H_
#define _GESTURE_UART_H_
#include "cloud_platform_task.h"
void gesture_uart_init(void);
void gesture_uart_deinit(void);
int gesture_uart_send_cloud_msg(unsigned char *msg, const int msg_len);
int gesture_uart_msg_get();
cloud_dev_t *get_gesture_dev();
#endif
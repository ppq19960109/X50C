#ifndef _UART_GESTURE_TASK_H_
#define _UART_GESTURE_TASK_H_

void * uart_gesture_task(void *arg);
int uart_send_gesture(unsigned char *in, int in_len);
int send_gesture_msg(unsigned char sync, unsigned char hour, unsigned char minute);
void gesture_time_sync_task(void);
#endif
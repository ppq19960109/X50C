#ifndef _GESTURE_UART_H_
#define _GESTURE_UART_H_

void gesture_uart_init(void);
void gesture_uart_deinit(void);
void gesture_auto_sync_time_alarm(int alarm);
void set_gesture_power(const int power);
#endif
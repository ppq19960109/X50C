#ifndef _PANGU_UART_H_
#define _PANGU_UART_H_

typedef struct
{
    char key[28];
    unsigned char change;
    unsigned char value_len;
    char *value;
} pangu_attr_t;

void pangu_uart_init(void);
void pangu_uart_deinit(void);
int pangu_recv_set(void *data);

#endif
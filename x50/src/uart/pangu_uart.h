#ifndef _PANGU_UART_H_
#define _PANGU_UART_H_

typedef struct
{
    char key[28];
    unsigned char change;
    unsigned char value_len;
    char *value;
} pangu_attr_t;

typedef struct
{
    unsigned char mode;
    unsigned char fire;
    unsigned short temp;
    unsigned short time;
    unsigned char motorSpeed;
    unsigned char motorDir;
    unsigned char waterTime;
    unsigned char fan;
    unsigned char hoodSpeed;
    unsigned char repeat;
} pangu_cook_attr_t;

typedef struct
{
    pangu_cook_attr_t cook_attr[30];
    unsigned char total_step;
    unsigned char current_step;
} pangu_cook_t;

void pangu_uart_init(void);
void pangu_uart_deinit(void);
int pangu_recv_set(void *data);
int pangu_state_event(unsigned char cmd);
#endif
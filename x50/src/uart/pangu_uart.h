#ifndef _PANGU_UART_H_
#define _PANGU_UART_H_
#include "cloud_platform_task.h"
enum work_operation
{
    WORK_OPERATION_RUN,
    WORK_OPERATION_PAUSE,
    WORK_OPERATION_STOP,
    WORK_OPERATION_FINISH,
    WORK_OPERATION_RUN_NOW,
};
enum report_work_state
{
    REPORT_WORK_STATE_STOP,
    REPORT_WORK_STATE_RESERVE,
    REPORT_WORK_STATE_PREHEAT,
    REPORT_WORK_STATE_RUN,
    REPORT_WORK_STATE_FINISH,
    REPORT_WORK_STATE_PAUSE,
    REPORT_WORK_STATE_RESERVE_PAUSE,
    REPORT_WORK_STATE_WATER,
    REPORT_WORK_STATE_CLEAN,
};

typedef struct
{
    char key[28];
    enum LINK_VALUE_TYPE value_type;
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
    unsigned short waterTime;
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
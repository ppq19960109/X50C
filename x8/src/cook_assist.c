#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "POSIXTimer.h"
#include "mlog.h"
#include "cook_assist.h"

#include "main.h"
#include "KV_linux.h"
#include "cloud_platform_task.h"
#include "uart_task.h"
#include "ecb_uart_parse_msg.h"
typedef struct
{
    char OilTempSwitch;
    char CookingCurveSwitch;
    char RMovePotLowHeatSwitch;
    char RAuxiliarySwitch;
    unsigned short RAuxiliaryTemp;
} cook_assist_t;
static int fd = -1;
static struct Select_Client_Event select_client_event;
static unsigned short left_temp = 0;
static unsigned short left_environment_temp = 0;
static unsigned short right_temp = 0;
static unsigned short right_environment_temp = 0;

//-----------------------------------------------
static char resp_all_flag = 0;
static cook_assist_t g_cook_assist;
static cJSON *resp = NULL;
static timer_t cook_assist_timer;

void cook_assist_set_smartSmoke(const char status)
{
    set_work_mode(status);
}

int cook_assist_recv_property_set(const char *key, cJSON *value)
{
    if (strcmp("RAuxiliarySwitch", key) == 0)
    {
        g_cook_assist.RAuxiliarySwitch = value->valueint;
        set_temp_control_switch(value->valueint, INPUT_RIGHT);
    }
    else if (strcmp("RAuxiliaryTemp", key) == 0)
    {
        g_cook_assist.RAuxiliaryTemp = value->valueint;
        set_temp_control_target_temp(value->valueint, INPUT_RIGHT);
    }
    else if (strcmp("CookingCurveSwitch", key) == 0)
    {
        g_cook_assist.CookingCurveSwitch = value->valueint;
    }
    else if (strcmp("OilTempSwitch", key) == 0)
    {
        g_cook_assist.OilTempSwitch = value->valueint;
    }
    else if (strcmp("RMovePotLowHeatSwitch", key) == 0)
    {
        g_cook_assist.RMovePotLowHeatSwitch = value->valueint;
        set_pan_fire_switch(value->valueint, INPUT_RIGHT);
    }
    else
    {
        return -1;
    }
    if (resp == NULL)
        resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, key, value->valueint);
    return 0;
}
static void oil_temp_cb(const unsigned short temp, enum INPUT_DIR input_dir)
{
    static unsigned char report_temp_count = 0;
    static unsigned short left_oil_temp = 0;
    static unsigned short right_oil_temp = 0;

    if (g_cook_assist.OilTempSwitch == 0)
        return;
    if (INPUT_LEFT == input_dir)
    {
        left_oil_temp = temp;
    }
    else
    {
        right_oil_temp = temp;
    }
    if (++report_temp_count > 8)
    {
        report_temp_count = 0;
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "LOilTemp", left_oil_temp);
        cJSON_AddNumberToObject(root, "ROilTemp", right_oil_temp);
        report_msg_all_platform(root);
    }
}
static int pan_fire_switch_cb(const unsigned char status, enum INPUT_DIR input_dir)
{
    return 0;
}
static int dry_switch_cb(const unsigned char status, enum INPUT_DIR input_dir)
{
    return 0;
}
static int temp_control_cb(const unsigned char status, enum INPUT_DIR input_dir)
{
    return 0;
}
static int temp_control_target_temp_cb(const unsigned short temp, enum INPUT_DIR input_dir)
{
    return 0;
}
void cook_assist_report_all(cJSON *root)
{
    cJSON_AddNumberToObject(root, "RAuxiliarySwitch", g_cook_assist.RAuxiliarySwitch);
    cJSON_AddNumberToObject(root, "RAuxiliaryTemp", g_cook_assist.RAuxiliaryTemp);
    cJSON_AddNumberToObject(root, "CookingCurveSwitch", g_cook_assist.CookingCurveSwitch);
    cJSON_AddNumberToObject(root, "OilTempSwitch", g_cook_assist.OilTempSwitch);
    cJSON_AddNumberToObject(root, "RMovePotLowHeatSwitch", g_cook_assist.RMovePotLowHeatSwitch);

    if (resp != NULL)
        cJSON_Delete(resp);
    resp = NULL;
}
static void POSIXTimer_cb(union sigval val)
{
    if (val.sival_int == 10)
    {
        if (resp_all_flag == 0)
        {
            if (resp == NULL)
                return;
            report_msg_all_platform(resp);
        }
        else
        {
            cJSON *root = cJSON_CreateObject();
            cook_assist_report_all(root);
            report_msg_all_platform(root);
        }
        resp = NULL;
    }
}

void cook_assist_start_single_recv()
{
    if (resp == NULL)
        return;
    POSIXTimerSet(cook_assist_timer, 0, 0);
    resp_all_flag = 1;
}
void cook_assist_end_single_recv()
{
    if (resp == NULL && resp_all_flag == 0)
        return;
    POSIXTimerSet(cook_assist_timer, 0, 1);
}
//-----------------------------------------------------------------
static int cook_assistant_hood_speed_cb(const int gear)
{
    unsigned char uart_buf[2];
    uart_buf[0] = 0x31;
    uart_buf[1] = gear;
    ecb_uart_send_cloud_msg(uart_buf, sizeof(uart_buf));
    return 0;
}
static int cook_assistant_fire_cb(const int gear, enum INPUT_DIR input_dir)
{
    unsigned char uart_buf[2];
    if (input_dir == INPUT_RIGHT)
    {
        uart_buf[0] = 0x20;
        uart_buf[1] = gear;
        ecb_uart_send_cloud_msg(uart_buf, sizeof(uart_buf));
    }
    return 0;
}

static int cook_assist_recv_cb(void *arg)
{
    static unsigned char uart_read_buf[32];
    int uart_read_len;

    uart_read_len = read(fd, uart_read_buf, sizeof(uart_read_buf));
    if (uart_read_len > 0)
    {
        printf("recv from cook_assist-------------------------- uart_read_len:%d\n", uart_read_len);
        hdzlog_info(uart_read_buf, uart_read_len);

        if (uart_read_len < 15 || uart_read_buf[0] != 0x55 || uart_read_buf[1] != 0x2b)
            return -1;

        left_environment_temp = (uart_read_buf[2] << 8) + uart_read_buf[3];
        left_temp = (uart_read_buf[4] << 8) + uart_read_buf[5];
        if (left_temp > 380)
            left_temp = 380;

        right_environment_temp = (uart_read_buf[8] << 8) + uart_read_buf[9];
        right_temp = (uart_read_buf[10] << 8) + uart_read_buf[11];
        if (right_temp > 380)
            right_temp = 380;

        cook_assistant_input(INPUT_RIGHT, right_temp * 10, right_environment_temp * 10);
        cook_assistant_input(INPUT_LEFT, left_temp * 10, left_environment_temp * 10);
        prepare_gear_change_task();
    }
    return 0;
}

static int cook_assist_except_cb(void *arg)
{
    return 0;
}

void cook_assist_init()
{
    register_oil_temp_cb(oil_temp_cb);
    register_pan_fire_switch_cb(pan_fire_switch_cb);
    register_dry_switch_cb(dry_switch_cb);
    register_temp_control_switch_cb(temp_control_cb);
    register_temp_control_target_temp_cb(temp_control_target_temp_cb);

    register_hood_gear_cb(cook_assistant_hood_speed_cb);
    register_fire_gear_cb(cook_assistant_fire_cb);

    cook_assistant_init(INPUT_LEFT);
    cook_assistant_init(INPUT_RIGHT);
    fd = uart_init("/dev/ttyS0", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        dzlog_error("cook_assist uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("cook_assist,fd:%d", fd);

    select_client_event.fd = fd;
    select_client_event.read_cb = cook_assist_recv_cb;
    select_client_event.except_cb = cook_assist_except_cb;

    add_select_client_uart(&select_client_event);

    cook_assist_timer = POSIXTimerCreate(10, POSIXTimer_cb);
}
void cook_assist_deinit()
{
    if (cook_assist_timer != NULL)
    {
        POSIXTimerDelete(cook_assist_timer);
        cook_assist_timer = NULL;
    }
    close(fd);
}
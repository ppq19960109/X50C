#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mlog.h"
#include "cook_assist.h"

#include "main.h"
#include "cloud_platform_task.h"
#include "uart_task.h"
#include "ecb_uart_parse_msg.h"
#include "uds_protocol.h"
#include "curl/curl.h"
#include "curl_http_request.h"

typedef struct
{
    char workMode;
    char OilTempSwitch;
    char CookingCurveSwitch;
    char RMovePotLowHeatSwitch;
    char RAuxiliarySwitch;
    char SmartSmokeSwitch;
    char RStoveStatus;
    unsigned short RAuxiliaryTemp;
} cook_assist_t;
static int fd = -1;
static struct Select_Client_Event select_client_event;
static unsigned short left_temp = 0;
static unsigned short left_environment_temp = 0;
static unsigned short right_temp = 0;
static unsigned short right_environment_temp = 0;
static unsigned long curveKey = 0;
//-----------------------------------------------

static pthread_mutex_t lock;
static cook_assist_t g_cook_assist = {
    workMode : 0,
    OilTempSwitch : 0,
    CookingCurveSwitch : 0,
    RMovePotLowHeatSwitch : 0,
    RAuxiliarySwitch : 0,
    SmartSmokeSwitch : 0,
    RAuxiliaryTemp : 0
};
static cJSON *resp = NULL;

static void cook_assist_judge_work_mode()
{
    if (g_cook_assist.RMovePotLowHeatSwitch == 0 && g_cook_assist.RAuxiliarySwitch == 0 && g_cook_assist.SmartSmokeSwitch == 0)
    {
        g_cook_assist.workMode = 0;
        set_work_mode(0);
    }
    else
    {
        g_cook_assist.workMode = 1;
        set_work_mode(1);
    }
}

void cook_assist_set_smartSmoke(const char status)
{
    set_smart_smoke_switch(status);
}

static int cook_assist_recv_property_set(const char *key, cJSON *value)
{
    if (strcmp("RAuxiliarySwitch", key) == 0)
    {
        set_temp_control_switch(value->valueint, INPUT_RIGHT);
    }
    else if (strcmp("RAuxiliaryTemp", key) == 0)
    {
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
        set_pan_fire_switch(value->valueint, INPUT_RIGHT);
    }
    else
    {
        return -1;
    }
    if (resp == NULL)
        resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, key, value->valueint);
    cook_assist_judge_work_mode();
    return 0;
}
void set_stove_status(unsigned char status, enum INPUT_DIR input_dir)
{
    set_ignition_switch(status, input_dir);
    if (input_dir == INPUT_RIGHT)
    {
        if (g_cook_assist.RStoveStatus == 0 && status > 0)
        {
            ++curveKey;
        }
        g_cook_assist.RStoveStatus = status;
    }
}

static void cookingCurve_post(const unsigned short temp)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "deviceMac", cloud_dev->mac);
    cJSON_AddStringToObject(resp, "iotId", cloud_dev->device_name);
    cJSON_AddNumberToObject(resp, "Temp", temp);
    cJSON_AddNumberToObject(resp, "curveKey", curveKey);

    char *json = cJSON_PrintUnformatted(root);
    curl_http_post("http://mcook.dev.marssenger.net/menu-anon/", json);
    free(json);
    cJSON_Delete(root);
}
static void oil_temp_cb(const unsigned short temp, enum INPUT_DIR input_dir)
{
    static unsigned char report_temp_count = 15;
    static unsigned short left_oil_temp = 0;
    static unsigned short right_oil_temp = 0;

    if (g_cook_assist.OilTempSwitch == 0 && g_cook_assist.CookingCurveSwitch == 0)
        return;
    if (INPUT_LEFT == input_dir)
    {
        left_oil_temp = temp;
    }
    else
    {
        right_oil_temp = temp;
    }

    if (++report_temp_count >= 20 || report_temp_count == 10)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "LOilTemp", left_oil_temp / 10);
        cJSON_AddNumberToObject(root, "ROilTemp", right_oil_temp / 10);
        if (report_temp_count != 10)
        {
            report_temp_count = 0;
            report_msg_all_platform(root);
            if (g_cook_assist.RStoveStatus > 0 && g_cook_assist.CookingCurveSwitch > 0)
            {
                cookingCurve_post(right_oil_temp / 10);
            }
        }
        else
        {
            send_event_uds(root, NULL);
        }
    }
}
static int smart_smoke_switch_cb(const unsigned char status)
{
    g_cook_assist.SmartSmokeSwitch = status;
    cook_assist_judge_work_mode();
    return 0;
}
static int pan_fire_switch_cb(const unsigned char status, enum INPUT_DIR input_dir)
{
    if (input_dir == INPUT_RIGHT)
    {
        g_cook_assist.RMovePotLowHeatSwitch = status;
    }
    return 0;
}
static int temp_control_cb(const unsigned char status, enum INPUT_DIR input_dir)
{
    if (input_dir == INPUT_RIGHT)
    {
        g_cook_assist.RAuxiliarySwitch = status;
    }
    return 0;
}
static int temp_control_target_temp_cb(const unsigned short temp, enum INPUT_DIR input_dir)
{
    if (input_dir == INPUT_RIGHT)
    {
        g_cook_assist.RAuxiliaryTemp = temp;
    }
    return 0;
}
void cook_assist_report_all(cJSON *root)
{
    pthread_mutex_lock(&lock);
    cJSON_AddNumberToObject(root, "RAuxiliarySwitch", g_cook_assist.RAuxiliarySwitch);
    cJSON_AddNumberToObject(root, "RAuxiliaryTemp", g_cook_assist.RAuxiliaryTemp);
    cJSON_AddNumberToObject(root, "CookingCurveSwitch", g_cook_assist.CookingCurveSwitch);
    cJSON_AddNumberToObject(root, "OilTempSwitch", g_cook_assist.OilTempSwitch);
    cJSON_AddNumberToObject(root, "RMovePotLowHeatSwitch", g_cook_assist.RMovePotLowHeatSwitch);

    if (resp != NULL)
        cJSON_Delete(resp);
    resp = NULL;
    pthread_mutex_unlock(&lock);
}

int cook_assist_link_recv(const char *key, cJSON *value)
{
    int res = -1;
    pthread_mutex_lock(&lock);
    if (key == NULL || value == NULL)
    {
        if (resp != NULL)
        {
            report_msg_all_platform(resp);
            resp = NULL;
        }
    }
    else
    {
        res = cook_assist_recv_property_set(key, value);
    }
    pthread_mutex_unlock(&lock);
    return res;
}
//-----------------------------------------------------------------
static int cook_assistant_hood_speed_cb(const int gear)
{
    if (g_cook_assist.SmartSmokeSwitch == 0)
        return -1;
    unsigned char uart_buf[2];
    if (gear < 0)
    {
        uart_buf[0] = 0x35;
        uart_buf[1] = 0;
    }
    else
    {
        uart_buf[0] = 0x31;
        uart_buf[1] = gear;
    }
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
    cook_assist_link_recv(NULL, NULL);
    if (g_cook_assist.OilTempSwitch == 0 && g_cook_assist.CookingCurveSwitch == 0 && g_cook_assist.RMovePotLowHeatSwitch == 0 && g_cook_assist.RAuxiliarySwitch == 0 && g_cook_assist.SmartSmokeSwitch == 0)
        return -1;
    if (uart_read_len > 0)
    {
        // printf("recv from cook_assist-------------------------- uart_read_len:%d\n", uart_read_len);
        // hdzlog_info(uart_read_buf, uart_read_len);
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

static int cook_assist_remind_cb(int index)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "CookAssistRemind", index);
    send_event_uds(root, NULL);
    if (index == 0)
    {
    }
    else if (index == 1)
    {
        unsigned char uart_buf[2];
        uart_buf[0] = 0x1d;
        uart_buf[1] = 0x01;
        ecb_uart_send_cloud_msg(uart_buf, sizeof(uart_buf));
    }
    else
    {
    }
    return 0;
}

void cook_assist_init()
{
    pthread_mutex_init(&lock, NULL);
    register_oil_temp_cb(oil_temp_cb);
    register_pan_fire_switch_cb(pan_fire_switch_cb);
    register_smart_smoke_switch_cb(smart_smoke_switch_cb);
    register_temp_control_switch_cb(temp_control_cb);
    register_temp_control_target_temp_cb(temp_control_target_temp_cb);

    register_cook_assist_remind_cb(cook_assist_remind_cb);

    register_hood_gear_cb(cook_assistant_hood_speed_cb);
    register_fire_gear_cb(cook_assistant_fire_cb);

    fd = uart_init("/dev/ttyS0", BAUDRATE_9600, DATABIT_8, PARITY_NONE, STOPBIT_1, FLOWCTRL_NONE, BLOCKING_NONBLOCK);
    if (fd < 0)
    {
        dzlog_error("cook_assist uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    dzlog_info("cook_assist,fd:%d", fd);
    dzlog_warn("cook_assist init,OilTempSwitch:%d", g_cook_assist.OilTempSwitch);
    cook_assistant_init(INPUT_LEFT);
    cook_assistant_init(INPUT_RIGHT);
    cook_assist_judge_work_mode();

    select_client_event.fd = fd;
    select_client_event.read_cb = cook_assist_recv_cb;
    select_client_event.except_cb = cook_assist_except_cb;

    add_select_client_uart(&select_client_event);

    // cook_assist_set_smartSmoke(1);
    // cook_assistant_hood_speed_cb(0,INPUT_RIGHT);
}
void cook_assist_deinit()
{
    close(fd);
    pthread_mutex_destroy(&lock);
}
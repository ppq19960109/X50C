#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mlog.h"
#include "cook_assist.h"

#include "main.h"
#include "cloud_platform_task.h"
#include "ecb_uart_parse_msg.h"
#include "uds_protocol.h"
#include "curl/curl.h"
#include "curl_http_request.h"
#include "md5_func.h"

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
    unsigned short RMovePotLowHeatOffTime;
    char version[8];
} cook_assist_t;
static hio_t *mio;
static int fd = -1;

static unsigned short left_temp = 0;
static unsigned short left_environment_temp = 0;
static unsigned short right_temp = 0;
static unsigned short right_environment_temp = 0;
static int curveKey = 0;
static unsigned char recv_timeout_count = 0;
static unsigned char temp_uart_switch = 0;
//-----------------------------------------------

static pthread_mutex_t lock;
static cook_assist_t g_cook_assist = {
    workMode : 0,
    OilTempSwitch : 1,
    CookingCurveSwitch : 0,
    RMovePotLowHeatSwitch : 0,
    RAuxiliarySwitch : 0,
    SmartSmokeSwitch : 0,
    RAuxiliaryTemp : 0,
    RMovePotLowHeatOffTime : 180
};
static cJSON *resp = NULL;

static int cook_assist_uart_send(const unsigned char *in, int in_len)
{
    if (mio == NULL)
    {
        LOGW("%s,mio NULL", __func__);
        return -1;
    }
    if (in == NULL || in_len <= 0)
    {
        return -1;
    }
    LOGW("cook_assist_uart_send--------------------------");
    mlogHex(in, in_len);
    return hio_write(mio, in, in_len);
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
static void cook_assist_uart_switch(const int state)
{
    static unsigned char open_uart_data[] = {0xAA, 0X01, 0X01, 0X00, 0XAC};
    static unsigned char close_uart_data[] = {0xAA, 0X01, 0X00, 0X00, 0XAB};
    static unsigned char version_uart_data[] = {0xAA, 0X01, 0X01, 0XFF, 0XAB, 0X96};
    if (state == 1)
        cook_assist_uart_send(open_uart_data, sizeof(open_uart_data));
    else if (state == 2)
        cook_assist_uart_send(version_uart_data, sizeof(version_uart_data));
    else
        cook_assist_uart_send(close_uart_data, sizeof(close_uart_data));
}
static void cook_assist_judge_work_mode()
{
    if (g_cook_assist.RMovePotLowHeatSwitch == 0 && g_cook_assist.RAuxiliarySwitch == 0 && g_cook_assist.SmartSmokeSwitch == 0)
    {
        if (g_cook_assist.workMode != 0)
        {
            g_cook_assist.workMode = 0;
            set_work_mode(0);
        }
    }
    else
    {
        if (g_cook_assist.workMode == 0)
        {
            g_cook_assist.workMode = 1;
            set_work_mode(1);
        }
    }
    if (g_cook_assist.CookingCurveSwitch == 0 && g_cook_assist.OilTempSwitch == 0 && g_cook_assist.workMode == 0)
    {
        if (temp_uart_switch != 0)
        {
            temp_uart_switch = 0;
            cook_assist_uart_switch(0);
        }
    }
    else
    {
        if (temp_uart_switch == 0)
        {
            temp_uart_switch = 1;
            cook_assist_uart_switch(1);
        }
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
    else if (strcmp("RMovePotLowHeatOffTime", key) == 0)
    {
        g_cook_assist.RMovePotLowHeatOffTime = value->valueint;
        set_pan_fire_close_delay_tick(value->valueint, INPUT_RIGHT);
    }
    else
    {
        if (strcmp("HoodSpeed", key) == 0)
        {
            cook_assistant_hood_speed_cb(-1);
            cook_assist_remind_cb(2);
        }
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
            srand(time(NULL));
            curveKey = rand();
        }
        g_cook_assist.RStoveStatus = status;
    }
}

static void cookingCurve_post(const signed short *temp, const unsigned char len)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceMac", cloud_dev->mac);
    cJSON_AddStringToObject(root, "iotId", cloud_dev->mac);
    cJSON_AddNumberToObject(root, "Temp", 0);
    cJSON_AddNumberToObject(root, "curveKey", curveKey);
    cJSON_AddStringToObject(root, "token", cloud_dev->token);

    time_t now_time = time(NULL);
    cJSON *cookCurveTempDTOS = cJSON_AddArrayToObject(root, "cookCurveTempDTOS");
    for (int i = 0; i < len; ++i)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "temp", temp[i]);
        cJSON_AddNumberToObject(item, "time", now_time - (len - i + 1) / 2);
        cJSON_AddItemToArray(cookCurveTempDTOS, item);
    }
    char *json = cJSON_PrintUnformatted(root);
    curl_http_post("http://mcook.marssenger.com/menu-anon/addCookCurve", json);
    free(json);
    cJSON_Delete(root);
}
static void oil_temp_cb(const unsigned short temp, enum INPUT_DIR input_dir)
{
    static signed short right_oil_curve[20];
    static unsigned char right_oil_curve_len;

    static unsigned char report_temp_count = 8;
    static unsigned short left_oil_temp = 0;
    static unsigned short right_oil_temp = 0;

    if (g_cook_assist.OilTempSwitch == 0 && g_cook_assist.CookingCurveSwitch == 0)
    {
        report_temp_count = 8;
        right_oil_curve_len = 0;
        return;
    }

    if (INPUT_LEFT == input_dir)
    {
        left_oil_temp = temp;
    }
    else
    {
        right_oil_temp = temp;
    }

    if (++report_temp_count >= 10 || report_temp_count == 5)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "LOilTemp", left_oil_temp / 10);
        cJSON_AddNumberToObject(root, "ROilTemp", right_oil_temp / 10);
        if (report_temp_count != 5)
        {
            report_temp_count = 0;
            report_msg_all_platform(root);
        }
        else
        {
            send_event_uds(root, NULL);
        }
    }
    if (report_temp_count % 2 == 0)
    {
        if (g_cook_assist.RStoveStatus > 0 && g_cook_assist.CookingCurveSwitch > 0)
        {
            right_oil_curve[right_oil_curve_len++] = right_oil_temp / 10;
            if (right_oil_curve_len >= 20)
            {
                cookingCurve_post(right_oil_curve, right_oil_curve_len);
                right_oil_curve_len = 0;
            }
        }
        else
        {
            right_oil_curve_len = 0;
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
    cJSON_AddNumberToObject(root, "RMovePotLowHeatOffTime", g_cook_assist.RMovePotLowHeatOffTime);
    cJSON_AddStringToObject(root, "CookAssistVersion", g_cook_assist.version);
    cook_assist_uart_switch(2);
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
            if (g_cook_assist.RAuxiliarySwitch > 0 && g_cook_assist.OilTempSwitch == 0)
            {
                g_cook_assist.OilTempSwitch = 1;
                cJSON_AddNumberToObject(resp, "OilTempSwitch", g_cook_assist.OilTempSwitch);
            }
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

static void on_uart_read(hio_t *io, void *buf, int readbytes)
{
    char *uart_read_buf = buf;
    int uart_read_len = readbytes;

    recv_timeout_count = 0;
    // printf("recv from cook_assist-------------------------- uart_read_len:%d\n", uart_read_len);
    // mlogHex(uart_read_buf, uart_read_len);
    if (uart_read_len > 0)
    {
        if (g_cook_assist.OilTempSwitch == 0 && g_cook_assist.CookingCurveSwitch == 0 && g_cook_assist.RMovePotLowHeatSwitch == 0 && g_cook_assist.RAuxiliarySwitch == 0 && g_cook_assist.SmartSmokeSwitch == 0)
            goto fail;

        if (uart_read_len < 15 || uart_read_buf[0] != 0x55 || uart_read_buf[1] != 0x2b)
            goto fail;

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
        if (uart_read_len == 15)
            return;
    fail:
        if (uart_read_len >= 8)
        {
            if (uart_read_buf[0] == 0xAA && uart_read_buf[7] == 0x96)
                sprintf(g_cook_assist.version, "%d.%d.%d", uart_read_buf[3], uart_read_buf[4], uart_read_buf[5]);
            else if (uart_read_len == 23 && uart_read_buf[15 + 0] == 0xAA && uart_read_buf[15 + 7] == 0x96)
                sprintf(g_cook_assist.version, "%d.%d.%d", uart_read_buf[15 + 3], uart_read_buf[15 + 4], uart_read_buf[15 + 5]);
            else
            {
                LOGE("%s,cook assist version error", __func__);
                return;
            }
            LOGW("%s,cook assist version:%s", __func__, g_cook_assist.version);
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "CookAssistVersion", g_cook_assist.version);
            send_event_uds(root, NULL);
        }
    }
}
static void on_idle(hidle_t *idle)
{
    cook_assist_link_recv(NULL, NULL);
    if (recv_timeout_count < 6)
    {
        ++recv_timeout_count;
        if (recv_timeout_count >= 6)
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "LOilTemp", -1);
            cJSON_AddNumberToObject(root, "ROilTemp", -1);
            report_msg_all_platform(root);
        }
    }
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
        LOGE("cook_assist uart init error:%d,%s", errno, strerror(errno));
        return;
    }
    LOGI("cook_assist,fd:%d", fd);
    LOGW("cook_assist init,OilTempSwitch:%d", g_cook_assist.OilTempSwitch);
    cook_assistant_init(INPUT_LEFT);
    cook_assistant_init(INPUT_RIGHT);

    hidle_add(g_loop, on_idle, INFINITE);
    static char uart_buf[36];
    mio = hread(g_loop, fd, uart_buf, sizeof(uart_buf), on_uart_read);

    // cook_assist_set_smartSmoke(1);
    // cook_assistant_hood_speed_cb(0,INPUT_RIGHT);
    cook_assist_judge_work_mode();
    cook_assist_uart_switch(2);
}
void cook_assist_deinit()
{
    close(fd);
    pthread_mutex_destroy(&lock);
}
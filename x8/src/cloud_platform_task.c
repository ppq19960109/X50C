#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"
#include "rkwifi.h"
#include "ecb_uart_parse_msg.h"
#include "link_solo.h"
#include "link_dynregmq_posix.h"
#include "link_fota_posix.h"
#include "link_fota_power_posix.h"
#include "link_ntp_posix.h"
#include "cloud_platform_task.h"
#include "device_task.h"
#include "POSIXTimer.h"
#include "quad_burn.h"
#include "cook_assist.h"
#include "curl_http_request.h"

static timer_t cook_name_timer;
static pthread_mutex_t mutex;
static cloud_dev_t *g_cloud_dev = NULL;
static char first_uds_report = 0;
static char demo_mode = 0;

void uds_report_reset(void)
{
    dzlog_warn("%s", __func__);
    first_uds_report = 0;
    ecb_uart_msg_get(true);
}

int cJSON_Object_isNull(cJSON *object) // cJSON判断Object是否为空
{
    char *json = cJSON_PrintUnformatted(object);
    if (strlen(json) == 2 && strcmp(json, "{}") == 0)
    {
        cJSON_free(json);
        return 1;
    }
    cJSON_free(json);
    return 0;
}

int report_msg_all_platform(cJSON *root)
{
    if (cJSON_Object_isNull(root))
    {
        cJSON_Delete(root);
        dzlog_warn("%s,send NULL", __func__);
        return -1;
    }
    char *json = cJSON_PrintUnformatted(root);
    link_send_property_post(json);
    cJSON_free(json);
    send_event_uds(root, NULL);
    return 0;
}

int report_msg_quad_uds(const char *msg)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "QuadInfo", msg);
    send_event_uds(resp, NULL);
    return 0;
}

cloud_dev_t *get_cloud_dev(void)
{
    return g_cloud_dev;
}

cloud_attr_t *get_attr_ptr(const char *name)
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (strcmp(name, attr[i].cloud_key) == 0)
        {
            return &attr[i];
        }
    }
    return NULL;
}

cloud_attr_t *get_attr_ptr_from_uartCmd(signed short cmd)
{
    if (cmd <= 0)
        return NULL;
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (cmd == attr[i].uart_cmd)
        {
            return &attr[i];
        }
    }
    return NULL;
}

unsigned char get_ErrorCodeShow(void)
{
    cloud_attr_t *attr = get_attr_ptr("ErrorCodeShow");
    if (attr == NULL)
    {
        return -1;
    }
    return *(attr->value);
}
unsigned int get_ErrorCode(void)
{
    cloud_attr_t *attr = get_attr_ptr("ErrorCode");
    if (attr == NULL)
    {
        return -1;
    }
    unsigned int error_code = 0;
    for (int i = 0; i < attr->uart_byte_len; ++i)
    {
        error_code = (error_code << 8) + attr->value[i];
    }
    return error_code;
}

signed char get_HoodSpeed(void)
{
    cloud_attr_t *attr = get_attr_ptr("HoodSpeed");
    if (attr == NULL)
    {
        return -1;
    }
    return *(attr->value);
}
signed char get_StoveStatus(void)
{
    cloud_attr_t *lattr = get_attr_ptr("LStoveStatus");
    if (lattr == NULL)
    {
        return -1;
    }
    cloud_attr_t *rattr = get_attr_ptr("RStoveStatus");
    if (rattr == NULL)
    {
        return -1;
    }
    return *(lattr->value) || *(rattr->value);
}
signed char get_OtaCmdPushType(void)
{
    cloud_attr_t *attr = get_attr_ptr("OtaCmdPushType");
    if (attr == NULL)
    {
        return -1;
    }
    return *(attr->value);
}
signed char set_OtaCmdPushType(char type)
{
    cloud_attr_t *attr = get_attr_ptr("OtaCmdPushType");
    if (attr == NULL)
    {
        return -1;
    }
    *(attr->value) = type;
    return 0;
}
// #define SOFT_TEST
#ifdef SOFT_TEST
static char *cloud_set_json = NULL;
#endif
static void POSIXTimer_cb(union sigval val)
{
#ifndef SOFT_TEST

    // if (val.sival_int == 0)
    // {
    cloud_attr_t *attr = get_attr_ptr("CookbookName");
    if (attr == NULL)
        return;
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "CookbookName", attr->value);
    report_msg_all_platform(resp);
    // }
#else
    if (cloud_set_json == NULL)
        return;
    link_send_property_post(cloud_set_json);
    cJSON_free(cloud_set_json);
    cloud_set_json = NULL;
#endif
}

int set_attr_report_uds(cJSON *root, set_attr_t *attr) //调用相关上报回调函数，并拼包
{
    if (root == NULL)
    {
        return -1;
    }
    if (attr != NULL)
    {
        if (attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT)
        {
            cJSON *item = (cJSON *)attr->cb(attr, NULL);
            if (item != NULL)
                cJSON_AddItemToObject(root, attr->cloud_key, item);
        }
    }
    return 0;
}

int set_attr_ctrl_uds(cJSON *root, set_attr_t *attr, cJSON *item) //调用相关设置回调函数，并拼包
{
    if (root == NULL)
    {
        return -1;
    }
    if (attr->fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->fun_type == LINK_FUN_TYPE_ATTR_CTRL)
    {
        item = (cJSON *)attr->cb(attr, item);
        if (item != NULL)
        {
            cJSON_AddItemToObject(root, attr->cloud_key, item);
        }
    }
    return 0;
}

static int get_attr_report_event(cloud_attr_t *ptr, const char *value, const int event_all)
{
    if ((ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT_CTRL && ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT) || ptr->uart_cmd < 0 || strlen(ptr->cloud_key) == 0)
    {
        return -1;
    }
    else if (LINK_VALUE_TYPE_NUM != ptr->cloud_value_type)
    {
        return -1;
    }
    // dzlog_warn("get_attr_report_event:%s", ptr->cloud_key);
    if (strcmp("LStOvState", ptr->cloud_key) == 0)
    {
        if (*value == 4 && (*ptr->value != *value || event_all > 0))
        {
            link_send_event_post("LCookPush", NULL);
        }
    }
    else if (strcmp("RStOvState", ptr->cloud_key) == 0)
    {
        if (*value == 4 && (*ptr->value != *value || event_all > 0))
        {
            link_send_event_post("RCookPush", NULL);
        }
    }
    else if (strcmp("ErrorCodeShow", ptr->cloud_key) == 0)
    {
        if (*ptr->value != *value || event_all > 0)
        {
            cJSON *resp = cJSON_CreateObject();
            if (event_all > 0)
            {
                cJSON_AddNumberToObject(resp, "FaultCode", *value);
            }
            else if (*ptr->value != *value)
            {
                if (*ptr->value != 0 && *value == 0)
                {
                    cJSON_AddNumberToObject(resp, "FaultCode", 31);
                }
                else
                {
                    cJSON_AddNumberToObject(resp, "FaultCode", *value);
                }
            }
            char *json = cJSON_PrintUnformatted(resp);
            link_send_event_post("ErrorPush", json);
            cJSON_free(json);
            cJSON_Delete(resp);
        }
    }
    return 0;
}

int get_attr_report_value(cJSON *resp, cloud_attr_t *ptr) //把串口上报数据解析，并拼包成JSON
{
    if ((ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT_CTRL && ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT) || ptr->uart_cmd < 0 || strlen(ptr->cloud_key) == 0)
    {
        return -1;
    }
    cJSON *item = NULL;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        if (strcmp("MultiStageState", ptr->cloud_key) == 0)
        {
            item = cJSON_CreateObject();

            cJSON_AddNumberToObject(item, "cnt", ptr->value[0]);
            cJSON_AddNumberToObject(item, "current", ptr->value[1]);
            cJSON_AddNumberToObject(item, "remind", ptr->value[2]);
            if (ptr->value[2])
            {
                unsigned char remind_num = ptr->value[1];
                if (remind_num > 3)
                    remind_num = 2;
                if (remind_num > 0)
                    --remind_num;
                cJSON_AddStringToObject(item, "RemindText", g_cloud_dev->remind[remind_num]);
            }
            else
            {
                cJSON_AddStringToObject(item, "RemindText", "");
            }
        }
    }
    else
    {
        int cloud_val = 0;
        if (LINK_VALUE_TYPE_STRING_NUM == ptr->cloud_value_type || LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
        {
            for (int i = 0; i < ptr->uart_byte_len; ++i)
            {
                cloud_val = (cloud_val << 8) + ptr->value[i];
            }
        }
        if (LINK_VALUE_TYPE_STRING_NUM == ptr->cloud_value_type)
        {
            char buf[16];
            sprintf(buf, "%d", cloud_val);
            item = cJSON_CreateString(buf);
        }
        else if (LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
        {
            if (strcmp("SysPower", ptr->cloud_key) == 0)
            {
                // set_gesture_power(cloud_val);
                if (demo_mode != 0 && cloud_val == 0)
                {
                    demo_mode = 0;
                }
            }
            else if (strcmp("ErrorCode", ptr->cloud_key) == 0)
            {
            }
            else if (strcmp("ErrorCodeShow", ptr->cloud_key) == 0)
            {
            }
            item = cJSON_CreateNumber(cloud_val);
        }
        else if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
        {
            if (ptr->uart_byte_len > 1)
            {
                char *buf = (char *)malloc(ptr->uart_byte_len + 1);
                if (buf == NULL)
                {
                    dzlog_error("malloc error\n");
                    return -1;
                }
                memcpy(buf, ptr->value, ptr->uart_byte_len);
                buf[ptr->uart_byte_len] = 0;
                item = cJSON_CreateString(buf);
                free(buf);
            }
            else
            {
                if (strcmp("ComSWVersion", ptr->cloud_key) == 0)
                {
                    item = cJSON_CreateString(g_cloud_dev->software_ver);
                }
                else if (strcmp("ElcSWVersion", ptr->cloud_key) == 0 || strcmp("ElcHWVersion", ptr->cloud_key) == 0 || strcmp("PwrSWVersion", ptr->cloud_key) == 0 || strcmp("PwrHWVersion", ptr->cloud_key) == 0)
                {
                    char buf[6];
                    sprintf(buf, "%d.%d", *ptr->value >> 4, *ptr->value & 0x0f);
                    item = cJSON_CreateString(buf);
                    if (ptr->uart_cmd == 3)
                        link_fota_power_report_version(buf);
                }
                else if (strcmp("WifiMac", ptr->cloud_key) == 0)
                {
                    item = cJSON_CreateString(g_cloud_dev->mac);
                }
                else
                {
                    item = cJSON_CreateString(ptr->value);
                }
            }
        }
        else if (LINK_VALUE_TYPE_ARRAY == ptr->cloud_value_type)
        {
            // item = cJSON_CreateIntArray(ptr->value, ptr->uart_byte_len);
            item = cJSON_CreateArray();
            for (int i = 0; i < ptr->uart_byte_len; ++i)
            {
                cJSON_AddItemToArray(item, cJSON_CreateNumber(ptr->value[i]));
            }
        }
        else
        {
        }
    }
    if (item != NULL)
        cJSON_AddItemToObject(resp, ptr->cloud_key, item);
    return 0;
}
void power_ota_install();
int get_attr_set_value(cloud_attr_t *ptr, cJSON *item, unsigned char *out) //把阿里云下发数据解析，并解析成串口数据
{
    long num = 0;
    if (out == NULL || ptr->uart_cmd < 0)
        return 0;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        int index = 0;
        if (strcmp("MultiStageContent", ptr->cloud_key) == 0 || strcmp("CookbookParam", ptr->cloud_key) == 0)
        {
            if (strcmp("MultiStageContent", ptr->cloud_key) == 0)
            {
                memset(g_cloud_dev->remind, 0, sizeof(g_cloud_dev->remind));
            }

            int arraySize = cJSON_GetArraySize(item);
            if (arraySize == 0)
            {
                dzlog_error("get_attr_set_value arraySize is 0\n");
                return 0;
            }
            ptr->uart_byte_len = arraySize * 13 + 1;
            out[1] = arraySize;
            index = 2;
            cJSON *arraySub, *RemindText, *Mode, *Temp, *Timer, *SteamTime, *FanTime, *WaterTime, *SteamGear;
            for (int i = 0; i < arraySize; i++)
            {
                arraySub = cJSON_GetArrayItem(item, i);
                if (arraySub == NULL)
                    continue;
                out[index++] = i + 1;

                if (cJSON_HasObjectItem(arraySub, "RemindText"))
                {
                    RemindText = cJSON_GetObjectItem(arraySub, "RemindText");
                    if (strlen(RemindText->valuestring) > 0)
                    {
                        strcpy(g_cloud_dev->remind[i], RemindText->valuestring);
                        out[index++] = 1;
                    }
                    else
                    {
                        out[index++] = 0;
                    }
                }
                else
                {
                    out[index++] = 0;
                }
                Mode = cJSON_GetObjectItem(arraySub, "Mode");
                out[index++] = Mode->valueint;
                Temp = cJSON_GetObjectItem(arraySub, "Temp");
                out[index++] = Temp->valueint >> 8;
                out[index++] = Temp->valueint;
                Timer = cJSON_GetObjectItem(arraySub, "Timer");
                out[index++] = Timer->valueint >> 8;
                out[index++] = Timer->valueint;

                if (cJSON_HasObjectItem(arraySub, "SteamTime"))
                {
                    SteamTime = cJSON_GetObjectItem(arraySub, "SteamTime");
                    out[index++] = SteamTime->valueint >> 8;
                    out[index++] = SteamTime->valueint;
                }
                else
                {
                    out[index++] = 0;
                    out[index++] = 0;
                }

                if (cJSON_HasObjectItem(arraySub, "FanTime"))
                {
                    FanTime = cJSON_GetObjectItem(arraySub, "FanTime");
                    out[index++] = FanTime->valueint >> 8;
                    out[index++] = FanTime->valueint;
                }
                else
                {
                    out[index++] = 0;
                    out[index++] = 0;
                }

                if (cJSON_HasObjectItem(arraySub, "WaterTime"))
                {
                    WaterTime = cJSON_GetObjectItem(arraySub, "WaterTime");
                    out[index++] = WaterTime->valueint;
                }
                else
                {
                    out[index++] = 0;
                }
                if (cJSON_HasObjectItem(arraySub, "SteamGear"))
                {
                    SteamGear = cJSON_GetObjectItem(arraySub, "SteamGear");
                    if (SteamGear->valueint <= 0 && SteamGear->valueint > 3)
                        out[index++] = 2;
                    else
                        out[index++] = SteamGear->valueint;
                }
                else
                {
                    out[index++] = 2;
                }
            }
            dzlog_warn("get_attr_set_value %s:%d %d", ptr->cloud_key, ptr->uart_byte_len, index);
        }
    }
    else
    {
        if (LINK_VALUE_TYPE_STRING_NUM == ptr->cloud_value_type)
        {
            if (stol(item->valuestring, 10, &num) == NULL)
                return 0;
        }
        else if (LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
        {
            num = item->valueint;
            if (strcmp("LoadPowerSet", ptr->cloud_key) == 0)
            {
                if (num == 4)
                    demo_mode = 1;
                else
                    demo_mode = 0;
            }
            else if (strcmp("ProductionTestStatus", ptr->cloud_key) == 0)
            {
                *ptr->value = num;
            }
            else if (strcmp("HoodSpeed", ptr->cloud_key) == 0)
            {
                power_ota_install();
            }
        }
        else if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
        {
            if (strcmp("CookbookName", ptr->cloud_key) == 0)
            {
                strcpy(ptr->value, item->valuestring);
                return 0;
            }
            memcpy(&out[1], item->valuestring, strlen(item->valuestring));
            goto end;
        }
        else
        {
            return 0;
        }
        for (int i = 0; i < ptr->uart_byte_len; ++i)
        {
            out[1 + i] = num >> (8 * (ptr->uart_byte_len - 1 - i));
        }
    }
end:
    out[0] = ptr->uart_cmd;
    dzlog_warn("uart_cmd:%d", ptr->uart_cmd);
    return ptr->uart_byte_len + 1;
}

void send_data_to_cloud(const unsigned char *value, const int value_len, const unsigned char command) //所有串口数据解析，并上报阿里云平台和UI
{
    // dzlog_debug("send_data_to_cloud...");
    // hdzlog_info((unsigned char *)value, value_len);
    int i, j;
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *cloud_attr = cloud_dev->attr;
    if (value == NULL)
    {
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cloud_attr_t *attr;
    for (i = 0; i < value_len; ++i)
    {
        for (j = 0; j < cloud_dev->attr_len; ++j)
        {
            attr = &cloud_attr[j];
            if (value[i] == attr->uart_cmd)
            {
                get_attr_report_event(attr, (char *)&value[i + 1], 0);
                memcpy(attr->value, &value[i + 1], attr->uart_byte_len);
                // dzlog_debug("i:%d cloud_key:%s", i, attr->cloud_key);
                // hdzlog_info((unsigned char *)attr->value, attr->uart_byte_len);
                get_attr_report_value(root, attr);
                switch (attr->uart_cmd)
                {
                case 0x4f:
                {
                    cloud_attr_t *ptr = get_attr_ptr("CookbookName");
                    if (ptr != NULL)
                    {
                        get_attr_report_value(root, ptr);
                    }
                }
                break;
                case 0x31:
                {
                    cloud_attr_t *ptr = get_attr_ptr_from_uartCmd(0x39);
                    if (ptr != NULL)
                    {
                        unsigned char value = *(ptr->value);
                        if (value == 2 || value == 3 || value == 6)
                        {
                            recv_ecb_gear(*(attr->value), 1);
                            *(ptr->value) = 0;
                            break;
                        }
                    }
                    recv_ecb_gear(*(attr->value), 0);
                }
                break;
                case 0x35:
                    cook_assist_set_smartSmoke(*(attr->value));
                    break;
                case 0x20:
                    recv_ecb_fire(*(attr->value), INPUT_RIGHT);
                    break;
                case 0x11:
                    set_stove_status(*(attr->value), INPUT_LEFT);
                    break;
                case 0x12:
                    set_stove_status(*(attr->value), INPUT_RIGHT);
                    break;
                }
                i += attr->uart_byte_len;
                break;
            }
        }
    }

    if (cJSON_Object_isNull(root))
    {
        cJSON_Delete(root);
        dzlog_warn("%s,send NULL", __func__);
        return;
    }
    if (cJSON_HasObjectItem(root, "LoadPowerState") == 0 && cJSON_HasObjectItem(root, "PCBInput") == 0)
    {
        char *json = cJSON_PrintUnformatted(root);
        link_send_property_post(json);
        cJSON_free(json);
    }
    if (ECB_UART_COMMAND_GETACK == command)
    {
        if (first_uds_report >= 2)
        {
            cJSON_Delete(root);
        }
        else
        {
            ++first_uds_report;
            send_event_uds(root, NULL);
        }
    }
    else
    {
        send_event_uds(root, NULL);
    }
    return;
}

int send_all_to_cloud(void) //发送所有属性给阿里云平台，用于刚建立连接
{
    dzlog_info("send_all_to_cloud");
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *cloud_attr = cloud_dev->attr;
    char *json = NULL;

    cJSON *root = cJSON_CreateObject();
    cloud_attr_t *attr;
    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        attr = &cloud_attr[i];
        if (strcmp("DataReportReason", attr->cloud_key) == 0 || strcmp("HoodOffRemind", attr->cloud_key) == 0 || strcmp("LoadPowerState", attr->cloud_key) == 0 || strcmp("PCBInput", attr->cloud_key) == 0)
            continue;
        get_attr_report_event(attr, attr->value, 1);
        get_attr_report_value(root, attr);
    }
    cook_assist_report_all(root);
    json = cJSON_PrintUnformatted(root);
    link_send_property_post(json);
    cJSON_free(json);
    cJSON_Delete(root);
    return 0;
}

int cloud_resp_get(cJSON *root, cJSON *resp) //解析UI GET命令
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *cloud_attr = cloud_dev->attr;
    cloud_attr_t *attr;
    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        attr = &cloud_attr[i];
        if (cJSON_HasObjectItem(root, attr->cloud_key))
        {
            if (strcmp("HoodOffRemind", attr->cloud_key) == 0)
                continue;
            get_attr_report_value(resp, attr);
        }
    }
    return 0;
}

int cloud_resp_getall(cJSON *root, cJSON *resp) //解析UI GETALL命令
{

    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *cloud_attr = cloud_dev->attr;
    cloud_attr_t *attr;
    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        attr = &cloud_attr[i];
        if (strcmp("HoodOffRemind", attr->cloud_key) == 0)
            continue;
        else if (strcmp("PwrSWVersion", attr->cloud_key) == 0)
            continue;
        get_attr_report_value(resp, attr);
    }
    cook_assist_report_all(resp);
    link_ntp_request();
    if (root != NULL)
    {
        if (!cJSON_IsNull(root))
            uds_report_reset();
    }
    return 0;
}

int cloud_resp_set(cJSON *root, cJSON *resp) //解析UI SETALL命令或阿里云平台下发命令
{
    pthread_mutex_lock(&mutex);
    cook_assist_start_single_recv();
    unsigned char uart_buf[256];
    int uart_buf_len = 0;

    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *cloud_attr = cloud_dev->attr;
    cloud_attr_t *attr;
    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        attr = &cloud_attr[i];
        if ((attr->cloud_fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->cloud_fun_type == LINK_FUN_TYPE_ATTR_CTRL) && cJSON_HasObjectItem(root, attr->cloud_key))
        {
            cJSON *item = cJSON_GetObjectItem(root, attr->cloud_key);
            if (cook_assist_recv_property_set(attr->cloud_key, item) != 0)
                uart_buf_len += get_attr_set_value(attr, item, &uart_buf[uart_buf_len]);
        }
    }
    if (uart_buf_len > 0)
    {
        ecb_uart_send_cloud_msg(uart_buf, uart_buf_len);
    }
    cook_assist_end_single_recv();
    pthread_mutex_unlock(&mutex);
    return 0;
}

static int recv_data_from_cloud(unsigned long devid, char *value, int value_len) //阿里云下发接口回调函数，初始化时注册
{
    if (demo_mode != 0)
        return -1;
    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        dzlog_error("JSON Parse Error");
        return -1;
    }
    cloud_resp_set(root, NULL);
#ifndef SOFT_TEST
    cJSON_Delete(root);
#else
    cloud_set_json = cJSON_PrintUnformatted(root);
    POSIXTimerSet(cook_name_timer, 0, 1);
    send_event_uds(root, NULL);
#endif
    return 0;
}
int save_device_secret(const char *device_secret)
{
    strcpy(g_cloud_dev->device_secret, device_secret);
    int res = 0;
    systemRun("chmod 600 ../" QUAD_NAME);
    systemRun("chmod 600 " QUAD_NAME);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, PRODUCT_KEY, g_cloud_dev->product_key);
    cJSON_AddStringToObject(root, PRODUCT_SECRET, g_cloud_dev->product_secret);
    cJSON_AddStringToObject(root, DEVICE_NAME, g_cloud_dev->device_name);
    cJSON_AddStringToObject(root, DEVICE_SECRET, g_cloud_dev->device_secret);

    char *json = cJSON_PrintUnformatted(root);

    res = operateFile(1, "../" QUAD_NAME, json, strlen(json));
    res = operateFile(1, QUAD_NAME, json, strlen(json));

    cJSON_free(json);
    cJSON_Delete(root);

    root = cJSON_CreateObject();
    cJSON_AddNullToObject(root, PRODUCT_KEY);
    cJSON_AddNullToObject(root, PRODUCT_SECRET);
    cJSON_AddNullToObject(root, DEVICE_NAME);
    cJSON_AddNullToObject(root, DEVICE_SECRET);
    cJSON *resp = cJSON_CreateObject();
    device_resp_get(root, resp);
    send_event_uds(resp, NULL);
    cJSON_Delete(root);

    systemRun("chmod 400 ../" QUAD_NAME);
    systemRun("chmod 400 " QUAD_NAME);

    sync();
    return res;
}

int save_device_quad(const char *productkey, const char *productsecret, const char *devicename, const char *devicesecret)
{
    strcpy(g_cloud_dev->product_key, productkey);
    strcpy(g_cloud_dev->product_secret, productsecret);
    strcpy(g_cloud_dev->device_name, devicename);
    return save_device_secret(devicesecret);
}

static void *cloud_quad_parse_json(void *input, const char *str) //启动时解析四元组文件
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }

    cJSON *ProductKey = cJSON_GetObjectItem(root, PRODUCT_KEY);
    if (ProductKey == NULL)
    {
        dzlog_error("ProductKey is NULL\n");
        goto fail;
    }
    cJSON *ProductSecret = cJSON_GetObjectItem(root, PRODUCT_SECRET);
    if (ProductSecret == NULL)
    {
        dzlog_error("ProductSecret is NULL\n");
        goto fail;
    }
    cJSON *DeviceName = cJSON_GetObjectItem(root, DEVICE_NAME);
    if (DeviceName == NULL)
    {
        dzlog_error("DeviceName is NULL\n");
        goto fail;
    }
    cJSON *DeviceSecret = cJSON_GetObjectItem(root, DEVICE_SECRET);
    if (DeviceSecret == NULL)
    {
        dzlog_error("DeviceSecret is NULL\n");
        goto fail;
    }

    cloud_dev_t *cloud_dev = (cloud_dev_t *)input;
    strcpy(cloud_dev->product_key, ProductKey->valuestring);
    strcpy(cloud_dev->product_secret, ProductSecret->valuestring);
    strcpy(cloud_dev->device_name, DeviceName->valuestring);
    strcpy(cloud_dev->device_secret, DeviceSecret->valuestring);

    cJSON_Delete(root);
    return cloud_dev;
fail:
    cJSON_Delete(root);
    return NULL;
}
static void *cloud_parse_json(void *input, const char *str) //启动时解析转换配置文件
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }
    cJSON *DeviceCategory = cJSON_GetObjectItem(root, "DeviceCategory");
    if (DeviceCategory == NULL)
    {
        dzlog_error("DeviceCategory is NULL\n");
        goto fail;
    }
    cJSON *DeviceModel = cJSON_GetObjectItem(root, "DeviceModel");
    if (DeviceModel == NULL)
    {
        dzlog_error("DeviceModel is NULL\n");
        goto fail;
    }
    cJSON *HardwareVer = cJSON_GetObjectItem(root, "HardwareVer");
    if (HardwareVer == NULL)
    {
        dzlog_error("HardwareVer is NULL\n");
        goto fail;
    }
    cJSON *UpdateLog = cJSON_GetObjectItem(root, "UpdateLog");
    if (UpdateLog == NULL)
    {
        dzlog_error("UpdateLog is NULL\n");
        goto fail;
    }
    cJSON *attr = cJSON_GetObjectItem(root, "attr");
    if (attr == NULL)
    {
        dzlog_error("attr is NULL\n");
        goto fail;
    }

    int arraySize = cJSON_GetArraySize(attr);
    if (arraySize == 0)
    {
        dzlog_error("attr arraySize is 0\n");
        goto fail;
    }
    int i;
    cloud_dev_t *cloud_dev = (cloud_dev_t *)malloc(sizeof(cloud_dev_t));
    if (cloud_dev == NULL)
    {
        dzlog_error("malloc error\n");
        goto fail;
    }
    memset(cloud_dev, 0, sizeof(cloud_dev_t));
    cloud_dev->attr_len = arraySize;
    cloud_dev->attr = (cloud_attr_t *)malloc(sizeof(cloud_attr_t) * cloud_dev->attr_len);
    if (cloud_dev->attr == NULL)
    {
        dzlog_error("malloc error\n");
        goto fail;
    }
    memset(cloud_dev->attr, 0, sizeof(cloud_attr_t) * cloud_dev->attr_len);

    strcpy(cloud_dev->device_category, DeviceCategory->valuestring);
    strcpy(cloud_dev->device_model, DeviceModel->valuestring);
    strcpy(cloud_dev->update_log, UpdateLog->valuestring);
    strcpy(cloud_dev->hardware_ver, HardwareVer->valuestring);

    cJSON *arraySub, *cloudKey, *valueType, *funType, *uartCmd, *uartByteLen;
    for (i = 0; i < arraySize; i++)
    {
        arraySub = cJSON_GetArrayItem(attr, i);
        if (arraySub == NULL)
            continue;

        if (cJSON_HasObjectItem(arraySub, "cloudKey"))
        {
            cloudKey = cJSON_GetObjectItem(arraySub, "cloudKey");
            strcpy(cloud_dev->attr[i].cloud_key, cloudKey->valuestring);
        }
        valueType = cJSON_GetObjectItem(arraySub, "valueType");
        cloud_dev->attr[i].cloud_value_type = valueType->valueint;
        funType = cJSON_GetObjectItem(arraySub, "funType");
        cloud_dev->attr[i].cloud_fun_type = funType->valueint;

        uartCmd = cJSON_GetObjectItem(arraySub, "uartCmd");
        cloud_dev->attr[i].uart_cmd = uartCmd->valueint;
        if (cJSON_HasObjectItem(arraySub, "uartByteLen"))
        {
            uartByteLen = cJSON_GetObjectItem(arraySub, "uartByteLen");
            cloud_dev->attr[i].uart_byte_len = uartByteLen->valueint;
        }
        if (cloud_dev->attr[i].uart_byte_len > 0)
        {
            cloud_dev->attr[i].value = (char *)malloc(cloud_dev->attr[i].uart_byte_len);
            if (cloud_dev->attr[i].value == NULL)
            {
                dzlog_error("malloc error\n");
                goto fail;
            }
            memset(cloud_dev->attr[i].value, 0, cloud_dev->attr[i].uart_byte_len);
        }
    }
    if (cJSON_HasObjectItem(root, "service"))
    {
    }
    if (cJSON_HasObjectItem(root, "event"))
    {
    }
    cJSON_Delete(root);
    return cloud_dev;
fail:
    cJSON_Delete(root);
    return NULL;
}
static int recv_sync_service_invoke(char *service_id, char **data)
{
    return 0;
}

static void quad_burn_success()
{
    sleep(1);
    wifiDisconnect();
    systemRun("wpa_cli remove_network all && wpa_cli save_config && sync");
}

static void link_timestamp_cb(const unsigned long timestamp)
{
    dzlog_warn("link_timestamp_cb:%ld", timestamp);
    struct timeval tv;
    tv.tv_sec = timestamp;
    settimeofday(&tv, NULL);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "NtpTimestamp", timestamp);
    send_event_uds(root, NULL);
}
int cloud_init(void) //初始化
{
    pthread_mutex_init(&mutex, NULL);
    register_save_quad_cb(save_device_quad);
    register_report_message_cb(report_msg_quad_uds);
    register_quad_burn_success_cb(quad_burn_success);
    register_link_timestamp_cb(link_timestamp_cb);
    register_recv_sync_service_invoke_cb(recv_sync_service_invoke);
    register_property_set_event_cb(recv_data_from_cloud); //注册阿里云下发回调
#ifdef DYNREGMQ
    register_dynreg_device_secret_cb(save_device_secret);
#endif
    cook_name_timer = POSIXTimerCreate(0, POSIXTimer_cb);

    g_cloud_dev = get_dev_profile(".", NULL, PROFILE_NAME, cloud_parse_json);
    if (g_cloud_dev == NULL)
    {
        dzlog_error("cloud_init error\n");
        return -1;
    }

    strcpy(g_cloud_dev->software_ver, SOFTER_VER);
    if (get_dev_profile("..", g_cloud_dev, QUAD_NAME, cloud_quad_parse_json) == NULL)
    {
        if (get_dev_profile(".", g_cloud_dev, QUAD_NAME, cloud_quad_parse_json) == NULL)
        {
            dzlog_error("cloud_init cloud_quad_parse_json error\n");
            return -1;
        }
    }
    quad_burn_init();
    getNetworkMac(ETH_NAME, g_cloud_dev->mac, sizeof(g_cloud_dev->mac), "");
    curl_http_request_init();
    // char buf[] = {1, 33, 44, 55, 77, 99};
    // http_report_hex("SET:", buf, sizeof(buf));
    // http_report_hex("EVENT:", buf, sizeof(buf));
    return 0;
}

void cloud_deinit(void) //反初始化
{
    curl_http_request_deinit();
    quad_burn_deinit();
    link_model_close();
    for (int i = 0; i < g_cloud_dev->attr_len; ++i)
    {
        if (g_cloud_dev->attr[i].uart_byte_len > 0)
            free(g_cloud_dev->attr[i].value);
    }
    free(g_cloud_dev->attr);
    free(g_cloud_dev);
    dzlog_warn("cloud_deinit...........\n");
    if (cook_name_timer != NULL)
    {
        POSIXTimerDelete(cook_name_timer);
        cook_name_timer = NULL;
    }
    pthread_mutex_destroy(&mutex);
}

void get_quad(void)
{
    char ip[18] = {0};
    unsigned int s_addr = 0;
    if (strlen(g_cloud_dev->device_secret) == 0)
    {
        s_addr = getNetworkIp(ETH_NAME, NULL, 0);
        if (s_addr < 0)
        {
            return;
        }
        s_addr &= ~(0xff << 24);
        s_addr |= 200 << 24;
        inet_ntop(AF_INET, &s_addr, ip, sizeof(ip));
        dzlog_warn("ip:0X%x,url:%s", s_addr, ip);

        quad_burn_requst(g_cloud_dev->product_key, g_cloud_dev->device_name, ip);
    }
}
// size_t http_weather_cb(void *ptr, size_t size, size_t nmemb, void *stream)
// {
//     printf("%s size:%u,nmemb:%u\n", __func__, size, nmemb);
//     printf("%s data:%s\n", __func__, (char *)ptr);
//     return size * nmemb;
// }
// int curl_weather()
// {
//     CURLcode res;
//     struct curl_slist *headers = NULL;

//     // get a curl handle
//     CURL *curl = curl_easy_init();
//     if (curl)
//     {
//         curl_easy_setopt(curl, CURLOPT_URL, "https://wttr.in/杭州?format=j2");//http://mcook.marssenger.com/application/weather/day https://wttr.in
//         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_weather_cb);
//         curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
//         // curl_easy_setopt(curl, CURLOPT_POST, 1);

//         curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
//         curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
//         // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
//         // perform the request, res will get the return code
//         res = curl_easy_perform(curl);
//         // check for errors
//         if (res != CURLE_OK)
//         {
//             fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
//         }
//         else
//         {
//             fprintf(stdout, "curl_easy_perform() success.\n");
//         }
//         // always cleanup
//         curl_easy_cleanup(curl);
//     }
//     return 0;
// }

void *cloud_task(void *arg) //云端任务
{
    if (strlen(g_cloud_dev->product_key) == 0)
    {
        strcpy(g_cloud_dev->product_key, DEFAULT_PRODUCT_KEY);
    }
    if (strlen(g_cloud_dev->product_secret) == 0)
    {
        strcpy(g_cloud_dev->product_secret, DEFAULT_PRODUCT_SECRET);
    }
    if (strlen(g_cloud_dev->device_name) == 0)
    {
        strcpy(g_cloud_dev->device_name, g_cloud_dev->mac);
    }
#if 1
    do
    {
        sleep(2);

        // curl_weather();
        if (strlen(g_cloud_dev->device_secret) > 0)
        {
            cloud_attr_t *attr = get_attr_ptr("ProductionTestStatus");
            if (attr == NULL)
            {
                dzlog_error("not attr ProductionTestStatus");
            }
            else
            {
                if (*attr->value > 0)
                {
                    dzlog_warn("wait ProductionTestStatus");
                    continue;
                }
            }
            if (getWifiRunningState() == RK_WIFI_State_CONNECTED)
            {
                link_main(g_cloud_dev->product_key, g_cloud_dev->product_secret, g_cloud_dev->device_name, g_cloud_dev->device_secret, g_cloud_dev->software_ver);
                break;
            }
            else
            {
                dzlog_warn("no Internet...");
            }
        }
        else
        {
            dzlog_warn("wait curl_http_get_quad");
        }
    } while (1);
#else
    link_main(g_cloud_dev->product_key, g_cloud_dev->product_secret, g_cloud_dev->device_name, g_cloud_dev->device_secret, g_cloud_dev->software_ver);
#endif
    return NULL;
}
#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"
#include "rkwifi.h"
#include "ecb_uart_parse_msg.h"
#include "link_solo.h"
#include "link_dynregmq_posix.h"
#include "link_fota_posix.h"
#include "cloud_platform_task.h"
#include "database_task.h"
#include "device_task.h"
#include "gesture_uart.h"
#include "POSIXTimer.h"
#include "md5_func.h"

static timer_t cook_name_timer;
static pthread_mutex_t mutex;
static cloud_dev_t *g_cloud_dev = NULL;

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

cloud_dev_t *get_cloud_dev(void)
{
    return g_cloud_dev;
}

unsigned char get_ErrorCodeShow(void)
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (strcmp("ErrorCodeShow", attr->cloud_key) == 0)
        {
            return *(attr->value);
        }
    }
    return 0;
}
unsigned int get_ErrorCode(void)
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (strcmp("ErrorCode", attr->cloud_key) == 0)
        {
            return *((int *)(attr->value));
        }
    }
    return 0;
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
    if ((ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT_CTRL && ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT) || strlen(ptr->cloud_key) == 0)
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
    if ((ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT_CTRL && ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT) || strlen(ptr->cloud_key) == 0)
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
                cJSON_AddStringToObject(item, "RemindText", g_cloud_dev->remind[(int)ptr->value[1]]);
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
                set_gesture_power(cloud_val);
            }
            else if (strcmp("CookbookID", ptr->cloud_key) == 0)
            {
                if (cloud_val > 0)
                {
                    cloud_attr_t *attr = get_attr_ptr("CookbookName");
                    if (attr != NULL && attr->value != NULL && strlen(attr->value) == 0)
                    {
                        if (select_for_cookbookID(cloud_val, attr->value, attr->uart_byte_len) == 0)
                        {
                            cJSON *resp = cJSON_CreateObject();
                            cJSON_AddStringToObject(resp, "CookbookName", attr->value);
                            report_msg_all_platform(resp);
                        }
                    }
                }
            }
            item = cJSON_CreateNumber(cloud_val);
        }
        else if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
        {
            if (ptr->uart_byte_len > 1)
            {
                char *buf = (char *)malloc(ptr->uart_byte_len + 1);
                memcpy(buf, ptr->value, ptr->uart_byte_len);
                ptr->value[ptr->uart_byte_len - 1] = 0;
                item = cJSON_CreateString(buf);
                free(buf);
            }
            else
            {
                if (strcmp("ComSWVersion", ptr->cloud_key) == 0)
                {
                    item = cJSON_CreateString(g_cloud_dev->software_ver);
                }
                else if (strcmp("ElcSWVersion", ptr->cloud_key) == 0 || strcmp("ElcHWVersion", ptr->cloud_key) == 0)
                {
                    char buf[6];
                    sprintf(buf, "%d.%d", *ptr->value >> 4, *ptr->value & 0x0f);
                    item = cJSON_CreateString(buf);
                }
                else if (strcmp("WifiMac", ptr->cloud_key) == 0)
                {
                    char mac[16];
                    getNetworkMac(ETH_NAME, mac, sizeof(mac), "");
                    item = cJSON_CreateString(mac);
                }
                else
                {
                    item = cJSON_CreateString(ptr->value);
                }
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

int get_attr_set_value(cloud_attr_t *ptr, cJSON *item, unsigned char *out) //把阿里云下发数据解析，并解析成串口数据
{
    long num = 0;
    if (out == NULL)
        return 0;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        int index = 0;
        if (strcmp("MultiStageContent", ptr->cloud_key) == 0 || strcmp("CookbookParam", ptr->cloud_key) == 0)
        {
            if (strcmp("MultiStageContent", ptr->cloud_key) == 0)
            {
                dzlog_warn("get_attr_set_value remind size:%d", sizeof(g_cloud_dev->remind));
                memset(g_cloud_dev->remind, 0, sizeof(g_cloud_dev->remind));
            }

            int arraySize = cJSON_GetArraySize(item);
            if (arraySize == 0)
            {
                dzlog_error("get_attr_set_value arraySize is 0\n");
                return 0;
            }
            ptr->uart_byte_len = arraySize * 12 + 1;
            out[1] = arraySize;
            index = 2;
            cJSON *arraySub, *RemindText, *Mode, *Temp, *Timer, *SteamTime, *FanTime, *WaterTime;
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

void send_data_to_cloud(const unsigned char *value, const int value_len) //所有串口数据解析，并上报阿里云平台和UI
{
    hdzlog_info((unsigned char *)value, value_len);
    int i, j;
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    cJSON *root = cJSON_CreateObject();
    if (value != NULL)
    {
        for (i = 0; i < value_len; ++i)
        {
            for (j = 0; j < cloud_dev->attr_len; ++j)
            {
                if (value[i] == attr[j].uart_cmd)
                {
                    get_attr_report_event(&attr[j], (char *)&value[i + 1], 0);
                    memcpy(attr[j].value, &value[i + 1], attr[j].uart_byte_len);
                    // dzlog_debug("i:%d cloud_key:%s", i, attr[j].cloud_key);
                    // hdzlog_info((unsigned char *)attr[j].value, attr[j].uart_byte_len);
                    get_attr_report_value(root, &attr[j]);
                    if (strcmp("MultiMode", attr[j].cloud_key) == 0 && *(attr[j].value) == 1)
                    {
                        cloud_attr_t *ptr = get_attr_ptr("CookbookName");
                        if (ptr != NULL)
                        {
                            get_attr_report_value(root, ptr);
                        }
                    }
                    i += attr[j].uart_byte_len;
                    break;
                }
            }
        }
    }
    report_msg_all_platform(root);
    // cJSON_Delete(root);
}

int send_all_to_cloud(void) //发送所有属性给阿里云平台，用于刚建立连接
{
    dzlog_info("send_all_to_cloud");
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;
    char *json = NULL;

    cJSON *root = cJSON_CreateObject();

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        get_attr_report_event(&attr[i], attr[i].value, 1);
        get_attr_report_value(root, &attr[i]);
    }
    json = cJSON_PrintUnformatted(root);
    link_send_property_post(json);
    cJSON_free(json);
    cJSON_Delete(root);

    // json = get_link_CookHistory();
    // link_send_property_post(json);
    // cJSON_free(json);
    return 0;
}

int cloud_resp_get(cJSON *root, cJSON *resp) //解析UI GET命令
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            get_attr_report_value(resp, &attr[i]);
        }
    }
    return 0;
}

int cloud_resp_getall(cJSON *root, cJSON *resp) //解析UI GETALL命令
{
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        get_attr_report_value(resp, &attr[i]);
    }
    return 0;
}

int cloud_resp_set(cJSON *root, cJSON *resp) //解析UI SETALL命令或阿里云平台下发命令
{
    pthread_mutex_lock(&mutex);
    unsigned char uart_buf[256];
    int uart_buf_len = 0;

    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;
    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        if ((attr[i].cloud_fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr[i].cloud_fun_type == LINK_FUN_TYPE_ATTR_CTRL) && cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            cJSON *item = cJSON_GetObjectItem(root, attr[i].cloud_key);
            uart_buf_len += get_attr_set_value(&attr[i], item, &uart_buf[uart_buf_len]);
        }
    }
    if (uart_buf_len > 0)
    {
        ecb_uart_send_cloud_msg(uart_buf, uart_buf_len);
    }

    cJSON *resp_db = cJSON_CreateObject();
    database_resp_set(root, resp_db);
    report_msg_all_platform(resp_db);
    pthread_mutex_unlock(&mutex);
    return 0;
}

static int recv_data_from_cloud(unsigned long devid, char *value, int value_len) //阿里云下发接口回调函数，初始化时注册
{
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

    res = operateFile(1, "../" QUAD_NAME ".json", json, strlen(json));
    res = operateFile(1, QUAD_NAME ".json", json, strlen(json));

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
    cJSON *AfterSalesPhone = cJSON_GetObjectItem(root, "AfterSalesPhone");
    if (AfterSalesPhone == NULL)
    {
        dzlog_error("AfterSalesPhone is NULL\n");
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
    memset(cloud_dev, 0, sizeof(cloud_dev_t));
    cloud_dev->attr_len = arraySize;
    cloud_dev->attr = (cloud_attr_t *)malloc(sizeof(cloud_attr_t) * cloud_dev->attr_len);
    memset(cloud_dev->attr, 0, sizeof(cloud_attr_t) * cloud_dev->attr_len);

    strcpy(cloud_dev->device_category, DeviceCategory->valuestring);
    strcpy(cloud_dev->device_model, DeviceModel->valuestring);
    strcpy(cloud_dev->after_sales_phone, AfterSalesPhone->valuestring);
    strcpy(cloud_dev->update_log, UpdateLog->valuestring);
    strcpy(cloud_dev->hardware_ver, HardwareVer->valuestring);

    cJSON *arraySub, *cloudKey, *valueType, *uartCmd, *uartByteLen;
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
    if (strcmp("GetHistory", service_id) == 0)
    {
        *data = get_link_CookHistory();
    }
    else
    {
        return -1;
    }
    return 0;
}
static void ota_complete_cb(void)
{
    sync();
    reboot(RB_AUTOBOOT);
}
int cloud_init(void) //初始化
{
    cook_name_timer = POSIXTimerCreate(0, POSIXTimer_cb);
    pthread_mutex_init(&mutex, NULL);
    register_ota_complete_cb(ota_complete_cb);
    register_recv_sync_service_invoke_cb(recv_sync_service_invoke);
    register_property_set_event_cb(recv_data_from_cloud); //注册阿里云下发回调
#ifdef DYNREGMQ
    register_dynreg_device_secret_cb(save_device_secret);
#endif
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
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

void cloud_deinit(void) //反初始化
{
    curl_global_cleanup();
    link_model_close();
    for (int i = 0; i < g_cloud_dev->attr_len; ++i)
        free(g_cloud_dev->attr[i].value);
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
#define QUAD_REQUEST_URL_FMT "http://%s:10010"
static char quad_request_url[80];
size_t http_get_quad_cb(void *ptr, size_t size, size_t nmemb, void *stream);

int curl_http_quad(const char *product_key, const char *mac, const char *path, const char *body)
{
    char buf[255];

    CURLcode res;
    struct curl_slist *headers = NULL;

    // get a curl handle
    CURL *curl = curl_easy_init();
    if (curl)
    {
        headers = curl_slist_append(headers, "Content-Type:application/json");
        sprintf(buf, "pk:%s", product_key);
        headers = curl_slist_append(headers, buf);
        sprintf(buf, "mac:%s", mac);
        headers = curl_slist_append(headers, buf);

        sprintf(buf, "%s%s%s%s", "1643738522000", product_key, mac, "mars");
        char md5_str[33] = {0};
        Compute_string_md5((unsigned char *)buf, strlen(buf), md5_str);
        sprintf(buf, "sign:%s.%s", "1643738522000", md5_str);
        headers = curl_slist_append(headers, buf);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // set the URL with GET request
        sprintf(buf, "%s%s", quad_request_url, path);
        curl_easy_setopt(curl, CURLOPT_URL, buf);

        if (body == NULL)
        {
            // write response msg into strResponse
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_quad_cb);
            // curl_easy_setopt(curl, CURLOPT_WRITEDATA, master_meta_info);
            // curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            body = "";
        }

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
        curl_easy_setopt(curl, CURLOPT_POST, 1);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        // perform the request, res will get the return code
        res = curl_easy_perform(curl);
        // check for errors
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            fprintf(stdout, "curl_easy_perform() success.\n");
        }
        // always cleanup
        curl_easy_cleanup(curl);
    }
    return 0;
}
size_t http_get_quad_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
    printf("http_get_quad_cb size:%u,nmemb:%u\n", size, nmemb);
    printf("http_get_quad_cb data:%s\n", (char *)ptr);
    // iotx_linkkit_dev_meta_info_t *master_meta_info = (iotx_linkkit_dev_meta_info_t *)stream;
    int res = 2, quad_rupleId = 0;
    cJSON *root = cJSON_Parse(ptr);
    if (root == NULL)
        return -1;
    char *json = cJSON_PrintUnformatted(root);
    printf("http_get_quad_cb json:%s\n", json);
    free(json);

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code == NULL)
    {
        goto fail;
    }
    if (code->valueint != 0)
        goto fail;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL)
    {
        goto fail;
    }
    if (cJSON_HasObjectItem(data, "quadrupleId"))
    {
        cJSON *quadrupleId = cJSON_GetObjectItem(data, "quadrupleId");
        quad_rupleId = quadrupleId->valueint;
    }
    if (cJSON_HasObjectItem(data, "productKey"))
    {
        cJSON *ProductKey = cJSON_GetObjectItem(data, "productKey");
        strcpy(g_cloud_dev->product_key, ProductKey->valuestring);
    }
    if (cJSON_HasObjectItem(data, "productSecret"))
    {
        cJSON *ProductSecret = cJSON_GetObjectItem(data, "productSecret");
        strcpy(g_cloud_dev->product_secret, ProductSecret->valuestring);
    }
    if (cJSON_HasObjectItem(data, "deviceName"))
    {
        cJSON *DeviceName = cJSON_GetObjectItem(data, "deviceName");
        strcpy(g_cloud_dev->device_name, DeviceName->valuestring);
    }
    if (cJSON_HasObjectItem(data, "deviceSecret"))
    {
        cJSON *DeviceSecret = cJSON_GetObjectItem(data, "deviceSecret");
        save_device_secret(DeviceSecret->valuestring);
        res = 1;
    }
fail:
    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "quadrupleId", quad_rupleId);
    cJSON_AddNumberToObject(resp, "state", res);
    char *body = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    curl_http_quad(g_cloud_dev->product_key, g_cloud_dev->device_name, "/iot/quadruple/report", body);
    cJSON_free(body);
    return size * nmemb;
}

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
        getNetworkMac(ETH_NAME, g_cloud_dev->device_name, sizeof(g_cloud_dev->device_name), "");
    }
#if 1
    do
    {
        if (getWifiRunningState() == RK_WIFI_State_CONNECTED)
        {
            if (strlen(g_cloud_dev->device_secret) == 0)
            {
                char ip[24];
                unsigned int s_addr = getNetworkIp(ETH_NAME, NULL, 0);
                if (s_addr < 0)
                {
                    sleep(1);
                    continue;
                }
                s_addr &= ~(0xff << 24);
                s_addr |= 200 << 24;
                inet_ntop(AF_INET, &s_addr, ip, sizeof(ip));
                dzlog_warn("ip:0X%x,url:%s", s_addr, ip);
                sprintf(quad_request_url, QUAD_REQUEST_URL_FMT, ip);
                dzlog_warn("quad_request_url:%s", quad_request_url);

                curl_http_quad(g_cloud_dev->product_key, g_cloud_dev->device_name, "/iot/quadruple/apply", NULL);
                sleep(2);
            }
            if (strlen(g_cloud_dev->device_secret) > 0)
            {
                link_main(g_cloud_dev->product_key, g_cloud_dev->product_secret, g_cloud_dev->device_name, g_cloud_dev->device_secret, g_cloud_dev->software_ver);
                break;
            }
            else
            {
                dzlog_warn("curl_http_get_quad is fail");
                sleep(2);
            }
        }
        else
        {
            dzlog_warn("no Internet...");
            sleep(2);
        }
    } while (1);
#else
    link_main(g_cloud_dev->product_key, g_cloud_dev->product_secret, g_cloud_dev->device_name, g_cloud_dev->device_secret, g_cloud_dev->software_ver);
#endif
    return NULL;
}
#include "main.h"

#include "uds_protocol.h"
#include "tcp_uds_server.h"

#include "uart_ecb_parse.h"
#include "linkkit_solo.h"
#include "uart_cloud_task.h"
#include "database_task.h"

static pthread_mutex_t mutex;
static cloud_dev_t *g_cloud_dev = NULL;

cloud_dev_t *get_cloud_dev(void)
{
    return g_cloud_dev;
}
// unsigned char get_BuzControl(void)
// {
//     cloud_dev_t *cloud_dev = g_cloud_dev;
//     cloud_attr_t *attr = cloud_dev->attr;

//     for (int i = 0; i < cloud_dev->attr_len; ++i)
//     {
//         if (strcmp("BuzControl", attr->cloud_key) == 0)
//         {
//             return *(attr->value);
//         }
//     }
//     return 0;
// }
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

int get_attr_report_value(cJSON *resp, cloud_attr_t *ptr) //把串口上报数据解析，并拼包成JSON
{
    if (ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT_CTRL && ptr->cloud_fun_type != LINK_FUN_TYPE_ATTR_REPORT)
    {
        return -1;
    }
    cJSON *item = NULL;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        if (ptr->uart_cmd == UART_CMD_MULTISTAGE_STATE)
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
        for (int i = 0; i < ptr->uart_byte_len; ++i)
        {
            cloud_val = (cloud_val << 8) + ptr->value[i];
        }
        if (LINK_VALUE_TYPE_STRING_NUM == ptr->cloud_value_type)
        {
            char buf[16];
            sprintf(buf, "%d", cloud_val);
            item = cJSON_CreateString(buf);
        }
        else if (LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
        {
            // if (strcmp("SysPower", ptr->cloud_key) == 0)
            //     cloud_val = 1;
            item = cJSON_CreateNumber(cloud_val);
        }
        else if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
        {
            if (ptr->uart_byte_len > 1 && ptr->value[ptr->uart_byte_len - 1] != 0)
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
                else if (strcmp("ElcSWVersion", ptr->cloud_key) == 0)
                {
                    char buf[6];
                    sprintf(buf, "%d.%d", *ptr->value >> 4, *ptr->value & 0x0f);
                    item = cJSON_CreateString(buf);
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
        if (ptr->uart_cmd == UART_CMD_MULTISTAGE_SET)
        {
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
            if (strcmp("MultiStageContent", ptr->cloud_key) == 0)
            {
                dzlog_warn("get_attr_set_value remind size:%d", sizeof(g_cloud_dev->remind));
                memset(g_cloud_dev->remind, 0, sizeof(g_cloud_dev->remind));
            }
            else
            {
            }
            dzlog_warn("get_attr_set_value %s:%d %d", ptr->cloud_key, ptr->uart_byte_len, index);
        }
        else if (strcmp(ptr->cloud_key, "CookbookName") == 0)
        {
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, ptr->cloud_key, item);
            char *json = cJSON_PrintUnformatted(resp);
            linkkit_user_post_property(json);
            cJSON_free(json);
            send_event_uds(resp, NULL);
            return 0;
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
            memcpy(&out[1], item->valuestring, strlen(item->valuestring));
            goto end;
        }
        else
        {
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
                    memcpy(attr[j].value, &value[i + 1], attr[j].uart_byte_len);
                    dzlog_debug("i:%d cloud_key:%s", i, attr[j].cloud_key);
                    hdzlog_info((unsigned char *)attr[j].value, attr[j].uart_byte_len);
                    get_attr_report_value(root, &attr[j]);
                    i += attr[j].uart_byte_len;
                    break;
                }
            }
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    linkkit_user_post_property(json); //阿里云平台上报接口
    cJSON_free(json);

    send_event_uds(root, NULL); //UI上报接口
    // cJSON_Delete(root);
}

int send_all_to_cloud(void) //发送所有属性给阿里云平台，用于刚建立连接
{
    dzlog_info("send_all_to_cloud");
    cloud_dev_t *cloud_dev = g_cloud_dev;
    cloud_attr_t *attr = cloud_dev->attr;

    cJSON *root = cJSON_CreateObject();

    for (int i = 0; i < cloud_dev->attr_len; ++i)
    {
        get_attr_report_value(root, &attr[i]);
    }
    char *json = cJSON_PrintUnformatted(root);
    linkkit_user_post_property(json);
    cJSON_free(json);
    cJSON_Delete(root);

    root = cJSON_CreateObject();
    database_resp_getall(NULL, root);

    json = cJSON_PrintUnformatted(root);
    linkkit_user_post_property(json);
    cJSON_free(json);
    cJSON_Delete(root);
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
    static unsigned char uart_buf[256];
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
        cloud_to_uart_ecb_msg(uart_buf, uart_buf_len);
    }

    cJSON *resp_db = cJSON_CreateObject();
    database_resp_set(root, resp_db);
    char *json = cJSON_PrintUnformatted(resp_db);
    linkkit_user_post_property(json);
    cJSON_free(json);
    send_event_uds(resp_db, NULL);

    pthread_mutex_unlock(&mutex);
    return 0;
}

static int recv_data_from_cloud(const int devid, const char *value, const int value_len) //阿里云下发接口回调函数，初始化时注册
{
    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        dzlog_error("JSON Parse Error");
        return -1;
    }
    cloud_resp_set(root, NULL);

    cJSON_Delete(root);
    return 0;
}
static void *cloud_quad_parse_json(void *input, const char *str) //启动时解析四元组文件
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }

    cJSON *ProductKey = cJSON_GetObjectItem(root, "ProductKey");
    if (ProductKey == NULL)
    {
        dzlog_error("ProductKey is NULL\n");
        goto fail;
    }
    cJSON *ProductSecret = cJSON_GetObjectItem(root, "ProductSecret");
    if (ProductSecret == NULL)
    {
        dzlog_error("ProductSecret is NULL\n");
        goto fail;
    }
    cJSON *DeviceName = cJSON_GetObjectItem(root, "DeviceName");
    if (DeviceName == NULL)
    {
        dzlog_error("DeviceName is NULL\n");
        goto fail;
    }
    cJSON *DeviceSecret = cJSON_GetObjectItem(root, "DeviceSecret");
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

void get_dev_version(char *hardware_ver, char *software_ver) //获取软件版本号
{
    if (hardware_ver)
        strcpy(hardware_ver, g_cloud_dev->hardware_ver);
    if (software_ver)
        strcpy(software_ver, g_cloud_dev->software_ver);
}

int cloud_init(void) //初始化
{
    pthread_mutex_init(&mutex, NULL);

    register_property_set_event_cb(recv_data_from_cloud); //注册阿里云下发回调
    register_property_report_all_cb(send_all_to_cloud);   //注册阿里云连接回调
    g_cloud_dev = get_dev_profile(".", NULL, PROFILE_NAME, cloud_parse_json);
    if (g_cloud_dev == NULL)
    {
        dzlog_error("cloud_init error\n");
        return -1;
    }

    strcpy(g_cloud_dev->software_ver, SOFTER_VER);
    if (get_dev_profile(".", g_cloud_dev, QUAD_NAME, cloud_quad_parse_json) == NULL)
    {
        dzlog_error("cloud_init cloud_quad_parse_json error\n");
        return -1;
    }

    return 0;
}

void cloud_deinit(void) //反初始化
{
    linkkit_close();
    for (int i = 0; i < g_cloud_dev->attr_len; ++i)
        free(g_cloud_dev->attr[i].value);
    free(g_cloud_dev->attr);
    free(g_cloud_dev);
    dzlog_warn("cloud_deinit...........\n");
    pthread_mutex_destroy(&mutex);
}
void *cloud_task(void *arg) //云端任务
{
    linkkit_main(g_cloud_dev->product_key, g_cloud_dev->product_secret, g_cloud_dev->device_name, g_cloud_dev->device_secret);
    return NULL;
}
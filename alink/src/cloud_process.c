#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"
#include "logFunc.h"
#include "commonFunc.h"
#include "networkFunc.h"

#include "linkkit_solo.h"
#include "cloud_process.h"
#include "tcp_uds_server.h"

static cloud_dev_t *g_cloud_dev = NULL;

cloud_dev_t *get_cloud_dev(void)
{
    return g_cloud_dev;
}

int (*send_data_to_ecb)(unsigned char *, const int);
void register_send_data_to_ecb_cb(int (*cb)(unsigned char *, const int))
{
    send_data_to_ecb = cb;
}

cJSON *get_attr_report_value(cloud_attr_t *ptr)
{
    cJSON *item = NULL;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        if (ptr->uart_cmd == 78)
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
            item = cJSON_CreateNumber(cloud_val);
        }
        else if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
        {
            if (ptr->value[ptr->uart_byte_len - 1] != 0)
            {
                char *buf = (char *)malloc(ptr->uart_byte_len + 1);
                memcpy(buf, ptr->value, ptr->uart_byte_len);
                ptr->value[ptr->uart_byte_len - 1] = 0;
                item = cJSON_CreateString(buf);
                free(buf);
            }
            else
                item = cJSON_CreateString(ptr->value);
        }
        else
        {
        }
    }
    return item;
}

int get_attr_set_value(cloud_attr_t *ptr, cJSON *item, unsigned char *out)
{
    long num = 0;
    if (out == NULL)
        return 0;
    if (LINK_VALUE_TYPE_STRUCT == ptr->cloud_value_type)
    {
        int index = 0;
        if (ptr->uart_cmd == 77)
        {
            int arraySize = cJSON_GetArraySize(item);
            if (arraySize == 0)
            {
                MLOG_ERROR("get_attr_set_value arraySize is 0\n");
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
                MLOG_WARN("get_attr_set_value remind size:%d", sizeof(g_cloud_dev->remind));
                memset(g_cloud_dev->remind, 0, sizeof(g_cloud_dev->remind));
            }
            else
            {
            }
            MLOG_WARN("get_attr_set_value %s:%d %d", ptr->cloud_key, ptr->uart_byte_len, index);
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

    return ptr->uart_byte_len + 1;
}

void send_data_to_cloud(const unsigned char *value, const int value_len)
{
    MLOG_HEX("send_data_to_cloud:", (unsigned char *)value, value_len);
    int i, j;
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cloud_attr_t *attr = NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *item = NULL;
    if (value != NULL)
    {
        for (i = 0; i < value_len; ++i)
        {
            for (j = 0; j < cloud_dev->attr_len; ++j)
            {
                attr = &cloud_dev->attr[j];
                if (value[i] == attr->uart_cmd)
                {
                    if (attr->value != NULL)
                    {
                        memcpy(attr->value, &value[i + 1], attr->uart_byte_len);
                        MLOG_DEBUG("i:%d cloud_key:%s", i, attr->cloud_key);
                        MLOG_HEX("attr value:", (unsigned char *)attr->value, attr->uart_byte_len);
                        item = get_attr_report_value(attr);
                        if (item != NULL)
                            cJSON_AddItemToObject(root, attr->cloud_key, item);
                    }
                    i += attr->uart_byte_len;
                    break;
                }
            }
        }
    }
    else
    {
        for (j = 0; j < cloud_dev->attr_len; ++j)
        {
            attr = &cloud_dev->attr[j];
            if (attr->value != NULL && (attr->cloud_fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || attr->cloud_fun_type == LINK_FUN_TYPE_ATTR_REPORT))
            {
                item = get_attr_report_value(attr);
                if (item != NULL)
                    cJSON_AddItemToObject(root, attr->cloud_key, item);
            }
        }
    }
    if (!cJSON_IsNull(root))
    {
        char *json = cJSON_PrintUnformatted(root);
        linkkit_user_post_property(json);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

int send_all_to_cloud(void)
{
    MLOG_INFO("recv_all_to_cloud");
    send_data_to_cloud(NULL, 0);
    return 0;
}

static int recv_data_from_cloud(const int devid, const char *value, const int value_len)
{
    static unsigned char uart_buf[256];
    int uart_buf_len = 0;

    int j;
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cloud_attr_t *ptr = NULL;

    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        MLOG_ERROR("JSON Parse Error");
        return -1;
    }

    for (j = 0; j < cloud_dev->attr_len; ++j)
    {
        ptr = &cloud_dev->attr[j];
        if ((ptr->cloud_fun_type == LINK_FUN_TYPE_ATTR_REPORT_CTRL || ptr->cloud_fun_type == LINK_FUN_TYPE_ATTR_CTRL) && cJSON_HasObjectItem(root, ptr->cloud_key))
        {
            cJSON *item = cJSON_GetObjectItem(root, ptr->cloud_key);
            uart_buf_len += get_attr_set_value(ptr, item, &uart_buf[uart_buf_len]);
        }
    }
    cJSON_Delete(root);
    if (uart_buf_len > 0)
    {
        if (send_data_to_ecb != NULL)
            send_data_to_ecb(uart_buf, uart_buf_len);
    }
    return 0;
}

static int recv_data_from_uds(char *value, unsigned int value_len)
{
    return recv_data_from_cloud(0,value,value_len);
}

static void *cloud_quad_parse_json(void *input, const char *str)
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }

    cJSON *ProductKey = cJSON_GetObjectItem(root, "ProductKey");
    if (ProductKey == NULL)
    {
        MLOG_ERROR("ProductKey is NULL\n");
        goto fail;
    }
    cJSON *ProductSecret = cJSON_GetObjectItem(root, "ProductSecret");
    if (ProductSecret == NULL)
    {
        MLOG_ERROR("ProductSecret is NULL\n");
        goto fail;
    }
    cJSON *DeviceName = cJSON_GetObjectItem(root, "DeviceName");
    if (DeviceName == NULL)
    {
        MLOG_ERROR("DeviceName is NULL\n");
        goto fail;
    }
    cJSON *DeviceSecret = cJSON_GetObjectItem(root, "DeviceSecret");
    if (DeviceSecret == NULL)
    {
        MLOG_ERROR("DeviceSecret is NULL\n");
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
static void *cloud_parse_json(void *input, const char *str)
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }

    cJSON *DeviceType = cJSON_GetObjectItem(root, "DeviceType");
    if (DeviceType == NULL)
    {
        MLOG_ERROR("DeviceType is NULL\n");
        goto fail;
    }
    cJSON *HardwareVer = cJSON_GetObjectItem(root, "HardwareVer");
    if (HardwareVer == NULL)
    {
        MLOG_ERROR("HardwareVer is NULL\n");
        goto fail;
    }

    cJSON *SoftwareVer = cJSON_GetObjectItem(root, "SoftwareVer");
    if (SoftwareVer == NULL)
    {
        MLOG_ERROR("SoftwareVer is NULL\n");
        goto fail;
    }

    cJSON *attr = cJSON_GetObjectItem(root, "attr");
    if (attr == NULL)
    {
        MLOG_ERROR("attr is NULL\n");
        goto fail;
    }

    int arraySize = cJSON_GetArraySize(attr);
    if (arraySize == 0)
    {
        MLOG_ERROR("attr arraySize is 0\n");
        goto fail;
    }
    int i;
    cloud_dev_t *cloud_dev = (cloud_dev_t *)malloc(sizeof(cloud_dev_t));
    cloud_dev->attr_len = arraySize;
    cloud_dev->attr = (cloud_attr_t *)malloc(sizeof(cloud_attr_t) * cloud_dev->attr_len);
    memset(cloud_dev->attr, 0, sizeof(cloud_attr_t) * cloud_dev->attr_len);

    strcpy(cloud_dev->device_type, DeviceType->valuestring);
    cloud_dev->hardware_ver = HardwareVer->valueint;
    cloud_dev->software_ver = SoftwareVer->valueint;

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

void get_dev_version(char *hardware_ver, char *software_ver)
{
    if (hardware_ver)
        sprintf(hardware_ver, "%d", g_cloud_dev->hardware_ver);
    if (software_ver)
        sprintf(software_ver, "%d", g_cloud_dev->software_ver);
}

int cloud_init(void)
{
    register_uds_recv_cb(recv_data_from_uds);
    register_property_set_event_cb(recv_data_from_cloud);
    register_property_report_all_cb(send_all_to_cloud);
    g_cloud_dev = get_dev_profile(PROFILE_PATH, NULL, PROFILE_NAME, cloud_parse_json);
    if (g_cloud_dev == NULL)
    {
        MLOG_ERROR("cloud_init error\n");
        return -1;
    }
    if (get_dev_profile(PROFILE_PATH, g_cloud_dev, QUAD_NAME, cloud_quad_parse_json) == NULL)
    {
        MLOG_ERROR("cloud_init cloud_quad_parse_json error\n");
        return -1;
    }
    return 0;
}

void cloud_deinit(void)
{
    for (int i = 0; i < g_cloud_dev->attr_len; ++i)
        free(g_cloud_dev->attr[i].value);
    free(g_cloud_dev->attr);
    free(g_cloud_dev);
}

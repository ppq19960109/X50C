#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"
#include "list.h"
#include "logFunc.h"
#include "commonFunc.h"

#include "linkkit_solo.h"
#include "cloud.h"

#include "ecb_parse.h"

enum LINK_VALUE_TYPE
{
    LINK_VALUE_TYPE_NUM = 0x00,
    LINK_VALUE_TYPE_STRING,
};
enum LINK_FUN_TYPE
{
    LINK_FUN_TYPE_ATTR_REPORT_CTRL = 0x00,
    LINK_FUN_TYPE_ATTR_REPORT,
    LINK_FUN_TYPE_ATTR_CTRL,
    LINK_FUN_TYPE_EVENT,
    LINK_FUN_TYPE_SERVICE,
};

typedef struct
{
    char cloud_key[33];
    enum LINK_VALUE_TYPE cloud_value_type;
    enum LINK_FUN_TYPE cloud_fun_type;
    unsigned char uart_cmd;
    unsigned char uart_byte_len;
    char *value;
    struct list_head node;
} cloud_attr_t;

typedef struct
{
    char product_key[33];
    char product_secret[33];
    char device_name[33];
    char device_secret[33];
    cloud_attr_t *attr;
    int attr_len;
} cloud_dev_t;

static cloud_dev_t *g_cloud_dev = NULL;

cloud_dev_t *get_cloud_dev(void)
{
    return g_cloud_dev;
}
cJSON *get_attr_report_value(cloud_attr_t *ptr)
{
    cJSON *item = NULL;
    int cloud_val = 0;
    for (int i = 0; i < ptr->uart_byte_len; ++i)
    {
        cloud_val = (cloud_val << 8) + ptr->value[i];
    }
    if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
    {
        char buf[16];
        sprintf(buf, "%d", cloud_val);
        item = cJSON_CreateString(buf);
    }
    else if (LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
    {
        item = cJSON_CreateNumber(cloud_val);
    }
    else
    {
    }
    return item;
}

int get_attr_set_value(cloud_attr_t *ptr, cJSON *item, unsigned char *out)
{
    long num = 0;
    if (out == NULL)
        return 0;
    if (LINK_VALUE_TYPE_STRING == ptr->cloud_value_type)
    {
        if (stol(item->valuestring, 10, &num) == NULL)
            return 0;
    }
    else if (LINK_VALUE_TYPE_NUM == ptr->cloud_value_type)
    {
        num = item->valueint;
    }
    else
    {
    }
    out[0] = ptr->uart_cmd;
    for (int i = 0; i < ptr->uart_byte_len; ++i)
    {
        out[1 + i] = num >> (8 * (ptr->uart_byte_len - 1 - i));
    }
    return ptr->uart_byte_len + 1;
}

void send_data_to_cloud(const unsigned char cmd, const unsigned char *value, const int value_len)
{
    MLOG_HEX("send_data_to_cloud:", value, value_len);
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
                        MLOG_HEX("attr value:", attr->value, attr->uart_byte_len);
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
    send_data_to_cloud(0, NULL, 0);
    return 0;
}

static int recv_data_from_cloud(const int devid, const char *value, const int value_len)
{
    static unsigned char uart_buf[128];
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
        ecb_uart_send_msg(ECB_UART_COMMAND_SET, uart_buf_len, uart_buf);
    return 0;
}

static void *cloud_parse_json(const char *devId, const char *str)
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

    strcpy(cloud_dev->product_key, ProductKey->valuestring);
    strcpy(cloud_dev->product_secret, ProductSecret->valuestring);
    strcpy(cloud_dev->device_name, DeviceName->valuestring);
    strcpy(cloud_dev->device_secret, DeviceSecret->valuestring);

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
            cloud_dev->attr[i].value = (char *)malloc(cloud_dev->attr[i].uart_byte_len);
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
int cloud_init(void)
{
    register_property_set_event_cb(recv_data_from_cloud);
    register_property_report_all_cb(send_all_to_cloud);
    g_cloud_dev = get_dev_profile(PROFILE_PATH, NULL, PROFILE_NAME, cloud_parse_json);
    if (g_cloud_dev == NULL)
    {
        MLOG_ERROR("cloud_init error\n");
        return -1;
    }
    return 0;
}

void cloud_deinit()
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    free(cloud_dev->attr);
    free(cloud_dev);
}

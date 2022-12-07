#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "wifi_task.h"
#include "database.h"
#include "database_task.h"
#include "ota_task.h"
#include "device_task.h"

static pthread_mutex_t mutex;
static char g_send_buf[2048];
static int g_seqid = 0;

unsigned char CheckSum(unsigned char *buf, int len) //和校验算法
{
    int i;
    unsigned char ret = 0;
    for (i = 0; i < len; i++)
    {
        ret += *(buf++);
    }
    return ret;
}

int send_event_uds(cJSON *send, const char *type) // uds发送u接口
{
    if (cJSON_Object_isNull(send))
    {
        cJSON_Delete(send);
        // dzlog_warn("%s,send NULL", __func__);
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    if (type == NULL)
        cJSON_AddStringToObject(root, TYPE, TYPE_EVENT);
    else
        cJSON_AddStringToObject(root, TYPE, type);

    cJSON_AddItemToObject(root, DATA, send);
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL)
    {
        dzlog_error("%s,cJSON_PrintUnformatted error", __func__);
        cJSON_Delete(root);
        return -1;
    }
    int len = strlen(json);

    printf("send to UI-------------------------- cJSON_PrintUnformatted json:%d,%s\n", len, json);
    pthread_mutex_lock(&mutex);
    char *send_buf;
    if (len + 10 > sizeof(g_send_buf))
    {
        send_buf = (char *)malloc(len + 10);
    }
    else
    {
        send_buf = g_send_buf;
    }
    send_buf[0] = FRAME_HEADER;
    send_buf[1] = FRAME_HEADER;
    send_buf[2] = 0;
    send_buf[3] = g_seqid / 256;
    send_buf[4] = g_seqid % 256;
    send_buf[5] = len >> 8;
    send_buf[6] = len;
    memcpy(&send_buf[7], json, len);
    send_buf[7 + len] = CheckSum((unsigned char *)&send_buf[2], len + 5);
    send_buf[8 + len] = FRAME_TAIL;
    send_buf[9 + len] = FRAME_TAIL;

    app_select_client_tcp_server_send(send_buf, len + 10);

    if (len + 10 > sizeof(g_send_buf))
    {
        free(send_buf);
    }

    cJSON_free(json);
    cJSON_Delete(root);
    pthread_mutex_unlock(&mutex);
    return 0;
}

static int uds_json_parse(char *value, unsigned int value_len) // uds接受的json数据解析
{
    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        dzlog_error("JSON Parse Error");
        return -1;
    }

    char *json = cJSON_PrintUnformatted(root);
    dzlog_debug("recv from UI-------------------------- json:%s", json);
    cJSON_free(json);

    cJSON *Type = cJSON_GetObjectItem(root, TYPE);
    if (Type == NULL)
    {
        dzlog_error("Type is NULL\n");
        goto fail;
    }

    cJSON *Data = cJSON_GetObjectItem(root, DATA);
    if (Data == NULL)
    {
        dzlog_error("Data is NULL\n");
        goto fail;
    }
    cJSON *resp = cJSON_CreateObject();           //创建返回数据
    if (strcmp(TYPE_GET, Type->valuestring) == 0) //解析GET命令
    {
        wifi_resp_get(Data, resp);
        cloud_resp_get(Data, resp);
        ota_resp_get(Data, resp);
        device_resp_get(Data, resp);
        database_resp_get(Data, resp);
    }
    else if (strcmp(TYPE_SET, Type->valuestring) == 0) //解析SET命令
    {
        wifi_resp_set(Data, resp);
        cloud_resp_set(Data, resp);
        ota_resp_set(Data, resp);
        device_resp_set(Data, resp);
        // database_resp_set(Data, resp);
    }
    else if (strcmp(TYPE_GETALL, Type->valuestring) == 0) //解析GETALL命令
    {
        wifi_resp_getall(Data, resp);
        cloud_resp_getall(Data, resp);
        ota_resp_getall(Data, resp);
        device_resp_getall(Data, resp);

        cJSON *resp = cJSON_CreateObject();
        database_resp_getall(Data, resp);
        send_event_uds(resp, NULL);
    }
    else //解析HEART命令
    {
        cJSON_AddNullToObject(resp, "Response");
        send_event_uds(resp, TYPE_HEART);
        goto heart;
    }
    send_event_uds(resp, NULL); //发送返回数据
heart:
    cJSON_Delete(root);
    return 0;
fail:
    cJSON_Delete(root);
    return -1;
}

int uds_event_all(void)
{
    cJSON *resp = cJSON_CreateObject();
    wifi_resp_getall(NULL, resp);
    cloud_resp_getall(NULL, resp);
    ota_resp_getall(NULL, resp);
    device_resp_getall(NULL, resp);
    send_event_uds(resp, NULL);

    resp = cJSON_CreateObject();
    database_resp_getall(NULL, resp);
    send_event_uds(resp, NULL);
    return 0;
}

static int uds_recv(char *bytes, unsigned int len) // uds接受回调函数，初始化时注册
{
    if (bytes == NULL)
        return -1;
    unsigned char *data = (unsigned char *)bytes;
    unsigned short msg_len;
    int seqid;
    // int encry;
    unsigned char verify;
    // hdzlog_info(data, len);
    for (int i = 0; i < len; ++i)
    {
        if (data[i] == FRAME_HEADER && data[i + 1] == FRAME_HEADER)
        {
            // encry = data[i + 2];
            seqid = (data[i + 3] << 8) + data[i + 4];
            msg_len = (data[i + 5] << 8) + data[i + 6];
            if (data[i + 6 + msg_len + 2] != FRAME_TAIL || data[i + 6 + msg_len + 3] != FRAME_TAIL)
            {
                dzlog_error("tail error");
                continue;
            }
            // hdzlog_info(&data[i], 6 + msg_len + 4);
            dzlog_debug("uds_recv seqid:%d msg_len:%d", seqid, msg_len);
            verify = data[i + 6 + msg_len + 1];
            unsigned char verify_check = CheckSum(&data[i + 2], msg_len + 5);
            if (verify_check != verify)
            {
                dzlog_error("CheckSum error:%d,%d", verify_check, verify);
                continue;
            }
            if (msg_len > 0)
            {
                if (uds_json_parse((char *)&data[i + 6 + 1], msg_len) == 0)
                {
                    i += 6 + msg_len + 3;
                }
            }
        }
    }
    return 0;
}

int uds_protocol_init(void) // uds协议相关初始化
{
    pthread_mutex_init(&mutex, NULL);
    device_task_init();
    wifi_task_init();
    ota_task_init();
    database_init();
    database_task_init();
    register_uds_recv_cb(uds_recv);
    return 0;
}
void uds_protocol_deinit(void) // uds协议相关反初始化
{
    uds_tcp_server_task_deinit();
    database_deinit();
    ota_task_deinit();
    pthread_mutex_destroy(&mutex);
}
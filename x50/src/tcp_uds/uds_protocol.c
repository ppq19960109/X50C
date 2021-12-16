#include "main.h"

#include "uds_protocol.h"
#include "tcp_uds_server.h"

#include "uart_cloud_task.h"
#include "wifi_task.h"
#include "database.h"
#include "database_task.h"
#include "ota_task.h"
#include "device_task.h"


static pthread_mutex_t mutex;
static char g_send_buf[4096];
static int g_seqid = 0;

unsigned char CheckSum(unsigned char *buf, int len)
{
    int i;
    unsigned char ret = 0;
    for (i = 0; i < len; i++)
    {
        ret += *(buf++);
    }
    return ret;
}

int cJSON_Object_isNull(cJSON *object)
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

int send_event_uds(cJSON *send)
{
    pthread_mutex_lock(&mutex);
    if (cJSON_Object_isNull(send))
    {
        cJSON_Delete(send);
        dzlog_warn("%s,send NULL", __func__);
        goto fail;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, TYPE, TYPE_EVENT);
    cJSON_AddItemToObject(root, DATA, send);
    char *json = cJSON_PrintUnformatted(root);
    int len = strlen(json);
    dzlog_debug("%d,%s", len, json);
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
    send_buf[7 + len] = 0;
    send_buf[8 + len] = FRAME_TAIL;
    send_buf[9 + len] = FRAME_TAIL;

    select_tcp_server_send(send_buf, len + 10);

    if (len + 10 > sizeof(g_send_buf))
    {
        free(send_buf);
    }

    cJSON_free(json);
    cJSON_Delete(root);
fail:
    pthread_mutex_unlock(&mutex);
    return 0;
}

static int uds_json_parse(char *value, unsigned int value_len)
{
    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        dzlog_error("JSON Parse Error");
        return -1;
    }

    char *json = cJSON_PrintUnformatted(root);
    dzlog_debug("json:%s", json);
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
    cJSON *resp = cJSON_CreateObject();
    if (strcmp(TYPE_GET, Type->valuestring) == 0)
    {
        wifi_resp_get(Data, resp);
        database_resp_get(Data, resp);
        cloud_resp_get(Data, resp);
        ota_resp_get(Data, resp);
        device_resp_get(Data, resp);
    }
    else if (strcmp(TYPE_SET, Type->valuestring) == 0)
    {
        wifi_resp_set(Data, resp);
        database_resp_set(Data, resp);
        cloud_resp_set(Data, resp);
        ota_resp_set(Data, resp);
        device_resp_set(Data, resp);
    }
    else
    {
        wifi_resp_getall(Data, resp);
        database_resp_getall(Data, resp);
        cloud_resp_getall(Data, resp);
        ota_resp_getall(Data, resp);
        device_resp_getall(Data, resp);
    }
    send_event_uds(resp);

    cJSON_Delete(root);
    return 0;
fail:
    cJSON_Delete(root);
    return -1;
}

static int uds_recv(char *data, unsigned int len)
{
    if (data == NULL)
        return -1;
    int ret = 0;
    int msg_len, encry, seqid;
    unsigned char verify;
    for (int i = 0; i < len; ++i)
    {
        if (data[i] == FRAME_HEADER && data[i + 1] == FRAME_HEADER)
        {
            encry = data[i + 2];
            seqid = (data[i + 3] << 8) + data[i + 4];
            msg_len = (data[i + 5] << 8) + data[i + 6];
            if (data[i + 6 + msg_len + 2] != FRAME_TAIL || data[i + 6 + msg_len + 3] != FRAME_TAIL)
            {
                continue;
            }
            verify = data[6 + msg_len + 1];
            dzlog_debug("uds_recv msg_len:%d", msg_len);
            if (msg_len > 0)
            {
                ret = uds_json_parse(&data[i + 6 + 1], msg_len);
                if (ret == 0)
                {
                    i += 6 + msg_len + 3;
                }
            }
        }
    }
    return 0;
}

int uds_protocol_init(void)
{
    pthread_mutex_init(&mutex, NULL);
    wifi_task_init();
    ota_task_init();
    database_init();

    register_uds_recv_cb(uds_recv);
    return 0;
}
void uds_protocol_deinit(void)
{
    ota_task_deinit();
    select_server_deinit();
    database_deinit();
    pthread_mutex_destroy(&mutex);
}
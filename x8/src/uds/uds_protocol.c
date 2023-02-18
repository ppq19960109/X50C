#include "main.h"

#include "uds_protocol.h"

#include "cloud_platform_task.h"
#include "wifi_task.h"
#include "ota_task.h"
#include "ota_power_task.h"
#include "device_task.h"

static pthread_mutex_t mutex;
static char g_send_buf[2048];
static int g_seqid = 0;
static hio_t *mio = NULL;

unsigned char CheckSum(unsigned char *buf, int len) // 和校验算法
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
    if (mio == NULL)
    {
        LOGW("%s,mio NULL", __func__);
        cJSON_Delete(send);
        return -1;
    }
    if (cJSON_Object_isNull(send))
    {
        cJSON_Delete(send);
        LOGW("%s,send NULL", __func__);
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
        LOGE("%s,cJSON_PrintUnformatted error", __func__);
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

    hio_write(mio, send_buf, len + 10);

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
        LOGE("JSON Parse Error");
        return -1;
    }

    // char *json = cJSON_PrintUnformatted(root);
    // LOGD("recv from UI-------------------------- json:%s", json);
    // cJSON_free(json);
    LOGD("recv from UI--------------------------:%.*s", value_len, value);

    cJSON *Type = cJSON_GetObjectItem(root, TYPE);
    if (Type == NULL)
    {
        LOGE("Type is NULL\n");
        goto fail;
    }

    cJSON *Data = cJSON_GetObjectItem(root, DATA);
    if (Data == NULL)
    {
        LOGE("Data is NULL\n");
        goto fail;
    }
    cJSON *resp = cJSON_CreateObject();           // 创建返回数据
    if (strcmp(TYPE_GET, Type->valuestring) == 0) // 解析GET命令
    {
        wifi_resp_get(Data, resp);
        cloud_resp_get(Data, resp);
        ota_resp_get(Data, resp);
        ota_power_resp_get(Data, resp);
        device_resp_get(Data, resp);
    }
    else if (strcmp(TYPE_SET, Type->valuestring) == 0) // 解析SET命令
    {
        wifi_resp_set(Data, resp);
        cloud_resp_set(Data, resp);
        ota_resp_set(Data, resp);
        ota_power_resp_set(Data, resp);
        device_resp_set(Data, resp);
    }
    else if (strcmp(TYPE_GETALL, Type->valuestring) == 0) // 解析GETALL命令
    {
        wifi_resp_getall(Data, resp);
        cloud_resp_getall(Data, resp);
        ota_resp_getall(Data, resp);
        ota_power_resp_getall(Data, resp);
        device_resp_getall(Data, resp);
    }
    else // 解析HEART命令
    {
        // cJSON_AddNullToObject(resp, "Response");
        // send_event_uds(resp, TYPE_HEART);
        goto heart;
    }
    send_event_uds(resp, NULL); // 发送返回数据
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
    return 0;
}

static int uds_recv(char *data, unsigned int len) // uds接受回调函数，初始化时注册
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
            // mlogHex(&data[i], 6 + msg_len + 4);
            LOGD("uds_recv encry:%d seqid:%d msg_len:%d", encry, seqid, msg_len);
            verify = data[i + 6 + msg_len + 1];
            unsigned char verify_check = CheckSum((unsigned char *)&data[i + 2], msg_len + 5);
            if (verify_check != verify)
            {
                LOGE("CheckSum error:%d,%d", verify_check, verify);
                // continue;
            }
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

static void on_close(hio_t *io)
{
    printf("on_close fd=%d error=%d\n", hio_fd(io), hio_error(io));
    mio = NULL;
}

static void on_recv(hio_t *io, void *buf, int readbytes)
{
    printf("on_recv fd=%d readbytes=%d\n", hio_fd(io), readbytes);
    char localaddrstr[SOCKADDR_STRLEN] = {0};
    char peeraddrstr[SOCKADDR_STRLEN] = {0};
    printf("[%s] <=> [%s]\n",
           SOCKADDR_STR(hio_localaddr(io), localaddrstr),
           SOCKADDR_STR(hio_peeraddr(io), peeraddrstr));
    printf("< %.*s", readbytes, (char *)buf);
    uds_recv((char *)buf, readbytes);
}

static void on_accept(hio_t *io)
{
    printf("on_accept connfd=%d\n", hio_fd(io));
    char localaddrstr[SOCKADDR_STRLEN] = {0};
    char peeraddrstr[SOCKADDR_STRLEN] = {0};
    printf("accept connfd=%d [%s] <= [%s]\n", hio_fd(io),
           SOCKADDR_STR(hio_localaddr(io), localaddrstr),
           SOCKADDR_STR(hio_peeraddr(io), peeraddrstr));
    if (mio != NULL)
        hio_close(io);
    mio = io;
    hio_setcb_close(io, on_close);
    hio_setcb_read(io, on_recv);
    hio_read_start(io);
}
int uds_protocol_init(void) // uds协议相关初始化
{
    pthread_mutex_init(&mutex, NULL);
    device_task_init();
    wifi_task_init();
    ota_task_init();
    ota_power_task_init();

    unlink("/tmp/unix_server.domain");
    hio_t *listenio = hloop_create_tcp_server(g_loop, "/tmp/unix_server.domain", -1, on_accept);
    if (listenio == NULL)
    {
        return -20;
    }
    return 0;
}
void uds_protocol_deinit(void) // uds协议相关反初始化
{
    ota_task_deinit();
    ota_power_task_deinit();
    pthread_mutex_destroy(&mutex);
}
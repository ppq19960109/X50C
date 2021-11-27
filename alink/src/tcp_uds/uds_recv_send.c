#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"
#include "commonFunc.h"
#include "networkFunc.h"
#include "logFunc.h"

#include "uds_recv_send.h"
#include "tcp_uds_server.h"

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
    if (cJSON_Object_isNull(send))
    {
        cJSON_Delete(send);
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, TYPE, TYPE_EVENT);
    cJSON_AddItemToObject(root, DATA, send);
    char *json = cJSON_PrintUnformatted(root);

    select_tcp_server_send(json, strlen(json));

    cJSON_free(json);
    cJSON_Delete(root);
    return 0;
}

static int recv_event_uds(char *value, unsigned int value_len)
{
    cJSON *root = cJSON_Parse(value);
    if (root == NULL)
    {
        MLOG_ERROR("JSON Parse Error");
        return -1;
    }
    cJSON *Type = cJSON_GetObjectItem(root, TYPE);
    if (Type == NULL)
    {
        MLOG_ERROR("Type is NULL\n");
        goto fail;
    }

    cJSON *Data = cJSON_GetObjectItem(root, DATA);
    if (Data == NULL)
    {
        MLOG_ERROR("Data is NULL\n");
        goto fail;
    }

    cJSON_Delete(root);
    return 0;
fail:
    cJSON_Delete(root);
    return -1;
}

void uds_recv_send_init()
{
    register_uds_recv_cb(recv_event_uds);
}
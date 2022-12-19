#ifndef _WIFI_TASK_H_
#define _WIFI_TASK_H_

#include "cJSON.h"

typedef enum {
	RK_WIFI_State_LINK_CONNECTED = 8,
	RK_WIFI_State_LINK_DISCONNECTED
} RK_WIFI_State_t;

int wifi_task_init(void);

int wifi_resp_get(cJSON *root,cJSON *resp);
int wifi_resp_getall(cJSON *root,cJSON *resp);
int wifi_resp_set(cJSON *root,cJSON *resp);
void register_wifi_connected_cb(void (*cb)(int));
void wifi_reset(void);
#endif
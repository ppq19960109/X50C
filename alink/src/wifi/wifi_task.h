#ifndef _WIFI_TASK_H_
#define _WIFI_TASK_H_

#include "cJSON.h"

int wifi_task_init(void);

int wifi_resp_get(cJSON *root,cJSON *resp);
int wifi_resp_getall(cJSON *root,cJSON *resp);
int wifi_resp_set(cJSON *root,cJSON *resp);
#endif
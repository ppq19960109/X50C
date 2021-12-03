#ifndef _OTA_TASK_H_
#define _OTA_TASK_H_

#include "cJSON.h"

int ota_task_init(void);

int ota_resp_get(cJSON *root,cJSON *resp);
int ota_resp_getall(cJSON *root,cJSON *resp);
int ota_resp_set(cJSON *root,cJSON *resp);
#endif
#ifndef _DEVICE_TASK_H_
#define _DEVICE_TASK_H_

#include "cJSON.h"

int device_resp_get(cJSON *root,cJSON *resp);
int device_resp_getall(cJSON *root,cJSON *resp);
int device_resp_set(cJSON *root,cJSON *resp);
#endif
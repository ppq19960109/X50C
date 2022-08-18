#ifndef _OTA_POWER_TASK_H_
#define _OTA_POWER_TASK_H_

#include "cJSON.h"

int ota_power_task_init(void);
void ota_power_task_deinit(void);

int ota_power_resp_get(cJSON *root,cJSON *resp);
int ota_power_resp_getall(cJSON *root,cJSON *resp);
int ota_power_resp_set(cJSON *root,cJSON *resp);

void ota_power_ack(const unsigned char *data);
#endif
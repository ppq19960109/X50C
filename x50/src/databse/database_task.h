#ifndef _DATABASE_TASK_H_
#define _DATABASE_TASK_H_

#include "cJSON.h"

int database_resp_get(cJSON *root,cJSON *resp);
int database_resp_getall(cJSON *root,cJSON *resp);
int database_resp_set(cJSON *root,cJSON *resp);
#endif
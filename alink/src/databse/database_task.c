#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"
#include "commonFunc.h"
#include "networkFunc.h"
#include "logFunc.h"

#include "database.h"
#include "uds_recv_send.h"
#include "tcp_uds_server.h"
#include "wifi_task.h"
#include "database_task.h"

// cook_attr_t g_cook_attr[120];
// int g_cook_attr_len = 0;
int g_history_seqid = 0;

int recipe_select_func(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;
    cJSON *item = (cJSON *)arg;
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "imgUrl", recipe->imgUrl);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddStringToObject(item, "details", recipe->details);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "time", recipe->time);
    cJSON_AddNumberToObject(item, "cookType", recipe->cookType);
    cJSON_AddNumberToObject(item, "cookTime", recipe->cookTime);
    return 0;
}

int histroy_select_func(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;
    cJSON *item = (cJSON *)arg;
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "imgUrl", recipe->imgUrl);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddStringToObject(item, "details", recipe->details);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "time", recipe->time);
    cJSON_AddNumberToObject(item, "cookType", recipe->cookType);
    cJSON_AddNumberToObject(item, "cookTime", recipe->cookTime);
    // g_cook_attr[g_cook_attr_len].id = recipe->id;
    // g_cook_attr[g_cook_attr_len].seqid = recipe->seqid;
    // ++g_cook_attr_len;
    if (recipe->seqid > g_history_seqid)
    {
        g_history_seqid = recipe->seqid;
    }
    return 0;
}
static void *CookRecipe_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateObject();
    select_from_table(RECIPE_TABLE_NAME, recipe_select_func, item);
    return item;
}

static void *CookHistory_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateObject();
    // g_cook_attr_len = 0;
    select_from_table(HISTORY_TABLE_NAME, histroy_select_func, item);

    return item;
}

static void *UpdateRecipe_cb(void *ptr, void *arg)
{

    return NULL;
}

static void *UpdateHistory_cb(void *ptr, void *arg)
{
    cJSON *item = (cJSON *)arg;

    cJSON *id = cJSON_GetObjectItem(item, "id");
    if (id == NULL)
        return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id->valueint);
    if (cJSON_HasObjectItem(item, "collect"))
    {
        cJSON *collect = cJSON_GetObjectItem(item, "collect");
        update_key_to_table(HISTORY_TABLE_NAME, "collect", collect->valueint, id->valueint);
        cJSON_AddNumberToObject(root, "collect", collect->valueint);
    }
    if (cJSON_HasObjectItem(item, "time"))
    {
        cJSON *time = cJSON_GetObjectItem(item, "time");
        update_key_to_table(HISTORY_TABLE_NAME, "time", time->valueint, id->valueint);
        cJSON_AddNumberToObject(root, "time", time->valueint);
    }

    update_key_to_table(HISTORY_TABLE_NAME, "seqid", g_history_seqid + 1, id->valueint);
    cJSON_AddNumberToObject(root, "seqid", g_history_seqid + 1);
    ++g_history_seqid;
    return root;
}

static void *DeleteHistory_cb(void *ptr, void *arg)
{
    cJSON *item = (cJSON *)arg;
    delete_row_from_table(HISTORY_TABLE_NAME, item->valueint);
    item = cJSON_CreateNumber(item->valueint);
    return item;
}

static void *InsertHistory_cb(void *ptr, void *arg)
{
    cJSON *item = (cJSON *)arg;

    cJSON *dishName = cJSON_GetObjectItem(item, "dishName");
    if (dishName == NULL)
        return NULL;
    recipes_t recipe = {0};

    recipe.seqid = g_history_seqid + 1;
    strcpy(recipe.dishName, dishName->valuestring);

    if (cJSON_HasObjectItem(item, "imgUrl"))
    {
        strcpy(recipe.imgUrl, cJSON_GetObjectItem(item, "imgUrl")->valuestring);
    }
    if (cJSON_HasObjectItem(item, "cookSteps"))
    {
        strcpy(recipe.cookSteps, cJSON_GetObjectItem(item, "cookSteps")->valuestring);
    }
    if (cJSON_HasObjectItem(item, "details"))
    {
        strcpy(recipe.details, cJSON_GetObjectItem(item, "details")->valuestring);
    }
    if (cJSON_HasObjectItem(item, "collect"))
    {
        cJSON *collect = cJSON_GetObjectItem(item, "collect");
        recipe.collect = collect->valueint;
    }
    if (cJSON_HasObjectItem(item, "time"))
    {
        cJSON *time = cJSON_GetObjectItem(item, "time");
        recipe.time = time->valueint;
    }
    if (cJSON_HasObjectItem(item, "cookType"))
    {
        recipe.cookType = cJSON_GetObjectItem(item, "cookType")->valueint;
    }
    if (cJSON_HasObjectItem(item, "cookTime"))
    {
        recipe.cookTime = cJSON_GetObjectItem(item, "cookTime")->valueint;
    }
    insert_replace_row_to_table(HISTORY_TABLE_NAME, &recipe);

    cJSON *root = cJSON_CreateObject();
    select_seqid_from_table(HISTORY_TABLE_NAME, g_history_seqid + 1, histroy_select_func, root);
    return root;
}

static set_attr_t g_database_set_attr[] = {
    {
        cloud_key : "CookRecipe",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : CookRecipe_cb
    },
    {
        cloud_key : "CookHistory",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : CookHistory_cb
    },
    {
        cloud_key : "UpdateRecipe",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        cb : UpdateRecipe_cb
    },
    {
        cloud_key : "UpdateHistory",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        cb : UpdateHistory_cb
    },
    {
        cloud_key : "DeleteHistory",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        cb : DeleteHistory_cb
    },
    {
        cloud_key : "InsertHistory",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT_CTRL,
        cb : InsertHistory_cb
    },

};

int database_task_init(void)
{

    return 0;
}

void database_task_deinit(void)
{
}

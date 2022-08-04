#include "main.h"

#include "uds_protocol.h"
#include "uds_tcp_server.h"

#include "cloud_platform_task.h"
#include "database_task.h"
#include "database.h"


int recipe_select_seqid_func(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;
    cJSON *item = (cJSON *)arg;
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "timestamp", recipe->timestamp);
    cJSON_AddNumberToObject(item, "recipeid", recipe->recipeid);
    cJSON_AddNumberToObject(item, "recipeType", recipe->recipeType);
    cJSON_AddNumberToObject(item, "cookPos", recipe->cookPos);

    return 0;
}

int select_for_cookbookID(int cookbookID, char *name, int name_len)
{
    cJSON *root = cJSON_CreateObject();
    select_recipeid_from_table(RECIPE_TABLE_NAME, cookbookID, recipe_select_seqid_func, root);
    if (cJSON_Object_isNull(root))
        goto fail;
    cJSON *dishName = cJSON_GetObjectItem(root, "dishName");
    if (dishName == NULL)
        goto fail;
    strncpy(name, dishName->valuestring, name_len);
    cJSON_Delete(root);
    return 0;
fail:
    cJSON_Delete(root);
    return -1;
}

int recipe_select_func(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;
    cJSON *root = (cJSON *)arg;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "timestamp", recipe->timestamp);
    cJSON_AddNumberToObject(item, "recipeid", recipe->recipeid);
    cJSON_AddNumberToObject(item, "recipeType", recipe->recipeType);
    cJSON_AddNumberToObject(item, "cookPos", recipe->cookPos);

    cJSON_AddItemToArray(root, item);
    return 0;
}

static void *CookRecipe_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateArray();
    select_from_table(RECIPE_TABLE_NAME, recipe_select_func, item);
    return item;
}

static void *UpdateRecipe_cb(void *ptr, void *arg)
{
    return NULL;
}

static set_attr_t g_database_set_attr[] = {
    {
        cloud_key : "CookRecipe",
        fun_type : LINK_FUN_TYPE_ATTR_REPORT,
        cb : CookRecipe_cb
    },
    {
        cloud_key : "UpdateRecipe",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : UpdateRecipe_cb
    },
};

static const int attr_len = sizeof(g_database_set_attr) / sizeof(g_database_set_attr[0]);
static set_attr_t *attr = g_database_set_attr;
int database_resp_get(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_report_uds(resp, &attr[i]);
        }
    }
    return 0;
}

int database_resp_getall(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        set_attr_report_uds(resp, &attr[i]);
    }
    return 0;
}

int database_resp_set(cJSON *root, cJSON *resp)
{
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_ctrl_uds(resp, &attr[i], cJSON_GetObjectItem(root, attr[i].cloud_key));
        }
    }
    return 0;
}

void database_task_reinit(void)
{
    database_reinit();
}

int database_task_init(void)
{
    return 0;
}

#include "main.h"

#include "uds_protocol.h"
#include "tcp_uds_server.h"

#include "uart_cloud_task.h"
#include "database_task.h"
#include "database.h"

#include "history_wrapper.h"

int g_history_seqid = 0;
char *leftWorkMode[] = {"未设定", "经典蒸", "高温蒸", "热风烧烤", "上下加热", "立体热风", "蒸汽烤", "空气炸", "保温烘干"};
int leftWorkModeNumber[] = {0, 1, 2, 35, 36, 38, 40, 42, 72};

char *leftWorkModeFun(int n)
{
    char *mode = NULL;
    switch (n)
    {
    case 1:
        mode = leftWorkMode[1];
        break;
    case 2:
        mode = leftWorkMode[2];
        break;
    case 35:
        mode = leftWorkMode[3];
        break;
    case 36:
        mode = leftWorkMode[4];
        break;
    case 38:
        mode = leftWorkMode[5];
        break;
    case 40:
        mode = leftWorkMode[6];
        break;
    case 42:
        mode = leftWorkMode[7];
        break;
    case 72:
        mode = leftWorkMode[8];
        break;
    default:
        mode = leftWorkMode[0];
        break;
    }
    return mode;
}

int histroy_select_seqid_func(void *data, void *arg)
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
    cJSON_AddNumberToObject(item, "timestamp", recipe->timestamp);
    cJSON_AddNumberToObject(item, "cookType", recipe->cookType);
    cJSON_AddNumberToObject(item, "recipeType", recipe->recipeType);
    cJSON_AddNumberToObject(item, "cookPos", recipe->cookPos);

    wrapper_reportInsertHistory((history_t *)data, 1);
    if (recipe->seqid > g_history_seqid)
    {
        g_history_seqid = recipe->seqid;
    }
    return 0;
}

static int wrapper_histroy_select_cb(history_t *recipe, void *arg)
{
    cJSON *root = (cJSON *)arg;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "imgUrl", recipe->imgUrl);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddStringToObject(item, "details", recipe->details);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "timestamp", recipe->timestamp);
    cJSON_AddNumberToObject(item, "cookType", recipe->cookType);
    cJSON_AddNumberToObject(item, "recipeType", recipe->recipeType);
    cJSON_AddNumberToObject(item, "cookPos", recipe->cookPos);

    cJSON_AddItemToArray(root, item);
    return 0;
}

static int wrapper_history_update_cb(const int id, const int collect, int timestamp)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *UpdateHistory = cJSON_AddObjectToObject(root, "UpdateHistory");
    cJSON_AddNumberToObject(UpdateHistory, "id", id);
    if (collect >= 0)
    {
        update_key_to_table(HISTORY_TABLE_NAME, "collect", collect, id);
        cJSON_AddNumberToObject(UpdateHistory, "collect", collect);
    }
    if (timestamp >= 0)
    {
        update_key_to_table(HISTORY_TABLE_NAME, "timestamp", timestamp, id);
        cJSON_AddNumberToObject(UpdateHistory, "timestamp", timestamp);
    }
    update_key_to_table(HISTORY_TABLE_NAME, "seqid", g_history_seqid + 1, id);
    cJSON_AddNumberToObject(UpdateHistory, "seqid", g_history_seqid + 1);
    ++g_history_seqid;
    dzlog_info("history_update_cb:g_history_seqid:%d\n", g_history_seqid);
    report_msg_all(root);
    wrapper_reportUpdateHistory(id, g_history_seqid, collect, timestamp);
    return 0;
}

static int wrapper_history_delete_cb(int id)
{
    cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(id));

    delete_row_from_table(HISTORY_TABLE_NAME, id);
    wrapper_reportDeleteHistory(id);
    report_msg_all(array);
    return 0;
}

static int wrapper_history_insert_cb(history_t *history)
{
    history->seqid = g_history_seqid + 1;
    insert_replace_row_to_table(HISTORY_TABLE_NAME, (recipes_t *)history);

    cJSON *root = cJSON_CreateObject();
    cJSON *InsertHistory = cJSON_AddObjectToObject(root, "InsertHistory");
    select_seqid_from_table(HISTORY_TABLE_NAME, g_history_seqid + 1, histroy_select_seqid_func, InsertHistory);
    if (!cJSON_Object_isNull(InsertHistory))
        report_msg_all(root);
    return 0;
}

int recipe_select_func(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;
    cJSON *root = (cJSON *)arg;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", recipe->id);
    cJSON_AddNumberToObject(item, "seqid", recipe->seqid);
    cJSON_AddStringToObject(item, "dishName", recipe->dishName);
    cJSON_AddStringToObject(item, "imgUrl", recipe->imgUrl);
    cJSON_AddStringToObject(item, "cookSteps", recipe->cookSteps);
    cJSON_AddStringToObject(item, "details", recipe->details);
    cJSON_AddNumberToObject(item, "collect", recipe->collect);
    cJSON_AddNumberToObject(item, "timestamp", recipe->timestamp);
    cJSON_AddNumberToObject(item, "cookType", recipe->cookType);
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

static void *CookHistory_cb(void *ptr, void *arg)
{
    cJSON *item = cJSON_CreateArray();
    wrapper_selectHistory(item);
    return item;
}

static void *UpdateRecipe_cb(void *ptr, void *arg)
{
    return NULL;
}

static void *UpdateHistory_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;

    cJSON *id = cJSON_GetObjectItem(item, "id");
    if (id == NULL)
        return NULL;

    if (cJSON_HasObjectItem(item, "collect"))
    {
        cJSON *collect = cJSON_GetObjectItem(item, "collect");
        wrapper_updateHistory(id->valueint, collect->valueint);
    }
    return NULL;
}

static void *DeleteHistory_cb(void *ptr, void *arg)
{
    if (NULL == arg)
        return NULL;
    cJSON *item = (cJSON *)arg;
    int itemSize = cJSON_GetArraySize(item);
    if (itemSize == 0)
    {
        dzlog_error("itemSize is 0\n");
        return NULL;
    }
    cJSON *array = cJSON_CreateArray();
    cJSON *arraySub;
    for (int i = 0; i < itemSize; i++)
    {
        arraySub = cJSON_GetArrayItem(item, i);
        if (arraySub == NULL)
            continue;
        delete_row_from_table(HISTORY_TABLE_NAME, arraySub->valueint);
        cJSON_AddItemToArray(array, cJSON_CreateNumber(arraySub->valueint));
        wrapper_reportDeleteHistory(arraySub->valueint);
    }

    return array;
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
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : UpdateRecipe_cb
    },
    {
        cloud_key : "UpdateHistory",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : UpdateHistory_cb
    },
    {
        cloud_key : "DeleteHistory",
        fun_type : LINK_FUN_TYPE_ATTR_CTRL,
        cb : DeleteHistory_cb
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

void cook_history(cJSON *root)
{
    recipes_t recipe = {0};
    cJSON *cookSteps = cJSON_CreateArray();
    int arraySize, i;
    cJSON *arrayItem, *arraySub, *Mode, *Temp, *Timer;
    if (cJSON_HasObjectItem(root, "CookbookName") && cJSON_HasObjectItem(root, "CookbookParam"))
    {
        cJSON *CookbookName = cJSON_GetObjectItem(root, "CookbookName");
        if (select_dishname_from_table(RECIPE_TABLE_NAME, CookbookName->valuestring, &recipe) < 0)
        {
            goto fail;
        }
        goto recipe;
    }
    else if (cJSON_HasObjectItem(root, "MultiStageContent"))
    {
        arrayItem = cJSON_GetObjectItem(root, "MultiStageContent");
        arraySize = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < arraySize; ++i)
        {
            arraySub = cJSON_GetArrayItem(arrayItem, i);
            if (arraySub == NULL)
                continue;
            cJSON *cookStep = cJSON_CreateObject();
            cJSON_AddItemToArray(cookSteps, cookStep);

            Mode = cJSON_GetObjectItem(arraySub, "Mode");
            Temp = cJSON_GetObjectItem(arraySub, "Temp");
            Timer = cJSON_GetObjectItem(arraySub, "Timer");
            cJSON_AddNumberToObject(cookStep, "mode", Mode->valueint);
            cJSON_AddNumberToObject(cookStep, "time", Timer->valueint);
            cJSON_AddNumberToObject(cookStep, "temp", Temp->valueint);
            cJSON_AddNumberToObject(cookStep, "number", i + 1);
            sprintf(&recipe.dishName[strlen(recipe.dishName)], "%s-", leftWorkModeFun(Mode->valueint));
        }
        recipe.dishName[strlen(recipe.dishName) - 1] = 0;
    }
    else if (cJSON_HasObjectItem(root, "LStOvMode") && cJSON_HasObjectItem(root, "LStOvSetTimer") && cJSON_HasObjectItem(root, "LStOvSetTemp"))
    {
        cJSON *cookStep = cJSON_CreateObject();
        cJSON_AddItemToArray(cookSteps, cookStep);

        cJSON *LStOvMode = cJSON_GetObjectItem(root, "LStOvMode");
        cJSON *LStOvSetTimer = cJSON_GetObjectItem(root, "LStOvSetTimer");
        cJSON *LStOvSetTemp = cJSON_GetObjectItem(root, "LStOvSetTemp");
        cJSON_AddNumberToObject(cookStep, "mode", LStOvMode->valueint);
        cJSON_AddNumberToObject(cookStep, "time", LStOvSetTimer->valueint);
        cJSON_AddNumberToObject(cookStep, "temp", LStOvSetTemp->valueint);
        cJSON_AddNumberToObject(cookStep, "number", 0);
        sprintf(recipe.dishName, "%s-%d℃-%d分钟", leftWorkModeFun(LStOvMode->valueint), LStOvSetTemp->valueint, LStOvSetTimer->valueint);
    }
    else if (cJSON_HasObjectItem(root, "RStOvSetTimer") && cJSON_HasObjectItem(root, "RStOvSetTemp"))
    {
        recipe.cookPos = 1;
        cJSON *cookStep = cJSON_CreateObject();
        cJSON_AddItemToArray(cookSteps, cookStep);
        
        cJSON *RStOvSetTimer = cJSON_GetObjectItem(root, "RStOvSetTimer");
        cJSON *RStOvSetTemp = cJSON_GetObjectItem(root, "RStOvSetTemp");
        cJSON_AddNumberToObject(cookStep, "mode", 0);
        cJSON_AddNumberToObject(cookStep, "time", RStOvSetTimer->valueint);
        cJSON_AddNumberToObject(cookStep, "temp", RStOvSetTemp->valueint);
        cJSON_AddNumberToObject(cookStep, "number", 0);
        sprintf(recipe.dishName, "%s-%d℃-%d分钟", "便捷蒸", RStOvSetTemp->valueint, RStOvSetTimer->valueint);
    }
    else
    {
        goto fail;
    }
    char *json = cJSON_PrintUnformatted(cookSteps);
    strcpy(recipe.cookSteps, json);
    cJSON_free(json);
recipe:
    recipe.timestamp = time(NULL);
    wrapper_insertHistory((history_t *)&recipe);
fail:
    cJSON_Delete(cookSteps);
}

int database_resp_set(cJSON *root, cJSON *resp)
{
    cook_history(root);
    for (int i = 0; i < attr_len; ++i)
    {
        if (cJSON_HasObjectItem(root, attr[i].cloud_key))
        {
            set_attr_ctrl_uds(resp, &attr[i], cJSON_GetObjectItem(root, attr[i].cloud_key));
        }
    }
    return 0;
}

int database_histroy_select_cb(void *data, void *arg)
{
    recipes_t *recipe = (recipes_t *)data;

    wrapper_reportInsertHistory((history_t *)data, 0);
    if (recipe->seqid > g_history_seqid)
    {
        g_history_seqid = recipe->seqid;
    }
    return 0;
}
int database_task_init(void)
{
    register_history_select_cb(wrapper_histroy_select_cb);
    register_history_insert_cb(wrapper_history_insert_cb);
    register_history_delete_cb(wrapper_history_delete_cb);
    register_history_update_cb(wrapper_history_update_cb);
    select_from_table(HISTORY_TABLE_NAME, database_histroy_select_cb, NULL);
    wrapper_reportInsertHistory(NULL, 1);
    return 0;
}
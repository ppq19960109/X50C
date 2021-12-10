#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "logFunc.h"
#include "commonFunc.h"

#include "sqlite3.h"

#include "database.h"

typedef struct
{
    sqlite3 *db;

} sqlHandle_t;

sqlHandle_t sqlHandle;

static const char *create_table =
    "CREATE TABLE %s(\
    ID INTEGER PRIMARY KEY AUTOINCREMENT,\
    SEQID INTEGER UNIQUE NOT NULL,\
    DISHNAME TEXT NOT NULL,\
    IMGURL TEXT,\
    DETAILS TEXT,\
    COOKSTEPS TEXT NOT NULL,\
    TIMESTAMP INTEGER,\
    COLLECT INTEGER,\
    COOKTIME INTEGER,\
    COOKTYPE INTEGER,\
    RECIPETYPE INTEGER);";

static const char *sql_select_seqid = "SELECT * FROM %s WHERE SEQID = ?;";
static const char *sql_select_id_seqid = "SELECT ID,SEQID FROM %s;";
static const char *sql_select = "SELECT * FROM %s;";
static const char *sql_insert_replace = "INSERT OR REPLACE INTO %s VALUES (NULL,?,?,?,?,?,?,?,?,?,?);";
static const char *sql_update = "UPDATE %s SET %s = ? WHERE ID = ?;";
static const char *sql_delete = "DELETE FROM %s WHERE ID = ?;";

static const char *sql_clear_table = "DELETE FROM %s;";
static const char *sql_clear_table_seq = "DELETE FROM sqlite_sequence WHERE name = %s;";

int databse_Reset(const char *table_name)
{
    char buf[128];
    sprintf(buf, sql_clear_table, table_name);

    char *errMsg = NULL;
    int rc = sqlite3_exec(sqlHandle.db, buf, NULL, NULL, &errMsg);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_exec errmsg:%s\n", __func__, errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    sprintf(buf, sql_clear_table_seq, table_name);
    rc = sqlite3_exec(sqlHandle.db, buf, NULL, NULL, &errMsg);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_exec errmsg:%s\n", __func__, errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int delete_row_from_table(const char *table_name, const int id)
{
    char buf[128];
    sprintf(buf, sql_delete, table_name);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(sqlHandle.db, buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_finalize(pstmt);
        return -1;
    }

    sqlite3_bind_int(pstmt, 1, id);

    sqlite3_step(pstmt);
    sqlite3_reset(pstmt);

    sqlite3_finalize(pstmt);
    return 0;
}

int update_key_to_table(const char *table_name, const char *key, const int val, const int id)
{
    char buf[128];
    sprintf(buf, sql_update, table_name, key);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(sqlHandle.db, buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_finalize(pstmt);
        return -1;
    }

    sqlite3_bind_int(pstmt, 1, val);
    sqlite3_bind_int(pstmt, 2, id);

    sqlite3_step(pstmt);
    sqlite3_reset(pstmt);

    sqlite3_finalize(pstmt);
    return 0;
}

int insert_replace_row_to_table(const char *table_name, recipes_t *recipes)
{
    char buf[128];
    sprintf(buf, sql_insert_replace, table_name);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(sqlHandle.db, buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_finalize(pstmt);
        return -1;
    }
    sqlite3_bind_int(pstmt, 1, recipes->seqid);
    sqlite3_bind_text(pstmt, 2, recipes->dishName, strlen(recipes->dishName), NULL);
    sqlite3_bind_text(pstmt, 3, recipes->imgUrl, strlen(recipes->imgUrl), NULL);
    sqlite3_bind_text(pstmt, 4, recipes->details, strlen(recipes->details), NULL);
    sqlite3_bind_text(pstmt, 5, recipes->cookSteps, strlen(recipes->cookSteps), NULL);
    sqlite3_bind_int(pstmt, 6, recipes->timestamp);
    sqlite3_bind_int(pstmt, 7, recipes->collect);
    sqlite3_bind_int(pstmt, 8, recipes->cookTime);
    sqlite3_bind_int(pstmt, 9, recipes->cookType);
    sqlite3_bind_int(pstmt, 10, recipes->recipeType);

    sqlite3_step(pstmt);
    sqlite3_reset(pstmt);

    sqlite3_finalize(pstmt);
    return 0;
}

int select_from_table(const char *table_name, int (*select_func)(void *, void *), void *arg)
{
    char buf[128];
    sprintf(buf, sql_select, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(sqlHandle.db, buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_finalize(stmt);
        return -1;
    }

    recipes_t recipes = {0};
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        //一列一列地去读取每一条记录 1表示列
        recipes.id = sqlite3_column_int(stmt, 0);
        recipes.seqid = sqlite3_column_int(stmt, 1);
        strcpy(recipes.dishName, (const char *)sqlite3_column_text(stmt, 2));
        strcpy(recipes.imgUrl, (const char *)sqlite3_column_text(stmt, 3));
        strcpy(recipes.details, (const char *)sqlite3_column_text(stmt, 4));
        strcpy(recipes.cookSteps, (const char *)sqlite3_column_text(stmt, 5));
        recipes.timestamp = sqlite3_column_int(stmt, 6);
        recipes.collect = sqlite3_column_int(stmt, 7);
        recipes.cookTime = sqlite3_column_int(stmt, 8);
        recipes.cookType = sqlite3_column_int(stmt, 9);
        recipes.recipeType = sqlite3_column_int(stmt, 10);

        printf("id:%d dishName:%s imgUrl:%s\n", recipes.id, recipes.dishName, recipes.imgUrl);
        if (select_func != NULL)
            select_func(&recipes, arg);
    }

    return sqlite3_finalize(stmt);
}

int select_seqid_from_table(const char *table_name, const int seqid, int (*select_func)(void *, void *), void *arg)
{
    char buf[128];
    sprintf(buf, sql_select_seqid, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(sqlHandle.db, buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, seqid);
    recipes_t recipes = {0};
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        //一列一列地去读取每一条记录 1表示列
        recipes.id = sqlite3_column_int(stmt, 0);
        recipes.seqid = sqlite3_column_int(stmt, 1);
        strcpy(recipes.dishName, (const char *)sqlite3_column_text(stmt, 2));
        strcpy(recipes.imgUrl, (const char *)sqlite3_column_text(stmt, 3));
        strcpy(recipes.details, (const char *)sqlite3_column_text(stmt, 4));
        strcpy(recipes.cookSteps, (const char *)sqlite3_column_text(stmt, 5));
        recipes.timestamp = sqlite3_column_int(stmt, 6);
        recipes.collect = sqlite3_column_int(stmt, 7);
        recipes.cookTime = sqlite3_column_int(stmt, 8);
        recipes.cookType = sqlite3_column_int(stmt, 9);
        recipes.recipeType = sqlite3_column_int(stmt, 10);

        printf("id:%d dishName:%s imgUrl:%s\n", recipes.id, recipes.dishName, recipes.imgUrl);
        if (select_func != NULL)
            select_func(&recipes, arg);
    }

    return sqlite3_finalize(stmt);
}

static int databse_create_table(const char *table_name)
{
    char buf[512];
    sprintf(buf, create_table, table_name);
    // printf("%s,sql:%s\n", __func__, buf);
    char *errMsg = NULL;
    int rc = sqlite3_exec(sqlHandle.db, buf, NULL, NULL, &errMsg);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_exec errmsg:%s\n", __func__, errMsg);
        sqlite3_free(errMsg);
    }
    return rc;
}
//---------------------------------
static void *recipes_parse_json(void *input, const char *str)
{
    cJSON *root = cJSON_Parse(str);
    if (root == NULL)
    {
        return NULL;
    }
    cJSON *RecipesVersion = cJSON_GetObjectItem(root, "RecipesVersion");
    if (RecipesVersion == NULL)
    {
        MLOG_ERROR("RecipesVersion is NULL\n");
        goto fail;
    }

    cJSON *attr = cJSON_GetObjectItem(root, "recipes");
    if (attr == NULL)
    {
        MLOG_ERROR("attr is NULL\n");
        goto fail;
    }

    int arraySize = cJSON_GetArraySize(attr);
    if (arraySize == 0)
    {
        MLOG_ERROR("attr arraySize is 0\n");
        goto fail;
    }
    int i;
    recipes_t recipes = {0};
    cJSON *arraySub, *seqid, *dishName, *imgUrl, *cookSteps, *details, *cookType, *cookTime, *recipeType;
    for (i = 0; i < arraySize; i++)
    {
        arraySub = cJSON_GetArrayItem(attr, i);
        if (arraySub == NULL)
            continue;

        if (cJSON_HasObjectItem(arraySub, "seqid"))
        {
            seqid = cJSON_GetObjectItem(arraySub, "seqid");
            recipes.seqid = seqid->valueint;
        }
        dishName = cJSON_GetObjectItem(arraySub, "dishName");
        strcpy(recipes.dishName, dishName->valuestring);
        imgUrl = cJSON_GetObjectItem(arraySub, "imgUrl");
        strcpy(recipes.imgUrl, imgUrl->valuestring);
        cookSteps = cJSON_GetObjectItem(arraySub, "cookSteps");
        strcpy(recipes.cookSteps, cookSteps->valuestring);
        details = cJSON_GetObjectItem(arraySub, "details");
        strcpy(recipes.details, details->valuestring);

        cookType = cJSON_GetObjectItem(arraySub, "cookType");
        recipes.cookType = cookType->valueint;
        cookTime = cJSON_GetObjectItem(arraySub, "cookTime");
        recipes.cookTime = cookTime->valueint;
        recipeType = cJSON_GetObjectItem(arraySub, "recipeType");
        recipes.recipeType = recipeType->valueint;

        insert_replace_row_to_table(RECIPE_TABLE_NAME, &recipes);
    }

    cJSON_Delete(root);
    return NULL;
fail:
    cJSON_Delete(root);
    return NULL;
}

void database_deinit(void)
{
    sqlite3_close_v2(sqlHandle.db);
}

int database_init(void)
{
    int rc = sqlite3_open_v2(DB_NAME, &sqlHandle.db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_open_v2 errmsg:%s\n", __func__, sqlite3_errmsg(sqlHandle.db));
        sqlite3_close_v2(sqlHandle.db);
    }
    databse_create_table(HISTORY_TABLE_NAME);
    if (databse_create_table(RECIPE_TABLE_NAME) == 0)
    {
        printf("%s,first databse_create_table RECIPE_TABLE_NAME.....................\n", __func__);
        get_dev_profile(".", NULL, RECIPES_FILE, recipes_parse_json);
    }

    return 0;
}

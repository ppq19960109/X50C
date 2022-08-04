#include "main.h"

#include "sqlite3.h"
#include "database.h"

typedef struct
{
    sqlite3 *recipe_db;
} sqlHandle_t;

sqlHandle_t sqlHandle;

static const char *create_table =
    "CREATE TABLE %s(\
    ID INTEGER PRIMARY KEY AUTOINCREMENT,\
    SEQID INTEGER NOT NULL,\
    DISHNAME TEXT NOT NULL,\
    COOKSTEPS TEXT NOT NULL,\
    TIMESTAMP INTEGER,\
    COLLECT INTEGER,\
    RECIPEID INTEGER,\
    RECIPETYPE INTEGER,\
    COOKPOS INTEGER);";

static const char *sql_select_id = "SELECT * FROM %s WHERE ID = ?;";
static const char *sql_select_seqid = "SELECT * FROM %s WHERE SEQID = ?;";
static const char *sql_select_recipeid = "SELECT * FROM %s WHERE RECIPEID = ?;";
static const char *sql_select_dishname = "SELECT * FROM %s WHERE DISHNAME = ?;";

static const char *sql_select = "SELECT * FROM %s;";
static const char *sql_insert_replace = "INSERT OR REPLACE INTO %s VALUES (NULL,?,?,?,?,?,?,?,?);";
// static const char *sql_insert = "INSERT INTO %s VALUES (NULL,?,?,?,?,?,?,?,?);";
static const char *sql_update = "UPDATE %s SET %s = ? WHERE ID = ?;";
static const char *sql_delete = "DELETE FROM %s WHERE ID = ?;";

static const char *sql_clear_table = "DELETE FROM %s;";
static const char *sql_clear_table_seq = "DELETE FROM sqlite_sequence WHERE name = %s;";
static const char *sql_drop_table = "DROP TABLE %s;";

sqlite3 *get_db_from_table(const char *table_name)
{
    sqlite3 *db = NULL;

    db = sqlHandle.recipe_db;

    return db;
}
int databse_drop_table(const char *table_name)
{
    char buf[128];
    sprintf(buf, sql_drop_table, table_name);

    char *errMsg = NULL;
    int rc = sqlite3_exec(get_db_from_table(table_name), buf, NULL, NULL, &errMsg);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_exec errmsg:%s\n", __func__, errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}
int databse_clear_table(const char *table_name)
{
    char buf[128];
    sprintf(buf, sql_clear_table, table_name);

    char *errMsg = NULL;
    int rc = sqlite3_exec(get_db_from_table(table_name), buf, NULL, NULL, &errMsg);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_exec errmsg:%s\n", __func__, errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    sprintf(buf, sql_clear_table_seq, table_name);
    rc = sqlite3_exec(get_db_from_table(table_name), buf, NULL, NULL, &errMsg);
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
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(pstmt);
        return -1;
    }
    sqlite3_bind_int(pstmt, 1, id);
    sqlite3_step(pstmt);
    // sqlite3_reset(pstmt);
    sqlite3_finalize(pstmt);
    return 0;
}

int update_key_to_table(const char *table_name, const char *key, const int val, const int id)
{
    char buf[128];
    sprintf(buf, sql_update, table_name, key);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(pstmt);
        return -1;
    }
    sqlite3_bind_int(pstmt, 1, val);
    sqlite3_bind_int(pstmt, 2, id);

    sqlite3_step(pstmt);
    // sqlite3_reset(pstmt);
    sqlite3_finalize(pstmt);
    return 0;
}

int update_key_string_table(const char *table_name, const char *key, const char *val, const int id)
{
    char buf[128];
    sprintf(buf, sql_update, table_name, key);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(pstmt);
        return -1;
    }
    sqlite3_bind_text(pstmt, 1, val, strlen(val), NULL);
    sqlite3_bind_int(pstmt, 2, id);

    sqlite3_step(pstmt);
    // sqlite3_reset(pstmt);
    sqlite3_finalize(pstmt);
    return 0;
}

int insert_replace_row_to_table(const char *table_name, recipes_t *recipes)
{
    char buf[128];
    sprintf(buf, sql_insert_replace, table_name);

    sqlite3_stmt *pstmt;
    const char *pzTail;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &pstmt, &pzTail);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(pstmt);
        return -1;
    }
    int index = 1;
    sqlite3_bind_int(pstmt, index++, recipes->seqid);
    sqlite3_bind_text(pstmt, index++, recipes->dishName, strlen(recipes->dishName), NULL);
    sqlite3_bind_text(pstmt, index++, recipes->cookSteps, strlen(recipes->cookSteps), NULL);
    sqlite3_bind_int(pstmt, index++, recipes->timestamp);
    sqlite3_bind_int(pstmt, index++, recipes->collect);
    sqlite3_bind_int(pstmt, index++, recipes->recipeid);
    sqlite3_bind_int(pstmt, index++, recipes->recipeType);
    sqlite3_bind_int(pstmt, index++, recipes->cookPos);

    sqlite3_step(pstmt);
    // sqlite3_reset(pstmt);
    sqlite3_finalize(pstmt);
    return 0;
}

static void select_stmt(recipes_t *recipes, sqlite3_stmt *stmt)
{
    int index = 0;
    //一列一列地去读取每一条记录 1表示列
    recipes->id = sqlite3_column_int(stmt, index++);
    recipes->seqid = sqlite3_column_int(stmt, index++);
    strcpy(recipes->dishName, (const char *)sqlite3_column_text(stmt, index++));
    strcpy(recipes->cookSteps, (const char *)sqlite3_column_text(stmt, index++));
    recipes->timestamp = sqlite3_column_int(stmt, index++);
    recipes->collect = sqlite3_column_int(stmt, index++);
    recipes->recipeid = sqlite3_column_int(stmt, index++);
    recipes->recipeType = sqlite3_column_int(stmt, index++);
    recipes->cookPos = sqlite3_column_int(stmt, index++);
    printf("id:%d dishName:%s\n", recipes->id, recipes->dishName);
}

int select_from_table(const char *table_name, int (*select_func)(void *, void *), void *arg)
{
    char buf[128];
    sprintf(buf, sql_select, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(stmt);
        return -1;
    }

    recipes_t recipes;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        select_stmt(&recipes, stmt);
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
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, seqid);
    recipes_t recipes;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        select_stmt(&recipes, stmt);
        if (select_func != NULL)
            select_func(&recipes, arg);
    }
    return sqlite3_finalize(stmt);
}

int select_recipeid_from_table(const char *table_name, const int recipeid, int (*select_func)(void *, void *), void *arg)
{
    char buf[128];
    sprintf(buf, sql_select_recipeid, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, recipeid);
    if (select_func != NULL)
    {
        recipes_t recipes;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            select_stmt(&recipes, stmt);
            if (select_func != NULL)
                select_func(&recipes, arg);
        }
        return sqlite3_finalize(stmt);
    }
    int ret = -1;
    recipes_t *recipes = (recipes_t *)arg;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        select_stmt(recipes, stmt);
        ret = 0;
    }
    sqlite3_finalize(stmt);
    return ret;
}

int select_dishname_from_table(const char *table_name, const char *dishname, void *arg)
{
    int ret = -1;
    char buf[128];
    sprintf(buf, sql_select_dishname, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, dishname, strlen(dishname), NULL);
    recipes_t *recipes = (recipes_t *)arg;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        select_stmt(recipes, stmt);
        ret = 0;
    }
    sqlite3_finalize(stmt);
    return ret;
}

int select_id_from_table(const char *table_name, const int id, void *arg)
{
    int ret = -1;
    char buf[128];
    sprintf(buf, sql_select_id, table_name);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(get_db_from_table(table_name), buf, -1, &stmt, NULL);
    if (SQLITE_OK != rc)
    {
        printf("%s,sqlite3_prepare_v2:%s\n", __func__, sqlite3_errmsg(get_db_from_table(table_name)));
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, id);
    recipes_t *recipes = (recipes_t *)arg;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        select_stmt(recipes, stmt);
        ret = 0;
    }
    sqlite3_finalize(stmt);
    return ret;
}

static int databse_create_table(const char *table_name)
{
    char buf[512];
    sprintf(buf, create_table, table_name);

    char *errMsg = NULL;
    int rc = sqlite3_exec(get_db_from_table(table_name), buf, NULL, NULL, &errMsg);
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
        dzlog_error("RecipesVersion is NULL\n");
        goto fail;
    }

    cJSON *attr = cJSON_GetObjectItem(root, "recipes");
    if (attr == NULL)
    {
        dzlog_error("attr is NULL\n");
        goto fail;
    }

    int arraySize = cJSON_GetArraySize(attr);
    if (arraySize == 0)
    {
        dzlog_error("attr arraySize is 0\n");
        goto fail;
    }
    int i;
    recipes_t recipes = {0};
    cJSON *arraySub, *dishName, *cookSteps, *recipeid, *recipeType, *cookPos;
    for (i = 0; i < arraySize; i++)
    {
        arraySub = cJSON_GetArrayItem(attr, i);
        if (arraySub == NULL)
            continue;

        dishName = cJSON_GetObjectItem(arraySub, "dishName");
        strcpy(recipes.dishName, dishName->valuestring);
        cookSteps = cJSON_GetObjectItem(arraySub, "cookSteps");
        strcpy(recipes.cookSteps, cookSteps->valuestring);

        recipeid = cJSON_GetObjectItem(arraySub, "recipeid");
        recipes.recipeid = recipeid->valueint;
        recipeType = cJSON_GetObjectItem(arraySub, "recipeType");
        recipes.recipeType = recipeType->valueint;
        cookPos = cJSON_GetObjectItem(arraySub, "cookPos");
        recipes.cookPos = cookPos->valueint;
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
    if (sqlHandle.recipe_db != NULL)
    {
        sqlite3_close_v2(sqlHandle.recipe_db);
    }
}

int database_init(void)
{
    int rc = sqlite3_open_v2(RECIPE_DB_NAME, &sqlHandle.recipe_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (SQLITE_OK != rc)
    {
        printf("%s,%s:sqlite3_open_v2 errmsg:%s\n", __func__, RECIPE_DB_NAME, sqlite3_errmsg(sqlHandle.recipe_db));
        sqlite3_close_v2(sqlHandle.recipe_db);
    }

    if (databse_create_table(RECIPE_TABLE_NAME) == 0)
    {
        printf("%s,%s:first databse_create_table ...\n", __func__, RECIPE_TABLE_NAME);
        get_dev_profile(".", NULL, RECIPES_FILE, recipes_parse_json);
    }
    return 0;
}

void database_reinit(void)
{
    databse_drop_table(RECIPE_TABLE_NAME);

    if (databse_create_table(RECIPE_TABLE_NAME) == 0)
    {
        printf("%s,%s:first databse_create_table ...\n", __func__, RECIPE_TABLE_NAME);
        get_dev_profile(".", NULL, RECIPES_FILE, recipes_parse_json);
    }
}
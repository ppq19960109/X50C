#ifndef _DATABASE_H_
#define _DATABASE_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdbool.h>

#define DB_NAME "mars.db"
#define HISTORY_TABLE_NAME "history"
#define RECIPE_TABLE_NAME "recipe"
#define RECIPES_FILE "Recipes"

    typedef struct
    {
        int id;
        int seqid;
        char dishName[48];
        char imgUrl[80];
        char details[960];
        char cookSteps[180];
        int timestamp;
        int collect;
        int recipeid;
        int recipeType;
        int cookPos;
    } recipes_t;

    int database_init(void);
    void database_deinit(void);
    int databse_drop_table(const char *table_name);
    int databse_clear_table(const char *table_name);

    int select_from_table(const char *table_name, int (*select_func)(void *, void *), void *arg);
    int select_seqid_from_table(const char *table_name, const int seqid, int (*select_func)(void *, void *), void *arg);
    int select_recipeid_from_table(const char *table_name, const int recipeid, int (*select_func)(void *, void *), void *arg);
    int select_dishname_from_table(const char *table_name, const char *dishname, void *arg);
    int select_id_from_table(const char *table_name, const int id, void *arg);

    int update_key_to_table(const char *table_name, const char *key, const int val, const int id);
    int update_key_string_table(const char *table_name, const char *key, const char *val, const int id);
    int delete_row_from_table(const char *table_name, const int id);
    int insert_replace_row_to_table(const char *table_name, recipes_t *recipes);
    int insert_row_to_table(const char *table_name, recipes_t *recipes);
#ifdef __cplusplus
}
#endif
#endif
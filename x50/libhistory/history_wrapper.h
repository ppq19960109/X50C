#ifndef HISTORY_WRAPPER_H
#define HISTORY_WRAPPER_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int id;
        int seqid;
        char dishName[64];
        char imgUrl[64];
        char details[1024];
        char cookSteps[256];
        int timestamp;
        int collect;
        int cookType;
        int recipeType;
        int cookPos;
    } history_t;
    void register_history_select_cb(int (*cb)(history_t *history, void *arg));
    void register_history_insert_cb(int (*cb)(history_t *history));
    void register_history_delete_cb(int (*cb)(int id));
    void register_history_update_cb(int (*cb)(history_t *history));
    void wrapper_clearHistory(void);
    void wrapper_selectHistory(void *arg);
    void wrapper_reportDeleteHistory(int id);
    int wrapper_insertHistory(history_t *history);
    void wrapper_reportInsertHistory(history_t *history, const int sort);
    int wrapper_updateHistory(history_t *history);
    void wrapper_reportUpdateHistory(history_t *history);
#ifdef __cplusplus
};
#endif
#endif
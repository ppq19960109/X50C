#include "history_wrapper.h"
#include "history.h"

static CookHistory cookHistory;
void wrapper_clearHistory(void)
{
    cookHistory.clearHistory();
}
int wrapper_insertHistory(history_t *history)
{
    return cookHistory.insertHistory(history);
}
void wrapper_reportInsertHistory(history_t *history, const int sort)
{
    cookHistory.reportInsertHistory(history, sort);
}
void wrapper_reportDeleteHistory(int id)
{
    cookHistory.reportDeleteHistory(id);
}
int wrapper_updateHistory(history_t *history)
{
    return cookHistory.updateHistory(history);
}
void wrapper_reportUpdateHistory(history_t *history)
{
    cookHistory.reportUpdateHistory(history);
}

void wrapper_selectHistory(void *arg)
{
    cookHistory.selectHistory(arg);
}
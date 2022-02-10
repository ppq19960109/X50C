#include "history_wrapper.h"
#include "history.h"

static CookHistory cookHistory;

int wrapper_insertHistory(history_t history)
{
    return cookHistory.insertHistory(history);
}
void wrapper_reportInsertHistory(history_t history)
{
    cookHistory.reportInsertHistory(history);
}
void wrapper_reportDeleteHistory(int id)
{
    cookHistory.reportDeleteHistory(id);
}
int wrapper_updateHistory(const int id, const int collect)
{
    return cookHistory.updateHistory(id, collect);
}
void wrapper_reportUpdateHistory(const int id, const int seqid, const int collect, int timestamp)
{
    cookHistory.reportUpdateHistory(id, seqid, collect, timestamp);
}

void wrapper_selectHistory()
{
    cookHistory.selectHistory();
}
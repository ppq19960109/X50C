#include "history.h"
#include <cstring>

int (*history_select_cb)(history_t history);
void register_history_select_cb(int (*cb)(history_t history))
{
    history_select_cb = cb;
}

int (*history_insert_cb)(history_t history);
void register_history_insert_cb(int (*cb)(history_t history))
{
    history_insert_cb = cb;
}

int (*history_delete_cb)(int id);
void register_history_delete_cb(int (*cb)(int id))
{
    history_delete_cb = cb;
}

int (*history_update_cb)(const int id, const int collect, int timestamp);
void register_history_update_cb(int (*cb)(const int id, const int collect, int timestamp))
{
    history_update_cb = cb;
}

void CookHistory::sortHistory()
{
    historyList.sort();
}

void CookHistory::selectHistory()
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        history_select_cb(iter->history);
    }
}

void CookHistory::reportDeleteHistory(int id)
{
    historyList.remove_if([=](recipes_t recipe)
                          { return recipe.history.id == id; });
}

int CookHistory::compareHistoryCollect(const history_t &single)
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        if (iter->history.collect == 0)
            continue;

        if (strcmp(iter->history.dishName, single.dishName) == 0 && strcmp(iter->history.imgUrl, single.imgUrl) == 0 && strcmp(iter->history.details, single.details) == 0 && strcmp(iter->history.cookSteps, single.cookSteps) == 0)
        {
            iter->history.timestamp = single.timestamp;
            return iter->history.id;
        }
    }
    return -1;
}

int CookHistory::getHistoryCount()
{
    int count = 0;
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        if (iter->history.collect == 0)
        {
            ++count;
        }
    }
    return count;
}

int CookHistory::lastHistoryId(const int collect)
{
    int id = -1;
    list<recipes_t>::reverse_iterator iter;
    for (iter = historyList.rbegin(); iter != historyList.rend(); ++iter)
    {
        if (iter->history.collect == collect)
        {
            id = iter->history.id;
            break;
        }
    }
    return id;
}

int CookHistory::insertHistory(history_t single)
{
    int ret = -1;

    single.timestamp = std::time(NULL);
    int id = compareHistoryCollect(single);
    if (id >= 0)
    {
        ret = history_update_cb(id, -1, single.timestamp);
    }
    else
    {
        if (getHistoryCount() >= MAX_HISTORY)
        {
            int id = lastHistoryId(0);
            if (id >= 0)
                ret = history_delete_cb(id);
        }

        ret = history_insert_cb(single);
    }
    return ret;
}

void CookHistory::reportInsertHistory(history_t single)
{
    recipes_t recipe;
    recipe.history = single;
    historyList.push_front(recipe);
    sortHistory();
}

int CookHistory::updateHistory(const int id, const int collect)
{
    int ret = -1;

    int lastId = -1;
    if (collect)
    {
        if (historyList.size() - getHistoryCount() >= MAX_COLLECT)
        {
            lastId = lastHistoryId(1);
        }
    }
    else
    {
        if (getHistoryCount() >= MAX_HISTORY)
        {
            lastId = lastHistoryId(0);
        }
    }
    if (lastId >= 0 && lastId != id)
    {
        ret = history_delete_cb(id);
    }
    ret = history_update_cb(id, collect, -1);
    return ret;
}

void CookHistory::reportUpdateHistory(const int id, const int seqid, const int collect, int timestamp)
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        if (iter->history.id == id)
        {
            iter->history.seqid = seqid;
            if (collect >= 0)
                iter->history.collect = collect;
            if (timestamp >= 0)
                iter->history.timestamp = timestamp;
            break;
        }
    }
    sortHistory();
}

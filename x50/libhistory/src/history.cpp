#include "history.h"
#include <cstring>

static int (*history_select_cb)(history_t *history, void *arg, int simple);
void register_history_select_cb(int (*cb)(history_t *history, void *arg, int simple))
{
    history_select_cb = cb;
}

static int (*history_insert_cb)(history_t *history);
void register_history_insert_cb(int (*cb)(history_t *history))
{
    history_insert_cb = cb;
}

static int (*history_delete_cb)(int id);
void register_history_delete_cb(int (*cb)(int id))
{
    history_delete_cb = cb;
}

static int (*history_update_cb)(history_t *history);
void register_history_update_cb(int (*cb)(history_t *history))
{
    history_update_cb = cb;
}

void CookHistory::clearHistory()
{
    historyList.clear();
}
void CookHistory::sortHistory()
{
    historyList.sort();
}

void CookHistory::selectHistory(void *arg, int simple)
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        history_select_cb(&iter->history, arg, simple);
    }
}

void CookHistory::reportDeleteHistory(int id)
{
    historyList.remove_if([=](recipes_t recipe)
                          { return recipe.history.id == id; });
}

int CookHistory::compareHistoryCollect(const history_t *single)
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        if (iter->history.collect == 0)
            continue;

        if (iter->history.recipeid == single->recipeid && strcmp(iter->history.cookSteps, single->cookSteps) == 0)
        {
            iter->history.timestamp = single->timestamp;
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

int CookHistory::insertHistory(history_t *single)
{
    int ret = -1;

    single->timestamp = std::time(NULL);
    int id = compareHistoryCollect(single);
    if (id >= 0)
    {
        history_t history = {0};
        history.id = id;
        history.collect = -1;
        history.timestamp = single->timestamp;
        ret = history_update_cb(&history);
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

void CookHistory::reportInsertHistory(history_t *single, const int sort)
{
    if (single != NULL)
    {
        recipes_t recipe;
        recipe.history = *single;
        historyList.push_front(recipe);
    }
    if (sort)
        sortHistory();
}

int CookHistory::updateHistory(history_t *single)
{
    int ret = -1;
    int lastId = -1;

    list<recipes_t>::iterator iter = std::find_if(historyList.begin(), historyList.end(), [=](recipes_t recipe)
                                                  { return recipe.history.id == single->id; });
    if (iter == historyList.end())
    {
        return -1;
    }
    if (single->collect >= 0)
    {
        if (single->collect != iter->history.collect)
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

        if (lastId >= 0 && lastId != single->id)
        {
            ret = history_delete_cb(single->id);
        }
    }
    ret = history_update_cb(single);
    return ret;
}

void CookHistory::reportUpdateHistory(history_t *single)
{
    list<recipes_t>::iterator iter;
    for (iter = historyList.begin(); iter != historyList.end(); ++iter)
    {
        if (iter->history.id == single->id)
        {
            iter->history.seqid = single->seqid;
            if (single->collect >= 0)
                iter->history.collect = single->collect;
            if (single->timestamp > 0)
                iter->history.timestamp = single->timestamp;
            if (strlen(single->dishName) > 0)
                strcpy(iter->history.dishName, single->dishName);
            break;
        }
    }
    sortHistory();
}

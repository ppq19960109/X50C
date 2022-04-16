#ifndef HISTORY_H
#define HISTORY_H

#include <iostream>
#include <list>
#include <string>
#include <algorithm>
#include <functional>
#include <ctime>
#include "history_wrapper.h"
using namespace std;

#define MAX_HISTORY (40)
#define MAX_COLLECT (40)

struct recipes_t
{
    history_t history;

    bool operator<(const recipes_t &other)
    {
        if (this->history.collect == other.history.collect)
        {
            return this->history.seqid > other.history.seqid;
        }
        else
        {
            return this->history.collect > 0;
        }
    }
};

class CookHistory
{
public:
    void clearHistory();
    void sortHistory();
    void selectHistory(void *arg, int simple);
    void reportDeleteHistory(int id);
    int compareHistoryCollect(const history_t *single);
    int getHistoryCount();
    int lastHistoryId(const int collect);
    int insertHistory(history_t *single);
    void reportInsertHistory(history_t *single, const int sort);
    int updateHistory(history_t *history);
    void reportUpdateHistory(history_t *history);
    list<recipes_t> historyList;
};

#endif
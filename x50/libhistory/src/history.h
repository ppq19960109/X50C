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
    // int id;
    // int seqid;
    // char dishName[64];
    // char imgUrl[64];
    // char details[512];
    // char cookSteps[256];
    // int timestamp;
    // int collect;
    // int cookType;
    // int recipeType;
    // int cookPos;
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
    void sortHistory();
    void selectHistory();
    void reportDeleteHistory(int id);
    int compareHistoryCollect(const history_t &single);
    int getHistoryCount();
    int lastHistoryId(const int collect);
    int insertHistory(history_t single);
    void reportInsertHistory(history_t single);
    
    int updateHistory(const int id, const int collect);
    void reportUpdateHistory(const int id, const int seqid, const int collect, int timestamp);
    list<recipes_t> historyList;
};

#endif
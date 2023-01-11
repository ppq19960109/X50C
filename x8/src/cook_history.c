#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "curl/curl.h"
#include "curl_http_request.h"
#include "cloud_platform_task.h"

static void cook_history_post(cJSON *root)
{
    cloud_dev_t *cloud_dev = get_cloud_dev();

    cJSON_AddStringToObject(root, "deviceMac", cloud_dev->mac);
    cJSON_AddStringToObject(root, "iotId", cloud_dev->mac);
    cJSON_AddStringToObject(root, "token", cloud_dev->token);

    char *json = cJSON_PrintUnformatted(root);
    curl_http_post("http://mcook.marssenger.com/menu-anon/addCookHistory", json);//http://mcook.marssenger.com http://mcook.dev.marssenger.net
    free(json);
}
void cook_history_reset()
{
    cloud_dev_t *cloud_dev = get_cloud_dev();
    cJSON *root = cJSON_CreateObject();
    // cJSON_AddStringToObject(root, "deviceMac", cloud_dev->mac);
    cJSON_AddStringToObject(root, "iotId", cloud_dev->mac);
    cJSON_AddStringToObject(root, "token", cloud_dev->token);

    char *json = cJSON_PrintUnformatted(root);
    curl_http_post("http://mcook.marssenger.com/menu-anon/deleteCookHistoryAndCurves", json);
    free(json);
}
int cook_history_set(void *data)
{
    cJSON *root = data;

    cJSON *resp = cJSON_CreateObject();
    cJSON *cookSteps = cJSON_AddArrayToObject(resp, "cookModelDTOList");
    int arraySize, i;
    cJSON *arrayItem, *arraySub, *Mode, *Temp, *Timer, *SteamGear;
    if (cJSON_HasObjectItem(root, "LCookbookParam") && cJSON_HasObjectItem(root, "LCookbookID"))
    {
        cJSON_AddNumberToObject(resp, "cavity", 0);
        cJSON_AddNumberToObject(resp, "number", 1);

        cJSON *LCookbookID = cJSON_GetObjectItem(root, "LCookbookID");
        cJSON_AddNumberToObject(resp, "menuId", LCookbookID->valueint);

        arrayItem = cJSON_GetObjectItem(root, "LCookbookParam");
        arraySize = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < arraySize; ++i)
        {
            arraySub = cJSON_GetArrayItem(arrayItem, i);
            if (arraySub == NULL)
                continue;
            cJSON *cookStep = cJSON_CreateObject();
            cJSON_AddItemToArray(cookSteps, cookStep);

            Mode = cJSON_GetObjectItem(arraySub, "Mode");
            Temp = cJSON_GetObjectItem(arraySub, "Temp");
            Timer = cJSON_GetObjectItem(arraySub, "Timer");
            cJSON_AddNumberToObject(cookStep, "model", Mode->valueint);
            cJSON_AddNumberToObject(cookStep, "time", Timer->valueint);
            cJSON_AddNumberToObject(cookStep, "temp", Temp->valueint);
            cJSON_AddNumberToObject(cookStep, "Index", i + 1);

            if (cJSON_HasObjectItem(arraySub, "SteamGear"))
            {
                SteamGear = cJSON_GetObjectItem(arraySub, "SteamGear");
                cJSON_AddNumberToObject(cookStep, "LSteamGear", SteamGear->valueint);
            }
        }
    }
    else if (cJSON_HasObjectItem(root, "LMultiStageContent"))
    {
        cJSON_AddNumberToObject(resp, "cavity", 0);
        cJSON_AddNumberToObject(resp, "number", 1);

        arrayItem = cJSON_GetObjectItem(root, "LMultiStageContent");
        arraySize = cJSON_GetArraySize(arrayItem);
        for (i = 0; i < arraySize; ++i)
        {
            arraySub = cJSON_GetArrayItem(arrayItem, i);
            if (arraySub == NULL)
                continue;
            cJSON *cookStep = cJSON_CreateObject();
            cJSON_AddItemToArray(cookSteps, cookStep);

            Mode = cJSON_GetObjectItem(arraySub, "Mode");
            Temp = cJSON_GetObjectItem(arraySub, "Temp");
            Timer = cJSON_GetObjectItem(arraySub, "Timer");
            cJSON_AddNumberToObject(cookStep, "model", Mode->valueint);
            cJSON_AddNumberToObject(cookStep, "time", Timer->valueint);
            cJSON_AddNumberToObject(cookStep, "temp", Temp->valueint);
            cJSON_AddNumberToObject(cookStep, "Index", i + 1);

            if (cJSON_HasObjectItem(arraySub, "SteamGear"))
            {
                SteamGear = cJSON_GetObjectItem(arraySub, "SteamGear");
                cJSON_AddNumberToObject(cookStep, "LSteamGear", SteamGear->valueint);
            }
        }
    }
    else if (cJSON_HasObjectItem(root, "LStOvMode") && cJSON_HasObjectItem(root, "LStOvSetTimer") && cJSON_HasObjectItem(root, "LStOvSetTemp"))
    {
        cJSON_AddNumberToObject(resp, "cavity", 0);
        cJSON_AddNumberToObject(resp, "number", 0);

        cJSON *cookStep = cJSON_CreateObject();
        cJSON_AddItemToArray(cookSteps, cookStep);

        cJSON *LStOvMode = cJSON_GetObjectItem(root, "LStOvMode");
        cJSON *LStOvSetTimer = cJSON_GetObjectItem(root, "LStOvSetTimer");
        cJSON *LStOvSetTemp = cJSON_GetObjectItem(root, "LStOvSetTemp");
        cJSON_AddNumberToObject(cookStep, "model", LStOvMode->valueint);
        cJSON_AddNumberToObject(cookStep, "time", LStOvSetTimer->valueint);
        cJSON_AddNumberToObject(cookStep, "temp", LStOvSetTemp->valueint);
        cJSON_AddNumberToObject(cookStep, "Index", 1);
        if (cJSON_HasObjectItem(root, "LSteamGear"))
        {
            SteamGear = cJSON_GetObjectItem(root, "LSteamGear");
            cJSON_AddNumberToObject(cookStep, "LSteamGear", SteamGear->valueint);
        }
    }
    else if (cJSON_HasObjectItem(root, "RStOvMode") && cJSON_HasObjectItem(root, "RStOvSetTimer") && cJSON_HasObjectItem(root, "RStOvSetTemp"))
    {
        cJSON_AddNumberToObject(resp, "cavity", 1);
        cJSON_AddNumberToObject(resp, "number", 0);

        cJSON *cookStep = cJSON_CreateObject();
        cJSON_AddItemToArray(cookSteps, cookStep);

        cJSON *RStOvMode = cJSON_GetObjectItem(root, "RStOvMode");
        cJSON *RStOvSetTimer = cJSON_GetObjectItem(root, "RStOvSetTimer");
        cJSON *RStOvSetTemp = cJSON_GetObjectItem(root, "RStOvSetTemp");
        cJSON_AddNumberToObject(cookStep, "model", RStOvMode->valueint);
        cJSON_AddNumberToObject(cookStep, "time", RStOvSetTimer->valueint);
        cJSON_AddNumberToObject(cookStep, "temp", RStOvSetTemp->valueint);
        cJSON_AddNumberToObject(cookStep, "Index", 1);
    }
    else
    {
        goto fail;
    }
    cook_history_post(resp);
fail:
    cJSON_Delete(resp);
    return 0;
}

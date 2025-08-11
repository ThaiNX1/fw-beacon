/**
 ******************************************************************************
 * @file    myCronJob.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "myCronJob.h"
#include "FlashHandler.h"
#include "timeCheck.h"
#include "ProtocolHandler.h"
#include "MqttHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "My_Cronjob"

#ifndef DISABLE_LOG_ALL
#define MY_CRONJOB_LOG_INFO_ON
#endif

#ifdef MY_CRONJOB_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#define log_warning(format, ...)
#endif

/*******************************************************************************
 * Extern Variables
 ******************************************************************************/
extern struct tm timeLocal;
extern schedule_t mySchedule;

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
void My_CronJob_Initialize()
{
    // FlashHandler_getMyJobsInStore();
    // myCronJob_resetSeconds();
}

void myCronJob_resetSeconds()
{
    for (uint8_t rst_sch = 0; rst_sch < mySchedule.totalJob; rst_sch++) {
		mySchedule.myTimeSetting[rst_sch].second = 0;
	}
    FlashHandler_saveMyJobsInStore(); 
    log_warning("Reset Seconds Power Success");
    myCronJob_logMyJobs();
}

bool myCronJob_checkSchNoRepeat(char *timeMday, char *timeMonth, char *timeWday)
{
    if ((strcmp(timeMday, "?") == 0) || (strcmp(timeMday, "*") == 0)) {
        return false;
    } else if ((strcmp(timeMonth, "?") == 0) || (strcmp(timeMonth, "*") == 0)) {
        return false;
    } else if (strcmp(timeWday, "?") != 0) {
        return false;
    } else {
        return true;
    }
}

bool myCronJob_checkAlwayRepeat(char *timeMday, char *timeMonth, char *timeWday)
{
    if ((strcmp(timeMday, "*") == 0) && (strcmp(timeMonth, "*") == 0) && (strcmp(timeWday, "*") == 0)) {
        return true;
    } else {
        return false;
    }
}

bool myCronJob_checkWeekDayRepeat(char *timeMday, char *timeMonth, char *timeWday)
{
    if (strcmp(timeMday, "?") != 0) {
        return false;
    } else if (strcmp(timeMonth, "*") != 0) {
        return false;
    } else if ((strcmp(timeWday, "?") == 0) || (strcmp(timeWday, "*") == 0)) {
        return false;
    } else {
        return true;
    }
}

bool myCronJob_findWeekday(char *timeWday, uint8_t day)
{
    char *find_weekday = NULL;
    char find_day[10];
    sprintf(find_day,"%d", day);
    find_weekday = strstr(timeWday, find_day);
    if (find_weekday == NULL) {
        log_warning("Day: %s Weekday_Setting: %s Result: NULL", find_day, timeWday);
        return false;
    } else {
        log_warning("Day: %s Weekday_Setting: %s Result: %s", find_day, timeWday, find_weekday);
        return true;
    }
}

void myCronJob_reportJobCurrent(int job_idx)
{
    char bufJob[150] = "";
    if (job_idx == -1) {
        strcpy(bufJob,"{\"d\":{}}");
    } else {
        sprintf(bufJob,"{\"d\":{\"e\":%d,\"j\":\"0-1 %d %d %s %s %s\",\"d\":[", mySchedule.myTimeSetting[job_idx].stateActive, 
                                                                                mySchedule.myTimeSetting[job_idx].minute,
                                                                                mySchedule.myTimeSetting[job_idx].hour,
                                                                                mySchedule.myTimeSetting[job_idx].dayOfMonth,
                                                                                mySchedule.myTimeSetting[job_idx].month,
                                                                                mySchedule.myTimeSetting[job_idx].dayOfWeek);
        char bufSub[25] = "";
        for (uint8_t cnt_cmd = 0; cnt_cmd < mySchedule.myTimeSetting[job_idx].numCommand; cnt_cmd++) {
            sprintf(bufSub,"{\"lid\":%d,\"d\":%d},", mySchedule.myTimeSetting[job_idx].myListId[cnt_cmd].id, mySchedule.myTimeSetting[job_idx].myListId[cnt_cmd].state);
            strcat(bufJob, bufSub);
        }
        bufJob[strlen(bufJob) - 1] = '\0'; //remove ','
        strcat(bufJob, "]}}");
    }
    MQTT_PublishDataCommon(bufJob, PROPERTY_CODE_SCHEDULE_CURRENT);
}

void myCronJob_reportAllJob()
{
    char bufMainJob[1500] = "";
    if (mySchedule.totalJob == 0) {
        strcpy(bufMainJob,"{\"d\":{\"total\":0,\"sch\":[]}}");
    } else {
        sprintf(bufMainJob,"{\"d\":{\"total\":%d,\"sch\":[", mySchedule.totalJob);
        char bufSubJob[150] = "";
        for (uint8_t cnt_job = 0; cnt_job < mySchedule.totalJob; cnt_job++) {
            sprintf(bufSubJob,"{\"e\":%d,\"j\":\"0-1 %d %d %s %s %s\",\"d\":[", mySchedule.myTimeSetting[cnt_job].stateActive,
                                                                                mySchedule.myTimeSetting[cnt_job].minute,
                                                                                mySchedule.myTimeSetting[cnt_job].hour,
                                                                                mySchedule.myTimeSetting[cnt_job].dayOfMonth,
                                                                                mySchedule.myTimeSetting[cnt_job].month,
                                                                                mySchedule.myTimeSetting[cnt_job].dayOfWeek);
            char bufSub[25] = "";
            for (uint8_t cnt_cmd = 0; cnt_cmd < mySchedule.myTimeSetting[cnt_job].numCommand; cnt_cmd++) {
                sprintf(bufSub,"{\"lid\":%d,\"d\":%d},", mySchedule.myTimeSetting[cnt_job].myListId[cnt_cmd].id, mySchedule.myTimeSetting[cnt_job].myListId[cnt_cmd].state);
                strcat(bufSubJob, bufSub);
            }
            bufSubJob[strlen(bufSubJob) - 1] = '\0'; //remove ','
            strcat(bufSubJob, "]},");
            strcat(bufMainJob, bufSubJob);
        }
        bufMainJob[strlen(bufMainJob) - 1] = '\0'; //remove ','
        strcat(bufMainJob, "]}}");
    }
    MQTT_PublishDataCommon(bufMainJob, PROPERTY_CODE_SCHEDULE);
}

void myCronJob_logMyJobs()
{
    printf(" Log Job Info: %d Jobs\n", mySchedule.totalJob);
    if (mySchedule.totalJob > 0) {
        for (uint8_t i = 0; i < mySchedule.totalJob; i++) {
            printf("  Job.%d \"%d %d %d %s %s %s\" [Act:%d-Cmd:%d]:\n", i, mySchedule.myTimeSetting[i].second,
                                                                                mySchedule.myTimeSetting[i].minute,
                                                                                mySchedule.myTimeSetting[i].hour,
                                                                                mySchedule.myTimeSetting[i].dayOfMonth,
                                                                                mySchedule.myTimeSetting[i].month,
                                                                                mySchedule.myTimeSetting[i].dayOfWeek,
                                                                                mySchedule.myTimeSetting[i].stateActive,
                                                                                mySchedule.myTimeSetting[i].numCommand);
            for (uint8_t j = 0; j < mySchedule.myTimeSetting[i].numCommand; j++) {
                printf("   [%d] - ID: %d State: %d\n", i, mySchedule.myTimeSetting[i].myListId[j].id, mySchedule.myTimeSetting[i].myListId[j].state);
            }
            printf("\n");
        } 
    }
}   

bool myCronJob_getJobCallback(cJSON* jobObject)
{
    char *second = NULL, *minute = NULL, *hour = NULL, *dayOfWeek = NULL, *dayOfMonth = NULL, *month = NULL;
    char  job[MAX_LEN_CHAR_JOB];

    if (cJSON_HasObjectItem(jobObject, "total") && cJSON_HasObjectItem(jobObject, "sch")) {
        int8_t total = cJSON_GetObjectItem(jobObject, "total")->valueint;
        if (total > MAX_SCHEDULE_SETTING) {
            return false;
        }
        cJSON* sch = cJSON_GetObjectItem(jobObject, "sch");
        uint8_t size_sch = cJSON_GetArraySize(sch);
        if (size_sch == total) {
            if (total >= 0) {
                for (uint8_t i = 0; i < total; i++)
                {
                    cJSON* item = cJSON_GetArrayItem(sch, i);
                    if (cJSON_HasObjectItem(item, "e") && cJSON_HasObjectItem(item, "j") && cJSON_HasObjectItem(item, "d")) {
                        mySchedule.myTimeSetting[i].stateActive = cJSON_GetObjectItem(item, "e")->valueint;
                        strcpy(job, cJSON_GetObjectItem(item, "j")->valuestring);
                        if ((strlen(job) > MAX_LEN_CHAR_JOB -1) || (strlen(job) == 0)) {
                            return false;
                        } else {
                            second = strtok(job, " ");
                            if (second == NULL) {
                                return false;
                            } else {
                                minute = strtok(NULL, " ");
                                if (minute == NULL) {
                                    return false;
                                } else {
                                    hour = strtok(NULL, " ");
                                    if (hour == NULL) {
                                        return false;
                                    } else {
                                        dayOfMonth = strtok(NULL, " ");
                                        if ((dayOfMonth == NULL) || (strlen(dayOfMonth) > MIN_CHAR_IN_TIME)) {
                                            return false;
                                        } else {
                                            month = strtok(NULL, " ");
                                            if ((month == NULL) || (strlen(month) > MIN_CHAR_IN_TIME)) {
                                                return false;
                                            } else {
                                                dayOfWeek = strtok(NULL, " ");
                                                if ((dayOfWeek == NULL) || (strlen(dayOfWeek) > MAX_CHAR_IN_TIME)) {
                                                    return false;
                                                } else {
                                                    mySchedule.myTimeSetting[i].second = 0;
                                                    mySchedule.myTimeSetting[i].minute = atoi(minute);
                                                    mySchedule.myTimeSetting[i].hour = atoi(hour);
                                                    strcpy(mySchedule.myTimeSetting[i].dayOfMonth, dayOfMonth);
                                                    strcpy(mySchedule.myTimeSetting[i].month, month);
                                                    strcpy(mySchedule.myTimeSetting[i].dayOfWeek, dayOfWeek);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }                       

                        cJSON* arrayCmd = cJSON_GetObjectItem(item, "d");
                        if (arrayCmd->type != cJSON_Array) {
                            return false;
                        }
                        uint8_t numCmd = cJSON_GetArraySize(arrayCmd);
                        if (numCmd == 0 || numCmd > NUM_CMD_ARRAY) {
                            return false;
                        }
                        mySchedule.myTimeSetting[i].numCommand = numCmd;
                        for (uint8_t cmdIdx = 0; cmdIdx < numCmd; cmdIdx++) {
                            cJSON* itemCmd = cJSON_GetArrayItem(arrayCmd, cmdIdx);
                            if (cJSON_HasObjectItem(itemCmd, "lid") && cJSON_HasObjectItem(itemCmd, "d")) {
                                uint8_t lid = cJSON_GetObjectItem(itemCmd, "lid")->valueint;
                                uint8_t state = cJSON_GetObjectItem(itemCmd, "d")->valueint;
                                mySchedule.myTimeSetting[i].myListId[cmdIdx].id = lid;
                                mySchedule.myTimeSetting[i].myListId[cmdIdx].state = state;
                            }   
                        }
                    } else {
                        return false;
                    }
                }
                for (uint8_t j = total; j < MAX_SCHEDULE_SETTING; j++) {
                    mySchedule.myTimeSetting[j].stateActive = 0;
                    mySchedule.myTimeSetting[j].numCommand = 0;
                    mySchedule.myTimeSetting[j].second = 0;
                    mySchedule.myTimeSetting[j].minute = 0;
                    mySchedule.myTimeSetting[j].hour = 0;
                    strcpy(mySchedule.myTimeSetting[j].dayOfMonth, "");
                    strcpy(mySchedule.myTimeSetting[j].month, "");
                    strcpy(mySchedule.myTimeSetting[j].dayOfWeek, "");
                    for (uint8_t k = 0; k < MAX_SCH_IN_ONE_TIME; k++)
                    {
                        mySchedule.myTimeSetting[j].myListId[k].id = 0;
                        mySchedule.myTimeSetting[j].myListId[k].state = 0;
                    }
                }
                mySchedule.totalJob = total;

                if (FlashHandler_saveMyJobsInStore()) {
                    myCronJob_logMyJobs(); 
                    sendStatusCronjob(1);
                    return true;  
                }
            } else {
                return false;
            }
        }
    }
    return false;
}

/***********************************************/
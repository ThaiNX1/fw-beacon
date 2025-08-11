/**
 ******************************************************************************
 * @file    myCronJob.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __MY_CRONJOB_H
#define __MY_CRONJOB_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define MAX_LEN_CHAR_JOB    30
#define NUM_CMD_ARRAY       3

/* Exported functions ------------------------------------------------------- */ 
void My_CronJob_Initialize();
void myCronJob_resetSeconds();
bool myCronJob_checkSchNoRepeat(char *timeMday, char *timeMonth, char *timeWday);
bool myCronJob_checkAlwayRepeat(char *timeMday, char *timeMonth, char *timeWday);
bool myCronJob_checkWeekDayRepeat(char *timeMday, char *timeMonth, char *timeWday);
bool myCronJob_findWeekday(char *timeWday, uint8_t day);
void myCronJob_reportJobCurrent(int job_idx);
void myCronJob_reportAllJob();
void myCronJob_logMyJobs();
bool myCronJob_getJobCallback(cJSON* jobObject);

#endif /* __MY_CRONJOB_H */

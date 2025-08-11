/**
 ******************************************************************************
 * @file    timeCheck.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __TIME_CHECK_H
#define __TIME_CHECK_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
#define MAX_SCHEDULE_SETTING 	10
#define MIN_CHAR_IN_TIME 		3
#define MAX_CHAR_IN_TIME 		12
#define MAX_SCH_IN_ONE_TIME 	1

typedef struct 
{
	uint8_t second;
	uint8_t minute;
	uint32_t hour;
} infoDataActiveDevice_t;

typedef struct 
{
	uint8_t id;
	uint8_t state;
} listId_t;

typedef struct 
{
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	char dayOfMonth[MIN_CHAR_IN_TIME];
	char month[MIN_CHAR_IN_TIME];
	char dayOfWeek[MAX_CHAR_IN_TIME];
	uint8_t stateActive;
	uint8_t numCommand;
	listId_t myListId[MAX_SCH_IN_ONE_TIME];
} timeSetting_t;

typedef struct 
{
	uint8_t totalJob;
	timeSetting_t myTimeSetting[MAX_SCHEDULE_SETTING];
} schedule_t;

/* Exported macro ------------------------------------------------------------*/
#define NAME_SPACE_ACTIVE_DEVICE 	"act_device"

#define KEY_BEGIN_ACTIVE_DEVICE 	"begin_act"
#define KEY_END_ACTIVE_DEVICE 		"end_act"
#define KEY_WAIT_TIME_ACTIVE 		"wait_act"
#define KEY_SAVE_TIME_ACTIVE 		"time_act"

/* Exported functions ------------------------------------------------------- */
void timerActiveDeviceStart();
void timerActiveDeviceStop();
void myScheduleStart();
void myScheduleStop();
void activeDevice(void* arg);
void mySheduleExecute(void* arg);
void Flash_loadDataActiveDevice();
bool Flash_saveBeginActive();
bool Flash_saveEndActived();
bool Flash_saveWaitTimeActive();
bool Flash_saveTimeActived();

/* Functions callback-------------------------------------------------------- */
bool checkRealTimeLocal();
void updateInfoActiveDevice();

#endif /* __TIME_CHECK_H */

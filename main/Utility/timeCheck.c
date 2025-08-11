/**
 ******************************************************************************
 * @file    timeCheck.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "timeCheck.h"
#include "FlashHandler.h"
#include "myCronJob.h"
#include "MqttHandler.h"
#include "OutputControl.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Time_Check"

#ifndef DISABLE_LOG_ALL
#define TIME_CHECK_LOG_INFO_ON
#endif

#ifdef TIME_CHECK_LOG_INFO_ON
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
extern char g_product_Id[30];

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool begin_active_device = false, end_active_device = false;
bool reset_schedule = false;
struct tm timeLocal;
schedule_t mySchedule;
infoDataActiveDevice_t wait_time_active, time_actived;

/*******************************************************************************
 * Declare Timer
 ******************************************************************************/
esp_timer_handle_t periodic_timer, schedule_timer;

const esp_timer_create_args_t periodic_timer_args = {
    .callback = &activeDevice,
    /* name is optional, but may help identify the timer when debugging */
    .name = "periodic"
};

const esp_timer_create_args_t my_schedule = {
    .callback = &mySheduleExecute,
    /* name is optional, but may help identify the timer when debugging */
    .name = "mySchedule"
};

/*******************************************************************************
 * Init & Deinit Timer
 ******************************************************************************/
void timerActiveDeviceStart()
{
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000)); // a periodic timer which will run every 1s
	log_warning("Start timer active device");
}

void timerActiveDeviceStop()
{
	ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
    ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
	log_warning("Stop timer active device");
}

void myScheduleStart()
{
	ESP_ERROR_CHECK(esp_timer_create(&my_schedule, &schedule_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(schedule_timer, 1000000)); // a periodic timer which will run every 1s
	log_warning("Start timer schedule");
}

void myScheduleStop()
{
	ESP_ERROR_CHECK(esp_timer_stop(schedule_timer));
    ESP_ERROR_CHECK(esp_timer_delete(schedule_timer));
	log_warning("Stop timer schedule");
}

/*******************************************************************************
 * Timer Active Device
 ******************************************************************************/
void activeDevice(void* arg)
{
	if (!begin_active_device) {
		wait_time_active.second++;
    	if (wait_time_active.second == 60) {
        	wait_time_active.second = 0;
        	wait_time_active.minute++;
    	}
    	if (wait_time_active.minute == 60) {
        	wait_time_active.minute = 0;
        	wait_time_active.hour++;
    	}
		if (wait_time_active.hour == 48) {
			updateInfoActiveDevice();
			begin_active_device = true;
			Flash_saveBeginActive();
			Flash_saveWaitTimeActive();
			log_warning("Device used enough 48h");
		}
	} else {
		time_actived.second++;
    	if (time_actived.second == 60) {
        	time_actived.second = 0;
        	time_actived.minute++;
    	}
    	if (time_actived.minute == 60) {
        	time_actived.minute = 0;
        	time_actived.hour++;
    	}
		
		if (end_active_device) {
			updateInfoActiveDevice();
			Flash_saveEndActived();
			Flash_saveTimeActived();
			log_info("Device SN: %s actived success", g_product_Id);
			log_info("End timer active device");
			timerActiveDeviceStop();
		} else if ((time_actived.hour % 2) == 0 && (time_actived.minute == 0) && (time_actived.second == 0)) {
			updateInfoActiveDevice();
			Flash_saveTimeActived();
		}
	}
}

/*******************************************************************************
 * Timer Schedule
 ******************************************************************************/
void mySheduleExecute(void* arg)
{
	for (uint8_t sch_count = 0; sch_count < mySchedule.totalJob; sch_count++) {
		if (mySchedule.myTimeSetting[sch_count].stateActive == 1) {
			if ((mySchedule.myTimeSetting[sch_count].hour == timeLocal.tm_hour) && (mySchedule.myTimeSetting[sch_count].minute == timeLocal.tm_min) && (timeLocal.tm_sec < 10)) {
				if (myCronJob_checkSchNoRepeat(mySchedule.myTimeSetting[sch_count].dayOfMonth, mySchedule.myTimeSetting[sch_count].month, mySchedule.myTimeSetting[sch_count].dayOfWeek)) {
					uint8_t temp_mday = atoi(mySchedule.myTimeSetting[sch_count].dayOfMonth);
					uint8_t temp_month = atoi(mySchedule.myTimeSetting[sch_count].month);
					if ((temp_mday == timeLocal.tm_mday) && (temp_month == (timeLocal.tm_mon + 1))) {
						log_warning("Schedule No Repeat");
						for (uint8_t cnt_cmd = 0; cnt_cmd < mySchedule.myTimeSetting[sch_count].numCommand; cnt_cmd++)
						{
							// Out_setRelay(mySchedule.myTimeSetting[sch_count].myListId[cnt_cmd].state);
						}
						mySchedule.myTimeSetting[sch_count].stateActive = 0;
						FlashHandler_saveMyJobsInStore();
						myCronJob_reportJobCurrent(sch_count);
					}
				} else if (myCronJob_checkAlwayRepeat(mySchedule.myTimeSetting[sch_count].dayOfMonth, mySchedule.myTimeSetting[sch_count].month, mySchedule.myTimeSetting[sch_count].dayOfWeek)) {
					if (mySchedule.myTimeSetting[sch_count].second == 0) {
						log_warning("Schedule Alway Repeat");
						for (uint8_t cnt_cmd = 0; cnt_cmd < mySchedule.myTimeSetting[sch_count].numCommand; cnt_cmd++)
						{
							// Out_setRelay(mySchedule.myTimeSetting[sch_count].myListId[cnt_cmd].state);
						}
						mySchedule.myTimeSetting[sch_count].second = 1;
						myCronJob_reportJobCurrent(sch_count);
					}
				} else if (myCronJob_checkWeekDayRepeat(mySchedule.myTimeSetting[sch_count].dayOfMonth, mySchedule.myTimeSetting[sch_count].month, mySchedule.myTimeSetting[sch_count].dayOfWeek)) {
					if (mySchedule.myTimeSetting[sch_count].second == 0) {
						if (myCronJob_findWeekday(mySchedule.myTimeSetting[sch_count].dayOfWeek, timeLocal.tm_wday)) {
							log_warning("Schedule Weekday Repeat");
							for (uint8_t cnt_cmd = 0; cnt_cmd < mySchedule.myTimeSetting[sch_count].numCommand; cnt_cmd++)
							{
								// Out_setRelay(mySchedule.myTimeSetting[sch_count].myListId[cnt_cmd].state);
							}
							mySchedule.myTimeSetting[sch_count].second = 1;
							myCronJob_reportJobCurrent(sch_count);
						}
					}
				}
			}
		}
	}
	// report daily (23h59 daily)
	if ((timeLocal.tm_hour == 23) && (timeLocal.tm_min == 59)) {
		if ((timeLocal.tm_sec) < 10 && !reset_schedule) {
			reset_schedule = true;
		} else if ((timeLocal.tm_sec) > 10 && reset_schedule) {
			reset_schedule = false;
			myCronJob_resetSeconds();
		}
	}
}

/*******************************************************************************
 * Flash Handle Functions
 ******************************************************************************/
void Flash_loadDataActiveDevice()
{
	FlashHandler_getData(NAME_SPACE_ACTIVE_DEVICE, KEY_BEGIN_ACTIVE_DEVICE, &begin_active_device);
	FlashHandler_getData(NAME_SPACE_ACTIVE_DEVICE, KEY_END_ACTIVE_DEVICE, &end_active_device);
	FlashHandler_getData(NAME_SPACE_ACTIVE_DEVICE, KEY_WAIT_TIME_ACTIVE, &wait_time_active);
	FlashHandler_getData(NAME_SPACE_ACTIVE_DEVICE, KEY_SAVE_TIME_ACTIVE, &time_actived);
	
	printf("/**********---------- Program Start ----------**********/\n");
	printf("State_Begin: [%s] -> Time_Wait_Active: \"%lu:%d:%d\"\n", begin_active_device == true ? "Yes" : "No", 
																		wait_time_active.hour, 
																		wait_time_active.minute, 
																		wait_time_active.second);
	printf("State_End: [%s] -> Time_Actived: \"%lu:%d:%d\"\n", end_active_device == true ? "Yes" : "No", 
																		time_actived.hour, 
																		time_actived.minute, 
																		time_actived.second);
	printf("/**********-----------------------------------**********/\n");
}

bool Flash_saveBeginActive()
{
    if (FlashHandler_setData(NAME_SPACE_ACTIVE_DEVICE, KEY_BEGIN_ACTIVE_DEVICE, &begin_active_device, sizeof(begin_active_device))) {
        // log_info("Set begin active success");
		return true;
    } else {
        log_error("Set begin active fail");
		return false;
    }
}

bool Flash_saveEndActived()
{
    if (FlashHandler_setData(NAME_SPACE_ACTIVE_DEVICE, KEY_END_ACTIVE_DEVICE, &end_active_device, sizeof(end_active_device))) {
        // log_info("Set end actived success");
		return true;
    } else {
        log_error("Set end actived fail");
		return false;
    }
}

bool Flash_saveWaitTimeActive()
{
    if (FlashHandler_setData(NAME_SPACE_ACTIVE_DEVICE, KEY_WAIT_TIME_ACTIVE, &wait_time_active, sizeof(wait_time_active))) {
        // log_info("Set wait time active success");
		return true;
    } else {
        log_error("Set wait time active fail");
		return false;
    }
}

bool Flash_saveTimeActived()
{
    if (FlashHandler_setData(NAME_SPACE_ACTIVE_DEVICE, KEY_SAVE_TIME_ACTIVE, &time_actived, sizeof(time_actived))) {
        // log_info("Set time actived success");
		return true;
    } else {
        log_error("Set time actived fail");
		return false;
    }
}

/*******************************************************************************
 * Application Functions
 ******************************************************************************/
bool checkRealTimeLocal()
{
	if (timeLocal.tm_year > (2023 - 1900)) {
		return true;
	} else {
		return false;
	}
}

void updateInfoActiveDevice()
{
	char data[50];			
	sprintf(data,"{\"time\":\"%lu:%02d:00\",\"confirm\":\"%d\"}", time_actived.hour, time_actived.minute, end_active_device);
	MQTT_PublishTimeActiveDevice(data);
}

/***********************************************/
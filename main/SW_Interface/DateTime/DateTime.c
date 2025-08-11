/**
 ******************************************************************************
 * @file    DateTime.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "DateTime.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Date_Time"

#ifndef DISABLE_LOG_ALL
#define DATE_TIME_LOG_INFO_ON
#endif

#ifdef DATE_TIME_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#define log_warning(format, ...)
#endif

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
void time_logDateTime(time_t now)
{
    char strftime_buf[64];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    log_warning("The time log is: %s", strftime_buf);
}

void setTimeZone()
{
    time_t now;
    struct tm timeinfo;
    time(&now);

    char strftime_buf[64];
    setenv("TZ", "UTC-07:00", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    log_info("The current date/time in VN is: %s", strftime_buf);
}

void time_sync_notification_cb(struct timeval *tv)
{
    printf("sntp syn status: %d\n", sntp_get_sync_status());
    log_info("Notification of a time synchronization event");
    time_logDateTime(tv->tv_sec);
}

static void initialize_sntp()
{
    log_warning("Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

static void obtain_time()
{
    initialize_sntp();
}

void dateTime_init()
{
    setTimeZone();
    time_t now;
    struct tm timeinfo;
    time(&now);

    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2023 - 1900)) {
        log_info("Time is not set yet. Connecting to WiFi and getting time over NTP.");
    }
    obtain_time();
}

void Date_Time_Initialize()
{
    dateTime_init();
}

/***********************************************/
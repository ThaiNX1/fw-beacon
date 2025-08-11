/**
 ******************************************************************************
 * @file    Global.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __GLOBAL_H
#define __GLOBAL_H

/* Includes ------------------------------------------------------------------*/
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp32/rom/rtc.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mbedtls/md5.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <esp_err.h>
#include <math.h> 
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "cJSON.h"

/* Exported types ------------------------------------------------------------*/
/* Extern variables ----------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define DEVICE_NAME         "SMART_DEVICE_BEACON"
#define MODEL_NAME          "HT-SB-3T-WB-01"

#define COMMON_VER          (3003)
#define VER_STRING          "3.0.0.3"
#define FIRM_VER            (2003)
#define HARD_VER            (1)
#define MODEL_VER_NUMBER    (1)     

#define DEVICE_PWD          "HT_EZLife"
#define DEVICE_KEY_SIGN     "j7TnA2l!eVq#Xp$5rGhZ9bC1@MdWu3Yf"

#define PASSWORD_LEN        (20)
#define PRODUCT_ID_LEN      (30)

#define ESP_ID              0x02E5
#define NORDIC_ID           0x0059
#define APPLE_ID            0x004C

#endif /* __GLOBAL_H */

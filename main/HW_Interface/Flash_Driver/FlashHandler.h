/**
 ******************************************************************************
 * @file    FlashHandler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __FLASH_HANDLER_H
#define __FLASH_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define NAMESPACE_GENARAL           "DEVICE_GENARAL"
#define KEY_DEVICE_INFO_ID_PWS      "InfoIdPassword"
#define KEY_CERTKEY_MQTT            "mqttCertkey"
#define KEY_ENVIR_MQTT              "mqttEnvir"
#define KEY_SCHEDULE                "schedule"
#define KEY_USER_ID                 "userId"

/* Exported functions ------------------------------------------------------- */
void Flash_Initialize();
void FlashHandler_initInfoSwitch();
void FlashHandler_resetUserSetting();
void FlashHandler_getDataOtaInStore();
bool FlashHandler_getData(char* nameSpace, char* key, void* dataStore);
bool FlashHandler_setData(char* nameSpace, char* key, void* dataStore, size_t dataSize);
bool FlashHandler_eraseData(char* nameSpace, const char* key);
bool FlashHandler_getDeviceInfoInStore();
bool FlashHandler_saveDeviceInfoInStore();
bool FlashHandler_getUserData();
bool FlashHandler_saveUserData();
bool FlashHandler_getCertKeyMqttInStore();
bool FlashHandler_saveCertKeyMqttInStore();
bool FlashHandler_getEnvironmentMqttInStore();
bool FlashHandler_saveEnvironmentMqttInStore();
bool FlashHandler_getMyJobsInStore();
bool FlashHandler_saveMyJobsInStore();

#endif /* __FLASH_HANDLER_H */

/**
 ******************************************************************************
 * @file    gateway_config.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __GATEWAY_CONFIG_H
#define __GATEWAY_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
typedef struct 
{
    bool checkStateWifiDefault;
    bool checkFactoryDefault;
} infoFactoryDefault_t;

/* Exported macro ------------------------------------------------------------*/
#define TIME_OUT_HARD_RESET         180000
#define HARD_RESET_KEY_WORD         "reset_ok"

#define NAMESPACE_GATEWAY_FLASH     "gateway_f"
#define KEY_GATEWAY_DEFAULT         "g_default"

#define NAMESPACE_RETURN_WIFI       "return"
#define KEY_RETURN_WIFI             "wifi"

/* Exported functions ------------------------------------------------------- */
void GatewayConfig_init();
void GatewayConfig_hardResetInit();
void GatewayConfig_hardResetInfo(const char *hardReset);
void GatewayConfig_removeUser(uint32_t user_ID);
void GatewayConfig_haveWifiFromBLE(char *ssid, char *password);
void GatewayConfig_wifiConnectDone();
void GatewayConfig_receivedBleDone();
void GatewayConfig_receivedHardResetDone();
void GatewayConfig_haveUserInfo(const char *userID);
bool GatewayConfig_checkUserId(uint32_t user_ID);

bool Flash_saveConfigWifi();
void Flash_loadConfigWifi();
void Flash_deleteConfigWifi();
bool Flash_saveInfoFactoryDefault();
void Flash_loadInfoFactoryDefault();
void Flash_deleteInfoFactoryDefault();
void Flash_setModeDefault();

#endif /* __GATEWAY_CONFIG_H */

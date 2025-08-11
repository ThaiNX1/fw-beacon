/**
 ******************************************************************************
 * @file    gateway_config.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "gateway_config.h"
#include "Wifi_Handler.h"
#include "FlashHandler.h"
#include "WatchDog.h"
#include "BLE_handler.h"
#include "MqttHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Gateway_Config"

#ifndef DISABLE_LOG_ALL
#define GATEWAY_CONFIG_LOG_INFO_ON
#endif

#ifdef GATEWAY_CONFIG_LOG_INFO_ON
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
extern uint32_t g_user_ID;

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool isWifiCofg = false;
bool isHardReset = false;
bool state_bleWifi = false;
bool g_newInfoWifiPass = false;
bool check_userId = true;
uint32_t userIdTemp = 0;

infoFactoryDefault_t infoFactoryDefault = {
    .checkStateWifiDefault = true,
    .checkFactoryDefault = true,
};

/*******************************************************************************
 * BLE Handler Callback
 ******************************************************************************/
void GatewayConfig_haveWifiFromBLE(char *ssid, char *password)
{
	if (!check_userId) {
        log_error("Check User ID Fail\n");
		BLE_sentToMobile(BLE_MES_USER_FALSE);
        return;
    }
	g_newInfoWifiPass = true;
	log_warning("Have wifi, ssid: %s, password: %s\n", ssid, password);
	Wifi_startConnect(ssid, password);
}

void GatewayConfig_haveUserInfo(const char *userID)
{
	log_info("User ID: %s\n", userID);
	userIdTemp =  atoi(userID);
	if (GatewayConfig_checkUserId(userIdTemp)) {
		printf("Check User ID OK\n");
		check_userId = true;
	} else {
		log_error("Check User ID Fail\n");
		check_userId = false;
	}
}

bool GatewayConfig_checkUserId(uint32_t user_ID)
{
	if (g_user_ID == 0) {
		log_warning("User ID Yet Setting\n");
		BLE_sentToMobile(BLE_MES_USER_OK);
		return true;
	} else if (g_user_ID != user_ID) {
		log_error("User ID Incorrect\n");
		BLE_sentToMobile(BLE_MES_USER_FALSE);
		return false;
	} else {
		printf("User ID Correct\n");
		BLE_sentToMobile(BLE_MES_USER_OK);
		return true;
	}
}

void GatewayConfig_removeUser(uint32_t user_ID)
{
	printf("User ID: %lu\n", user_ID);
	if (isHardReset) {
		log_error("Hard reset is running\n");
		return;
	} else if (user_ID == 0) {
		log_error("User ID Invalid\n");
	} else if (user_ID != g_user_ID) {
		log_error("User ID Incorrect\n");
	} else {
		printf("User ID Correct\n");
		FlashHandler_resetUserSetting();
	}
}

void GatewayConfig_receivedBleDone()
{
	printf("received BLE done\n");
	if (infoFactoryDefault.checkFactoryDefault && infoFactoryDefault.checkStateWifiDefault) {
		log_warning("Set State Wifi Default Success\n");
		infoFactoryDefault.checkStateWifiDefault = false;
		return;
	}

	ESP_resetChip();
}

void GatewayConfig_receivedHardResetDone()
{
	printf("received hard reset done\n");
	ESP_resetChip();
}

/*******************************************************************************
 * Wifi Handler Callback
 ******************************************************************************/
void GatewayConfig_wifiConnectDone()
{
	printf("wifi connect done\n");
	if (isWifiCofg || infoFactoryDefault.checkFactoryDefault) {
		if (g_newInfoWifiPass) {
			log_warning("Have a new info wifi\n");
			state_bleWifi = true;
			BLE_sentToMobile(BLE_MES_WIFI_OK);
		}
	}
}

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
void GatewayConfig_init()
{
	printf("gateway config init\n");
	if (infoFactoryDefault.checkFactoryDefault) {
		log_error("Mode Config wifi is running\n");
		return;
	} else if (isWifiCofg) {
		log_error("Config wifi is running\n");
		return;
	}
	
	vTaskDelay(1000/portTICK_PERIOD_MS);
	isWifiCofg = true;
	BLE_reAdvertising();
}

void GatewayConfig_hardResetInit()
{
	printf("Hard Reset Init\n");
	if (isHardReset) {
		log_error("Hard Reset is running\n");
		return;
	}
	vTaskDelay(1000/portTICK_PERIOD_MS);
	isHardReset = true;
	BLE_startModeHardReset();
}

void GatewayConfig_hardResetInfo(const char *hardReset)
{
	if (strcmp(hardReset, HARD_RESET_KEY_WORD) != 0) {
		printf("Key Word Incorrect\n");
		BLE_sentToMobile(BLE_MES_RESET_FALSE);
		return;
	}
	FlashHandler_resetUserSetting();
	printf("Hard Reset Done\n");
	vTaskDelay(1000/portTICK_PERIOD_MS);
	BLE_sentToMobile(BLE_MES_RESET_OK);
}

/*******************************************************************************
 * Flash Funtions Callback
 ******************************************************************************/
bool Flash_saveConfigWifi()
{
    if (FlashHandler_setData(NAMESPACE_RETURN_WIFI, KEY_RETURN_WIFI, &isWifiCofg, sizeof(isWifiCofg))) {
        // log_info("Set config wifi success");
		return true;
    } else {
        log_error("Set config wifi fail");
		return false;
    }
}

void Flash_loadConfigWifi()
{
    FlashHandler_getData(NAMESPACE_RETURN_WIFI, KEY_RETURN_WIFI, &isWifiCofg);
	printf("Wifi_Config_Mode: %d\n", isWifiCofg);
}

void Flash_deleteConfigWifi()
{
    FlashHandler_eraseData(NAMESPACE_RETURN_WIFI, KEY_RETURN_WIFI);
}

bool Flash_saveInfoFactoryDefault()
{
    if (FlashHandler_setData(NAMESPACE_GATEWAY_FLASH, KEY_GATEWAY_DEFAULT, &infoFactoryDefault, sizeof(infoFactoryDefault))) {
        // log_info("Set info repeater success");
        return true;
    } else {
        log_error("Set info repeater fail");
        return false;
    }
}

void Flash_loadInfoFactoryDefault()
{
    FlashHandler_getData(NAMESPACE_GATEWAY_FLASH, KEY_GATEWAY_DEFAULT, &infoFactoryDefault);
    printf("State_Default: %d, Factory_Default: %d\n", infoFactoryDefault.checkStateWifiDefault, infoFactoryDefault.checkFactoryDefault);
}

void Flash_deleteInfoFactoryDefault()
{
    FlashHandler_eraseData(NAMESPACE_GATEWAY_FLASH, KEY_GATEWAY_DEFAULT);
}

void Flash_setModeDefault()
{
    infoFactoryDefault.checkFactoryDefault = false;
	Flash_saveInfoFactoryDefault();
    ESP_resetChip();
}

/***********************************************/

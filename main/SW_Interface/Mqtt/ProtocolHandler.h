/**
 ******************************************************************************
 * @file    ProtocolHandler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __PROTOCOL_HANDLER_H
#define __PROTOCOL_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
typedef struct
{
	uint16_t versionEspOld;
} versionFwOld;

typedef struct 
{
	bool state;
	char dataSend[100];
} confirmEndOta;

/* Exported macro ------------------------------------------------------------*/
#define EVT_UPDATE_PROPERTY 	"UP"
#define EVT_CONTROL_PROPERTY 	"CP"
#define EVT_UPDATE_FIRMWARE 	"UF"

#define PROPERTY_CODE_S_SWITCH   			"S_SWITCH"
#define PROPERTY_CODE_S_LOCKTOUCH   		"S_LOCKTOUCH"
#define PROPERTY_CODE_S_MODE   				"S_MODE"
#define PROPERTY_CODE_DEVICE_INFO  			"DEVICE_INFO"
#define PROPERTY_CODE_WIFI_INFO  			"WIFI_INFO"
#define PROPERTY_CODE_BEACON_INFO  			"INFO_BEACON"
#define PROPERTY_CODE_ACTIVE_DEVICE  		"ACTIVE_DEVICE"
#define PROPERTY_CODE_LINK_FIRMWARE  		"LINK_UPDATE"
#define PROPERTY_CODE_PROCESS_OTA  			"PROCESS_OTA"
#define PROPERTY_CODE_SCHEDULE  			"SCHEDULE"
#define PROPERTY_CODE_SCHEDULE_CURRENT  	"SCHEDULE_CURRENT"

#define NAME_VERSION_FW_OLD 			"ver_fw_old"
#define NAME_VERSION_FW_ESP 			"ver_fw_esp"
#define NAME_CONFIRM_END_OTA 			"cf_end_ota"
#define KEY_VERSION_FW_OLD 				"update_fw"

#define MAX_LEN_COMMON 					12
#define MIN_LEN_COMMON 					9
#define MAX_LEN_ESP 					10
#define MIN_LEN_ESP 					8

/* Exported functions ------------------------------------------------------- */
void sendStatusCronjob(uint8_t valueSend);
void checkEndOtaFromServer();
void checkUpdateVerFwEsp();
void reportOtaCheckDataInvalid();
void reportOtaFailure();

void ht_processCmd(char *data);
void ht_processCertificate(char* topic, char* data);

bool Flash_saveOldVersionFirmware();
bool Flash_saveStateOtaEsp();
bool Flash_saveStateEndOta();

#endif /* __PROTOCOL_HANDLER_H */
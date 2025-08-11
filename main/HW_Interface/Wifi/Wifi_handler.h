/**
 ******************************************************************************
 * @file    Wifi_handler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __WIFI_HANDLER_H
#define __WIFI_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Extern Variables --------------------------------------------------------- */
extern EventGroupHandle_t wifi_event_group;

/* Exported types ------------------------------------------------------------*/
typedef enum
{
	Wifi_State_None,
	Wifi_State_Started,
	Wifi_State_Got_IP,
} Wifi_State;

/* Exported macro ------------------------------------------------------------*/
#define CONNECTED_BIT BIT0
/*	WIFI connected bit this bit is clear when 
		1, Wifi is not conected 
		2, In wifi config mode
 */ 

#define WIFI_LIST_MAX_LEN 			500
#define WIFI_RECONNECT_INTERVAL 	30000
#define WIFI_RECONNECT_NEW_WIFI 	10000
#define TIME_OUT_CONFIG_WIFI 		180000
#define TIME_OUT_GET_DATA_WIFI 		40000

#define WIFI_HANDLER_IS_CONECTED (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)
#define WIFI_HANDLER_WAIT_CONECTED_NOMAL_FOREVER while((xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY) & CONNECTED_BIT) == 0)

/* Exported functions ------------------------------------------------------- */
void Wifi_Initialize();
void Wifi_start();
void Wifi_stop();
void Wifi_startConnect(char* ssid, char*password);
void Wifi_reConnect();
int Wifi_checkRssi();
void Wifi_startScan();
void Wifi_startConfigMode();
void Wifi_getMacStr();
void setProductId_defaultMac();
void Wifi_retryToConnect();
void Wifi_updateInfoWifi();
void Wifi_setStateDefault();
Wifi_State getWifiState();

#endif /* __WIFI_HANDLER_H */

/**
 ******************************************************************************
 * @file    BLE_handler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __BLE_HANDLER_H
#define __BLE_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define BLE_MES_WIFI_OK                 "Wifi_OK"

#define BLE_MES_RESET_OK                "Reset_OK"
#define BLE_MES_RESET_FALSE             "Reset_FALSE"

#define BLE_MES_USER_OK                 "User_OK"
#define BLE_MES_USER_EXIST              "User_EXIST"
#define BLE_MES_USER_FALSE              "User_FALSE"

/* Exported functions ------------------------------------------------------- */
void BLE_init();
void BLE_startConfigMode();
void BLE_startControlMode();
void BLE_sentToMobile(const char *sentMes);
void BLE_reAdvertising();
void BLE_releaseBle();
void BLE_startModeHardReset();
void BLE_iBeaconSetSpecial(uint16_t cid, uint16_t major, uint16_t minor);

#endif /* __BLE_HANDLER_H */

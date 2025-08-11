/**
 ******************************************************************************
 * @file    MqttHandler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __MQTT_HANDLER_H
#define __MQTT_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
enum s_topic_filter
{
    EVENT_TYPE,
    PROPERTY_CODE
};

typedef struct 
{
    char cert[1500];
    char key[2000];
} mqtt_certKey_t;

typedef struct 
{
    char environment[10];
} mqtt_environment_t;

/* Exported macro ------------------------------------------------------------*/
#define USER_NAME_MQTT_HT_EZLIFE     "HT_EZLife"

#define AWS_PHASE_PRODUCTION
#define ENVIR_STR    "PROD"

// #define AWS_PHASE_DEVERLOP
// #define ENVIR_STR    "DEV"

// #define AWS_PHASE_STAGING
// #define ENVIR_STR    "STG"

/* Exported functions ------------------------------------------------------- */
void MQTT_Initialize();
void MQTT_Start();
void MQTT_Stop();
void MQTT_restartClient();
void MQTT_connectToServerDone();

int MQTT_SubscribeToDeviceTopic(char *product_Id);
int MQTT_SubscribeTopicGetPrivateKey();
int MQTT_PublishStatusGetPrivateKey(char* productId);
int MQTT_SubscribeTopicGetCert();
int MQTT_PublishRequestGetCert(char* signature);
int MQTT_SubscribeTopicConfirmCert();
int MQTT_PublishStatusConfirmCert();
void MqttHandle_startCheckNewCertificate();

void MQTT_PublishVersion(uint16_t model, uint16_t hardver, uint16_t commonVer, uint16_t firmver);
void MQTT_PublishSwitchState(uint8_t id, uint8_t state);
void MQTT_PublishInfoBeacon(uint16_t major);
void MQTT_PublishTimeActiveDevice(char* data);
void MQTT_PublishInfoWifi(char* data);
void MQTT_PublishStateUpdateFirmware(char* data);
void MQTT_PublishDataCommon(char* data, char* property);
void MQTT_PublishData(char* data, char* property);

#endif /* __MQTT_HANDLER_H */

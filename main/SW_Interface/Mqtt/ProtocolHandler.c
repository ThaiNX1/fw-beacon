/**
 ******************************************************************************
 * @file    ProtocolHandler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "ProtocolHandler.h"
#include "MqttHandler.h"
#include "BLE_handler.h"
#include "HttpHandler.h"
#include "FlashHandler.h"
#include "WatchDog.h"
#include "timeCheck.h"
#include "gateway_config.h"
#include "OTA_http.h"
#include "myCronJob.h"
#include "OutputControl.h"
#include "HTG_Utility.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Protocol_Handler"

#ifndef DISABLE_LOG_ALL
#define PROTOCOL_HANDLER_LOG_INFO_ON
#endif

#ifdef PROTOCOL_HANDLER_LOG_INFO_ON
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
extern bool begin_active_device, end_active_device;
extern bool needCheckVersionEspOTA;
extern bool g_mqttHaveNewCertificate;
extern char topic_filter[2][30];
extern char g_product_Id[PRODUCT_ID_LEN];

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool s_taskUpdateFirmware = false;
bool checkStateOtaEsp = false;
char *s_linkDownload = NULL;
char *s_signature = NULL;

uint16_t lenCert = 0;
uint16_t lenKey = 0;

versionFwOld versionFwOld_t = {
	.versionEspOld = 0,
};
confirmEndOta confirmEndOta_t = {
	.state = false,
	.dataSend = {0},
};
mqtt_certKey_t *pMqttCertKey = NULL;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void sendStatusCronjob(uint8_t valueSend)
{
	char data[10]= "";
	sprintf(data, "%d", valueSend);
	MQTT_PublishData((char *)data, PROPERTY_CODE_SCHEDULE);
}

/*******************************************************************************
 * Task Process Update Firmware
 ******************************************************************************/
static void task_processUF(void *arg)
{
	s_taskUpdateFirmware = true;
	log_warning("Begin task process update firmware");

	log_warning(" --> OTA For ESP");
	char data_send[100] = "{\"Begin\":1,\"End\":0,\"Status\":\"Check_Data_Valid\"}";
	MQTT_PublishStateUpdateFirmware(data_send);

	needCheckVersionEspOTA = false;
	checkStateOtaEsp = true;
	Flash_saveStateOtaEsp();

	OTA_http_DoOTA(s_linkDownload, PHASE_OTA_ESP);

	vTaskDelay(2000/portTICK_PERIOD_MS);

	log_warning("Delete task process update firmware");
    vTaskDelete(NULL);
}

void progressUpdateFirmware()
{
	if (s_taskUpdateFirmware) {
		log_error("Task process update firmware is running");
		return;
	}

	confirmEndOta_t.state = false;
	strcpy(confirmEndOta_t.dataSend, "");
	Flash_saveStateEndOta();

	versionFwOld_t.versionEspOld = FIRM_VER;
	Flash_saveOldVersionFirmware();

	BLE_releaseBle();
	vTaskDelay(1000/portTICK_PERIOD_MS);

	xTaskCreate(task_processUF, "task_processUF", 4*1024, NULL, 5, NULL);
}

static void task_checkStateServer(void *arg) 
{
	uint8_t delayTimeSendStateOta = 0;

	log_warning("Begin task check state server");
	while (confirmEndOta_t.state)
	{
		if (delayTimeSendStateOta == 0) {
			MQTT_PublishStateUpdateFirmware(confirmEndOta_t.dataSend);
		}
		if (delayTimeSendStateOta >= 60) {
			delayTimeSendStateOta = 0;
		}
		delayTimeSendStateOta++;
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}

	log_warning("Delete task check state server");
	vTaskDelay(100/portTICK_PERIOD_MS);
	vTaskDelete(NULL);
}

void checkEndOtaFromServer()
{
	xTaskCreate(task_checkStateServer, "task_checkStateServer", 2048, NULL, 5, NULL);
}

void checkUpdateVerFwEsp()
{
    if (versionFwOld_t.versionEspOld != FIRM_VER) {
		char data_send[100] = "{\"Begin\":0,\"End\":1,\"Status\":\"Ota_Success\"}";
	    MQTT_PublishStateUpdateFirmware(data_send);

		confirmEndOta_t.state = true;
		strcpy(confirmEndOta_t.dataSend, data_send);
		Flash_saveStateEndOta();

        versionFwOld_t.versionEspOld = FIRM_VER;
        Flash_saveOldVersionFirmware();

		checkStateOtaEsp = false;
    	Flash_saveStateOtaEsp();
    } else {
		reportOtaFailure();
	}
}

void reportOtaCheckDataInvalid()
{
	char data_send[100] = "{\"Begin\":0,\"End\":1,\"Status\":\"Check_Data_Invalid\"}";
	MQTT_PublishStateUpdateFirmware(data_send);

	if (s_linkDownload != NULL) {
		free(s_linkDownload);
		s_linkDownload = NULL;
	}
	if (s_signature != NULL) {
		free(s_signature);
		s_signature = NULL;
	}
}

void reportOtaFailure()
{
	char data_send[100] = "{\"Begin\":0,\"End\":1,\"Status\":\"Ota_Fail\"}";
	MQTT_PublishStateUpdateFirmware(data_send);

	confirmEndOta_t.state = true;
	strcpy(confirmEndOta_t.dataSend, data_send);
	Flash_saveStateEndOta();

	checkStateOtaEsp = false;
	Flash_saveStateOtaEsp();
	ESP_resetChip();
}

/*******************************************************************************
 * Process Data Receive From Server
 ******************************************************************************/
void ht_processCmd(char *data)
{
	cJSON *msgObject = cJSON_Parse(data);
	if (msgObject == NULL) {
		log_error("CJSON_Parse unsuccess!");
		return;
	}

	if (strcmp(topic_filter[EVENT_TYPE], EVT_CONTROL_PROPERTY) == 0) {
		// event type: control property (CP)
		if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_ACTIVE_DEVICE) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				if (atoi(cJSON_GetObjectItem(msgObject, "d")->valuestring)) {
					end_active_device = true;
				}
			}
		} else if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_S_SWITCH) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				cJSON *dataItem = cJSON_GetObjectItem(msgObject, "d");
				if (cJSON_HasObjectItem(dataItem, "id") && cJSON_HasObjectItem(dataItem, "state")) {
					uint8_t id = atoi(cJSON_GetObjectItem(dataItem, "id")->valuestring);
					uint8_t state = atoi(cJSON_GetObjectItem(dataItem, "state")->valuestring);
					Out_setRelay(id - 1, state);
				}
			}
		}
		else if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_S_LOCKTOUCH) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				if (atoi(cJSON_GetObjectItem(msgObject, "d")->valuestring)) {
					// chưa xử lý
				}
			}
		} else if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_S_MODE) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				if (atoi(cJSON_GetObjectItem(msgObject, "d")->valuestring)) {
					// chưa xử lý
				}
			}
		}
	} else if (strcmp(topic_filter[EVENT_TYPE], EVT_UPDATE_FIRMWARE) == 0) {
		// event type: update firmware (UF)
		if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_LINK_FIRMWARE) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				cJSON *dataItem = cJSON_GetObjectItem(msgObject, "d");
				if (cJSON_HasObjectItem(dataItem, "url") && cJSON_HasObjectItem(dataItem, "signature")) {
					char *url = cJSON_GetObjectItem(dataItem, "url")->valuestring;
					char *sig = cJSON_GetObjectItem(dataItem, "signature")->valuestring;
					
					if (url != NULL && sig != NULL) {
						char key_ota[20] = "";
						uint32_t crc32_sn = ht_check_crc32_sn_device();
						sprintf(key_ota, "HT-%08lx", crc32_sn);
						// log_warning("Key OTA: \"%s\"", key_ota);

						s_linkDownload = (char*)calloc(strlen(url) + 1, 1);
						strcpy(s_linkDownload, url);
						s_signature = (char*)calloc(strlen(sig) + 1, 1);
						strcpy(s_signature, sig);

						log_warning("URL: ");
						printf(" \"%s\"\n", s_linkDownload);
						log_warning("Signature: ");
						printf(" \"%s\"\n", s_signature);

						if (verify_ota_signature(MODEL_NAME, s_linkDownload, s_signature, key_ota)) {
							log_warning("Signature verification successful");
							progressUpdateFirmware();
						} else {
							log_error("OTA signature verification failed");
							reportOtaCheckDataInvalid();
						}
					} else {
						log_error("URL or Signature is NULL");
						reportOtaCheckDataInvalid();
					}
				} else {
					log_error("URL or Signature not found in message");
					reportOtaCheckDataInvalid();
				}
			}
		} else if (strcmp(topic_filter[PROPERTY_CODE], PROPERTY_CODE_PROCESS_OTA) == 0) {
			if (cJSON_HasObjectItem(msgObject, "d")) {
				if (atoi(cJSON_GetObjectItem(msgObject, "d")->valuestring)) {
					confirmEndOta_t.state = false;
					strcpy(confirmEndOta_t.dataSend, "");
					Flash_saveStateEndOta();
				}
			}
		}
	}

	cJSON_Delete(msgObject);
}

/*******************************************************************************
 * Process Certificate
 ******************************************************************************/
void ht_processCertificate(char* topic, char* data)
{
	char* part1 = NULL, *part2 = NULL, *part3 = NULL;
	part1 = strtok(topic, "/");
	if (part1 == NULL) {
		log_error("Part1 is NULL");
		return;
	}
	part2 = strtok(NULL, "/");
	if (part2 == NULL) {
		log_error("Part2 is NULL");
		return;
	}
	part3 = strtok(NULL, "/");
	if (part3 == NULL) {
		log_error("Part3 is NULL");
		return;
	}
	
#ifdef AWS_PHASE_PRODUCTION
	log_warning("%s %s %s", part1, part2, part3);
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
#endif

#ifdef AWS_PHASE_PRODUCTION
	if (strcmp(part2, g_product_Id) != 0) {
		log_error("Product ID mismatch: %s != %s", part2, g_product_Id);
		return;
	}
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
#endif

	cJSON *msgObject = cJSON_Parse(data);
	if (msgObject == NULL) {
		log_error("CJSON_Parse unsuccess!");
		return;
	}

	if (cJSON_HasObjectItem(msgObject, "serialNumber")) {
		if (strcmp(part2, cJSON_GetObjectItem(msgObject, "serialNumber")->valuestring) != 0) {
			log_error("Product ID mismatch: %s != %s", part2, cJSON_GetObjectItem(msgObject, "serialNumber")->valuestring);
			cJSON_Delete(msgObject);
			return;
		}
	} else {
		log_error("Serial number not found in message");
		cJSON_Delete(msgObject);
		return;
	}

	if (strcmp(part3, "privateKey") == 0) {
		if (cJSON_HasObjectItem(msgObject, "privateKey")) {
			ht_gen_signature(cJSON_GetObjectItem(msgObject, "privateKey")->valuestring);
		}
	} else if (strcmp(part3, "getcert") == 0) {
		if (cJSON_HasObjectItem(msgObject, "keyLink") && cJSON_HasObjectItem(msgObject, "certLink")) {
			char* linkCert = cJSON_GetObjectItem(msgObject, "certLink")->valuestring;
			char* linkKey = cJSON_GetObjectItem(msgObject, "keyLink")->valuestring;

			if (linkCert != NULL && linkKey != NULL && pMqttCertKey == NULL) {
				pMqttCertKey = (mqtt_certKey_t*)calloc(1, sizeof(mqtt_certKey_t));
				if (pMqttCertKey != NULL) {
					BLE_releaseBle();
					vTaskDelay(1000/portTICK_PERIOD_MS);
					lenCert = 0;
					lenKey = 0;
					printf("link cert: %s\n", linkCert);
					if (Http_downloadFileCert(linkCert) == true) {
						// printf("%s", pMqttCertKey->cert);
						printf("link key: %s\n", linkKey);

						if (Http_downloadFileKey(linkKey) == true) {
							// printf("%s", pMqttCertKey->key);
							printf("%d %d\n", lenCert, lenKey);
							MqttHandle_startCheckNewCertificate();
						}
					}
				}
			}
		}
	} else if (strcmp(part3, "complete") == 0) {
		if (cJSON_HasObjectItem(msgObject, "status")) {
			if (strcmp(cJSON_GetObjectItem(msgObject, "status")->valuestring, "completed") == 0) {
				printf("confirm ok\n");
				if (FlashHandler_saveCertKeyMqttInStore() == true) {
					FlashHandler_saveEnvironmentMqttInStore();
					g_mqttHaveNewCertificate = true;
					printf("restart esp after 1s\n");
					vTaskDelay(1000/portTICK_PERIOD_MS);
					ESP_resetChip();
				}
			}
		}
	}

	cJSON_Delete(msgObject);
}

/*******************************************************************************
 * Flash Functions
 ******************************************************************************/
bool Flash_saveOldVersionFirmware()
{
    if (FlashHandler_setData(NAME_VERSION_FW_OLD, KEY_VERSION_FW_OLD, &versionFwOld_t, sizeof(versionFwOld_t))) {
        log_info("Set old version firmware success");
		return true;
    } else {
        log_error("Set old version firmware fail");
		return false;
    }
}

bool Flash_saveStateOtaEsp()
{
    if (FlashHandler_setData(NAME_VERSION_FW_ESP, KEY_VERSION_FW_OLD, &checkStateOtaEsp, sizeof(checkStateOtaEsp))) {
        log_info("Set state ota esp success");
		return true;
    } else {
        log_error("Set state ota esp fail");
		return false;
    }
}

bool Flash_saveStateEndOta()
{
    if (FlashHandler_setData(NAME_CONFIRM_END_OTA, KEY_VERSION_FW_OLD, &confirmEndOta_t, sizeof(confirmEndOta_t))) {
        log_info("Set state end ota success");
		return true;
    } else {
        log_error("Set state end ota fail");
		return false;
    }
}

/***********************************************/
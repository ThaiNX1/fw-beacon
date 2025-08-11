/**
 ******************************************************************************
 * @file    MqttHandler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "MqttHandler.h"
#include "gateway_config.h"
#include "HTG_Utility.h"
#include "Wifi_Handler.h"
#include "FlashHandler.h"
#include "ProtocolHandler.h"
#include "OutputControl.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "MQTT_Handler"

#ifndef DISABLE_LOG_ALL
#define MQTT_LOG_INFO_ON
#endif

#ifdef MQTT_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#endif

#define MAX_LEN_MSG 250

/*******************************************************************************
 * Extern Variables
 ******************************************************************************/
extern char g_product_Id[PRODUCT_ID_LEN];
extern char g_password[PASSWORD_LEN];

extern infoFactoryDefault_t infoFactoryDefault;
extern mqtt_certKey_t *pMqttCertKey;

extern const uint8_t client_cert_pem_start[] 		asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] 			asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] 		asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] 			asm("_binary_client_key_end");

extern const uint8_t amazon_root_ca_pem_start[]   	asm("_binary_amazon_root_ca_pem_start");
extern const uint8_t amazon_root_ca_pem_end[]   	asm("_binary_amazon_root_ca_pem_end");

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool g_isMqttConnected = false;
bool g_mqttHaveNewCertificate = false;
bool needConfirmNewCerificate = false;
bool s_isFirstConnected = true;

char topic_filter[2][30] = {0};
int msgIdSubcribeDevice = 0;
int msgIdSubcribeGetPrivateKey = 0;
int msgIdSubConfirmCert = 0;

uint8_t count_error_connect = 0;
uint16_t count_restart_client = 0;

mqtt_certKey_t certKeyMqtt = {"none", "none"};
mqtt_environment_t envir = {"none"};

esp_mqtt_client_handle_t g_mqttCientHandle;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void topic_name_filter(char *data, int len);
void sub_string(char *des, char *src, int start, int end);

/*******************************************************************************
 * Process Properties
 ******************************************************************************/
int MQTT_SubscribeToDeviceTopic(char *product_Id)
{
	if (!g_mqttHaveNewCertificate) {
		return 0;
	}
	char topicName[50] = "";
	subTopicFromProductId(product_Id, topicName);
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, topicName, 0);
    log_info(" Subcribe to topic: %s , id: %d", topicName, msg_id);
	return msg_id;
}

int MQTT_SubscribeTopicGetPrivateKey()
{
	char ptopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(ptopic, "device/%s/privateKey", g_product_Id);
	#endif
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, ptopic, 0);
    log_info(" Subcribe to topic: %s , id: %d", ptopic, msg_id);
	return msg_id;
}

int MQTT_PublishStatusGetPrivateKey(char* productId)
{
	char pData[100] = "";
	char pTopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(pTopic, "device/%s/status", g_product_Id);
	#endif
	sprintf(pData,"{\"serialNumber\":\"%s\",\"status\":\"ready\"}", productId);
	if (!g_isMqttConnected) {
		return -1;
	}
	if (esp_mqtt_client_publish(g_mqttCientHandle, pTopic, pData, strlen(pData), 0, 0) == -1) {
		return -1;
	}
	log_info(" [Device] Publish Topic: %s", pTopic);
	printf(" [Device] Publish Data: %s\n", pData);
	return 0;
}

int MQTT_SubscribeTopicGetCert()
{
	char ptopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(ptopic, "certificates/%s/getcert", g_product_Id);
	#endif
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, ptopic, 0);
    log_info(" Subcribe to topic: %s , id: %d", ptopic, msg_id);
	return msg_id;
}

int MQTT_PublishRequestGetCert(char* signature)
{
	char pData[1024] = "";
	char pTopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(pTopic, "certificates/%s/requestcert", g_product_Id);
	#endif
	sprintf(pData,"{\"serialNumber\":\"%s\",\"signature\":\"%s\"}", g_product_Id, signature);
	if (!g_isMqttConnected) {
		return -1;
	}
	if (esp_mqtt_client_publish(g_mqttCientHandle, pTopic, pData, strlen(pData), 0, 0) == -1) {
		return -1;
	}
	log_info(" [Device] Publish Topic: %s", pTopic);
	printf(" [Device] Publish Data: %s\n", pData);
	return 0;
}

int MQTT_SubscribeTopicConfirmCert()
{
	char ptopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(ptopic, "certificates/%s/complete", g_product_Id);
	#endif
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, ptopic, 0);
    log_info(" Subcribe to topic: %s , id: %d", ptopic, msg_id);
	return msg_id;
}

int MQTT_PublishStatusConfirmCert()
{
	char pData[1024] = "";
	char pTopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	#elif defined(AWS_PHASE_DEVERLOP)
	#elif defined(AWS_PHASE_PRODUCTION)
	sprintf(pTopic, "certificates/%s/confirm", g_product_Id);
	#endif
	sprintf(pData,"{\"serialNumber\":\"%s\",\"status\":\"connected\"}", g_product_Id);
	if (!g_isMqttConnected) {
		return -1;
	}
	if (esp_mqtt_client_publish(g_mqttCientHandle, pTopic, pData, strlen(pData), 0, 0) == -1) {
		return -1;
	}
	log_info(" [Device] Publish Topic: %s", pTopic);
	printf(" [Device] Publish Data: %s\n", pData);
	return 0;
}

static void MQTT_data_cb(esp_mqtt_event_handle_t event)
{
	char *topic = (char *)malloc(event->topic_len + 1);
	memcpy(topic, event->topic, event->topic_len);
	topic[event->topic_len] = 0;
	log_info("[Receive] topic: %s", topic);

	char *data = (char *)malloc(event->data_len + 1);
	memcpy(data, event->data + event->current_data_offset, event->data_len);
	data[event->data_len] = 0;
	printf("[Receive] data [%d/%d bytes]: %s\n", event->data_len + event->current_data_offset, event->total_data_len, data);

	char topic_private[100] = "";
	sprintf(topic_private, "device/%s/privateKey", g_product_Id);

	if ((strcmp(topic, topic_private) == 0) || (strncmp(topic, "certificates", 12) == 0)) {
		ht_processCertificate(topic, data);
	} else {
		topic_name_filter(topic, event->topic_len);
		// log_info("EventType: %s\n", topic_filter[EVENT_TYPE]);
		// log_info("PropertyCode: %s\n", topic_filter[PROPERTY_CODE]);
		ht_processCmd(data);
	}	
	
	free(topic);
	free(data);
}

/*******************************************************************************
 * MQTT Event
 ******************************************************************************/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = event_data;

	switch ((esp_mqtt_event_id_t)event_id) {
	case MQTT_EVENT_CONNECTED:
	{
		log_info("MQTT_EVENT_CONNECTED");
		count_error_connect = 0;
		count_restart_client = 0;

		msgIdSubcribeDevice = MQTT_SubscribeToDeviceTopic(g_product_Id);

		if (!g_mqttHaveNewCertificate && !needConfirmNewCerificate) {
			msgIdSubcribeGetPrivateKey = MQTT_SubscribeTopicGetPrivateKey();
		}

		if (needConfirmNewCerificate) {
			msgIdSubConfirmCert = MQTT_SubscribeTopicConfirmCert();
		}

		g_isMqttConnected = true;
		if (s_isFirstConnected) {
			if (!g_mqttHaveNewCertificate) {
				MQTT_PublishStatusGetPrivateKey(g_product_Id);
			}
			s_isFirstConnected = false;
		}

		MQTT_connectToServerDone();
		Out_publishStateRelayDefault();
		break;
	}
	case MQTT_EVENT_DISCONNECTED:
	{
		log_info("MQTT_EVENT_DISCONNECTED");
		g_isMqttConnected = false;
		break;
	}
	case MQTT_EVENT_SUBSCRIBED:
		log_info("MQTT_EVENT_SUBSCRIBED");
		if (event->msg_id == msgIdSubConfirmCert) {
			if (needConfirmNewCerificate) {
				MQTT_PublishStatusConfirmCert();
				// needConfirmNewCerificate = false;
			}
		}	
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		log_info("MQTT_EVENT_UNSUBSCRIBED,");
		break;
	case MQTT_EVENT_PUBLISHED:
		log_info("MQTT_EVENT_PUBLISHED");
		break;
	case MQTT_EVENT_DATA:
		log_info("MQTT_EVENT_DATA");
		MQTT_data_cb(event);
		break;
	case MQTT_EVENT_ERROR:
		log_info("MQTT_EVENT_ERROR");
		count_error_connect++;
		if (count_error_connect >= 12) {
			MQTT_restartClient();
			count_error_connect = 0;
		}
		break;
	default:
		log_info("Other event id: %ld", event_id);
		break;
	}
}

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
void MQTT_Initialize()
{
	if (FlashHandler_getEnvironmentMqttInStore() == false) {
        FlashHandler_saveEnvironmentMqttInStore();
    }

	if (FlashHandler_getCertKeyMqttInStore() == false) {
		// FlashHandler_saveCertKeyMqttInStore();
	} else {
		if (strcmp(certKeyMqtt.cert,"none") == 0 && strcmp(certKeyMqtt.key,"none") == 0) {
			g_mqttHaveNewCertificate = false;
		} else {
			g_mqttHaveNewCertificate = true;
			if (strcmp(envir.environment, ENVIR_STR) != 0) {
                g_mqttHaveNewCertificate = false;
                printf("Had cert but bad cert\n");
            }
		}
	}

	printf("Cert is have new cert: %s, len cert: %d, len key: %d\n", g_mqttHaveNewCertificate == true ? "yes":"no", strlen(certKeyMqtt.cert), strlen(certKeyMqtt.key));
	if (!g_mqttHaveNewCertificate) {
		strcpy(envir.environment, ENVIR_STR);
		strcpy(certKeyMqtt.cert, (const char*)client_cert_pem_start);
		strcpy(certKeyMqtt.key, (const char*)client_key_pem_start);
		printf("copy cert key default: %d, %d\n", strlen(certKeyMqtt.cert), strlen(certKeyMqtt.key));
	}
	
	const esp_mqtt_client_config_t mqtt_cfg = {
		.broker = {
			#ifdef AWS_PHASE_PRODUCTION
				.address.uri = "mqtts://d06946761osfaa0nlznxq-ats.iot.ap-southeast-1.amazonaws.com:8883",
			#else
			#endif
			.verification.certificate = (const char *)amazon_root_ca_pem_start,
		},
		.credentials.client_id = g_product_Id,
		.credentials.authentication.certificate = (const char *)(certKeyMqtt.cert),
		.credentials.authentication.key = (const char *)(certKeyMqtt.key),
    };
	g_mqttCientHandle = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(g_mqttCientHandle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void MQTT_Start()
{
	if (!g_isMqttConnected) {
		esp_mqtt_client_start(g_mqttCientHandle);
	}
}

void MQTT_Stop()
{
	if (g_isMqttConnected) {
		esp_mqtt_client_stop(g_mqttCientHandle);
	}
}

void MQTT_restartClient()
{
	log_error(" -> Restart MQTT Client When MQTT_EVENT_ERROR");
	Wifi_stop();
	vTaskDelay(2000/portTICK_PERIOD_MS);
	Wifi_start();
	count_restart_client++;
	log_error(" --> %s: %d", __func__, count_restart_client);
}

void MQTT_connectToServerDone()
{
	printf("mqtt connect done\n");
}

int MQTT_PublishToDeviceTopic(char* pubTopicName, char* pubData)
{
	if (!g_mqttHaveNewCertificate) {
		return 0;
	}
	if (!g_isMqttConnected) {
		return -1;
	}
	if (esp_mqtt_client_publish(g_mqttCientHandle, pubTopicName, pubData, strlen(pubData), 0, 0) == -1) {
		return -1;
	}
	log_info(" [Device] Publish Topic: %s", pubTopicName);
	printf(" [Device] Publish Data: %s\n", pubData);

	return 0;
}

void MQTT_PublishVersion(uint16_t model, uint16_t hardver, uint16_t commonVer, uint16_t firmver)
{
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf(pData, "{\"d\":{\"serialNumber\":\"%s\",\"model\":\"%d\",\"hardVer\":\"%d\",\"comVer\":\"%d\",\"firmVer\":\"%d\"}}", g_product_Id, model, hardver, commonVer, firmver);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, PROPERTY_CODE_DEVICE_INFO, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
}

void MQTT_PublishSwitchState(uint8_t id, uint8_t state)
{
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf(pData,"{\"d\":{\"id\":\"%d\",\"state\":\"%d\"}}", id, state);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, PROPERTY_CODE_S_SWITCH, pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
}

void MQTT_PublishInfoBeacon(uint16_t major)
{
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf(pData,"{\"d\":\"%d\"}", major);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, PROPERTY_CODE_BEACON_INFO, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
}

void MQTT_PublishTimeActiveDevice(char* data)
{
	char* pData = (char*)malloc(strlen(data) + 20);
    char pubTopicName[100] = {0};
	sprintf(pData, "{\"d\":%s}", data);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, PROPERTY_CODE_ACTIVE_DEVICE, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
	free(pData);
}

void MQTT_PublishInfoWifi(char* data)
{
	char* pData = (char*)malloc(strlen(data) + 20);
    char pubTopicName[100] = {0};
	sprintf(pData, "{\"d\":%s}", data);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, PROPERTY_CODE_WIFI_INFO, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
	free(pData);
}

void MQTT_PublishStateUpdateFirmware(char* data)
{
	char* pData = (char*)malloc(strlen(data) + 20);
    char pubTopicName[100] = {0};
	sprintf(pData, "{\"d\":%s}", data);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_FIRMWARE, PROPERTY_CODE_PROCESS_OTA, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
	free(pData);
}

void MQTT_PublishDataCommon(char* data, char* property)
{
	char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData, "%s", data);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, property, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
}

void MQTT_PublishData(char* data, char* property)
{
	char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData, "{\"d\":%s}", data);

    pubTopicFromProductId(g_product_Id, EVT_UPDATE_PROPERTY, property, pubTopicName);
    MQTT_PublishToDeviceTopic(pubTopicName, pData);
}

void sub_string(char *des, char *src, int start, int end)
{
    int index = 0;
    for (int i = start + 1; i < end; i++) {
        des[index++] = src[i];
    }
    des[index] = '\0';
}

void topic_name_filter(char *data, int len)
{
	// format topic: device/<SN>/com/event_type/property

    int index[5], count = 0, count_sub = 0;
    for (int i = 0; i < len; i++) {
        if (data[i] == '/') {
            index[count++] = i;
        }
    }
    index[count] = len;

    for (int i = 2; i < count; i++) {
        sub_string(topic_filter[count_sub++], data, index[i], index[i+1]);
    }
}

static void taskCheckFileCertificate(void* arg)
{
	while (1)
	{
		if (pMqttCertKey != NULL) {
			if ((strlen(pMqttCertKey->key) > 0) && (strlen(pMqttCertKey->cert) > 0)) {
				esp_mqtt_client_disconnect(g_mqttCientHandle);
				printf("update config mqtt...\n");
				strcpy(certKeyMqtt.cert, (const char*)pMqttCertKey->cert);
				strcpy(certKeyMqtt.key, (const char*)pMqttCertKey->key);
				printf("start reconnect mqtt...\n");
				esp_mqtt_client_reconnect(g_mqttCientHandle);
				needConfirmNewCerificate = true;
				break;
			} else {
				break;
			}
		} else {
			break;
		}
	}

	if (pMqttCertKey != NULL) {
		free(pMqttCertKey);
		pMqttCertKey = NULL;
	}
	vTaskDelete(NULL);
}

void MqttHandle_startCheckNewCertificate()
{
	xTaskCreate(taskCheckFileCertificate, "taskFileCertificate", 4096, NULL, 5, NULL);
}

/***********************************************/
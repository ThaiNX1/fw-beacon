/**
 ******************************************************************************
 * @file    FlashHandler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "FlashHandler.h"
#include "timeCheck.h"
#include "gateway_config.h"
#include "Wifi_Handler.h"
#include "WatchDog.h"
#include "MqttHandler.h"
#include "ProtocolHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Flash_Handler"

#ifndef DISABLE_LOG_ALL
#define FLASH_HANDLER_LOG_INFO_ON
#endif

#ifdef FLASH_HANDLER_LOG_INFO_ON
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
extern bool checkStateOtaEsp;
extern bool isHardReset;
extern char g_password[PASSWORD_LEN];
extern char g_product_Id[PRODUCT_ID_LEN];
extern uint32_t g_user_ID;

extern versionFwOld versionFwOld_t;
extern confirmEndOta confirmEndOta_t;
extern mqtt_certKey_t certKeyMqtt;
extern mqtt_environment_t envir;
extern schedule_t mySchedule;

/*******************************************************************************
 * Typedef Variables
 ******************************************************************************/
typedef struct
{
    char productId[PRODUCT_ID_LEN];
    char password[PASSWORD_LEN];
} info_id_password_t;

/*******************************************************************************
 * Application Functions
 ******************************************************************************/
void Flash_Initialize()
{
    nvs_flash_init();
    Flash_loadDataActiveDevice();
    Flash_loadInfoFactoryDefault();
    // Flash_loadConfigWifi();
    FlashHandler_getDataOtaInStore();
    // FlashHandler_initInfoSwitch();
}

void FlashHandler_initInfoSwitch()
{
    if (!FlashHandler_getDeviceInfoInStore()) {
        FlashHandler_saveDeviceInfoInStore();      
    }
    if (!FlashHandler_getUserData()) {
        FlashHandler_saveUserData(); 
    }
}

void FlashHandler_resetUserSetting()
{
    // set wifi default
    Wifi_setStateDefault();
    // gateway config data
    FlashHandler_eraseData(NAMESPACE_GATEWAY_FLASH, KEY_GATEWAY_DEFAULT);
    // FlashHandler_eraseData(NAMESPACE_RETURN_WIFI, KEY_RETURN_WIFI);
    // genaral data
    FlashHandler_eraseData(NAMESPACE_GENARAL, KEY_SCHEDULE);
    // FlashHandler_eraseData(NAMESPACE_GENARAL, KEY_USER_ID);

    printf("\n\nReset User Setting Success\n\n");
    if (isHardReset) {
        return;
    }
    
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ESP_resetChip();
}

void FlashHandler_getDataOtaInStore()
{
    if (FlashHandler_getData(NAME_VERSION_FW_OLD, KEY_VERSION_FW_OLD, &versionFwOld_t)) {
        // log_info("Get Old version firmware success");
        printf("Old Version ESP: %d\n", versionFwOld_t.versionEspOld);
    } else {
        printf("Get old version firmware: no data\n");
    }

    if (FlashHandler_getData(NAME_VERSION_FW_ESP, KEY_VERSION_FW_OLD, &checkStateOtaEsp)) {
        // log_info("Get state version firmware esp success");
        printf("State OTA ESP: %d\n", checkStateOtaEsp);
    } else {
        printf("Get state version firmware esp: no data\n");
    }

    if (FlashHandler_getData(NAME_CONFIRM_END_OTA, KEY_VERSION_FW_OLD, &confirmEndOta_t)) {
        // log_info("Get state end ota success");
        printf("State End OTA: %d\n", confirmEndOta_t.state);
        printf("Data: \"%s\"\n", confirmEndOta_t.dataSend);
    } else {
        printf("Get state end ota: no data\n");
    }
}

bool FlashHandler_getData(char* nameSpace,char* key,void* dataStore)
{
    // log_info("Getting data store in flash");
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(nameSpace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        log_error("open nvs error");
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        log_error("nvs_get_blob error");
        return false;
    }

    // Read previously saved blob if available
    if (required_size > 0) {
        // log_info("Found data store in flash");
        err = nvs_get_blob(my_handle, key, dataStore, &required_size);
        if (err != ESP_OK) {
            log_error("nvs_get_blob error");
            return false;
        }
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        log_error("nvs_commit error");
        return false;
    }
    // Close
    nvs_close(my_handle);
    // log_info("Get data store in flash done");
    if (required_size > 0) {
        return true;
    }
    
    return false;
}

bool FlashHandler_setData(char* nameSpace,char* key,void* dataStore,size_t dataSize)
{
    // log_info("Saving data to flash");
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(nameSpace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        log_error("open nvs error");
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    // Write value including previously saved blob if available
    required_size = dataSize;
    // log_info("store data size: %d", required_size);
    err = nvs_set_blob(my_handle, key, dataStore, required_size);

    if (err != ESP_OK) {
        log_error("nvs_set_blob error %d", err);
        return false;
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        log_error("nvs_commit error");
        return false;
    }
    // Close
    nvs_close(my_handle);
    // log_info("Data is saved\n");
    return true;
}

bool FlashHandler_eraseData(char* nameSpace, const char *key)
{
    nvs_handle_t my_handle;
    esp_err_t err = ESP_OK;

    err = nvs_open(nameSpace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        log_error("open nvs error");
        return false;
    }

    if (key) {
        err = nvs_erase_key(my_handle, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            log_info("Erase, namespace \"%s\", key \"%s\" not exists", nameSpace, key);
            return false;
        }
    }
    if (err != ESP_OK) {
        log_error("Erase, nvs_erase_%s_%s failed, err %s", nameSpace, key, esp_err_to_name(err));
        return false;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        log_error("Erase, nvs_commit failed, err %s", esp_err_to_name(err));
        return false;
    }
    if (key) {
        log_info("Erase done, namespace \"%s\", key \"%s\"", nameSpace, key);
    } 
    return true;
}

bool FlashHandler_getDeviceInfoInStore()
{
    info_id_password_t deviceInfo;
    if (FlashHandler_getData(NAMESPACE_GENARAL, KEY_DEVICE_INFO_ID_PWS, &deviceInfo)) {
        strcpy(g_password, deviceInfo.password);
        strcpy(g_product_Id, deviceInfo.productId);
        return true;
    } else {
        return false;
    }
}

bool FlashHandler_saveDeviceInfoInStore()
{
    info_id_password_t deviceInfo;
    strcpy(deviceInfo.productId, g_product_Id);
    strcpy(deviceInfo.password, g_password);
    if (FlashHandler_setData(NAMESPACE_GENARAL, KEY_DEVICE_INFO_ID_PWS, &deviceInfo, sizeof(deviceInfo))) {
        // log_info("Save data in store done id: %s, pw: %s", deviceInfo.productId, deviceInfo.password);
        return true;
    } else {
        log_error("Save device info false");
        return false;
    }
}

bool FlashHandler_getUserData()
{
    if (FlashHandler_getData(NAMESPACE_GENARAL, KEY_USER_ID, &g_user_ID)) {
        // log_info("Got user data in store done");
        return true;
    } else {
        log_error("Got user data have not set yet");
        return false;
    }
}

bool FlashHandler_saveUserData()
{
    if (FlashHandler_setData(NAMESPACE_GENARAL, KEY_USER_ID, &g_user_ID, sizeof(g_user_ID))) {
        // log_info("Save user data in store done");
        return true;
    } else {
        log_error("Save user data false");
        return false;
    }  
}

bool FlashHandler_getCertKeyMqttInStore()
{
    if (FlashHandler_getData(NAMESPACE_GENARAL, KEY_CERTKEY_MQTT, &certKeyMqtt)) {
        // log_info("Got cert : %s", (char*)(certKeyMqtt.cert));
        // log_info("Got key : %s", (char*)(certKeyMqtt.key));
        return true;
    } else {
        log_error("Get cert key fail");
        return false;
    }
}

bool FlashHandler_saveCertKeyMqttInStore()
{
    if (FlashHandler_setData(NAMESPACE_GENARAL, KEY_CERTKEY_MQTT, &certKeyMqtt, sizeof(certKeyMqtt))) {
        // log_info("Save cert key mqtt ok");
        return true;
    } else {
        log_error("Save cert key false");
        return false;
    }
}

bool FlashHandler_getEnvironmentMqttInStore()
{
    if (FlashHandler_getData(NAMESPACE_GENARAL, KEY_ENVIR_MQTT, &envir)) {
        printf("Got environment: %s\n", (char*)(envir.environment));
        return true;
    } else {
        log_error("Get environment fail");
        return false;
    }
}

bool FlashHandler_saveEnvironmentMqttInStore()
{
    if (FlashHandler_setData(NAMESPACE_GENARAL, KEY_ENVIR_MQTT, &envir, sizeof(mqtt_environment_t))) {
        // log_info("Save environment mqtt ok");
        return true;
    } else {
        log_error("Save environment false");
        return false;
    }
}

bool FlashHandler_getMyJobsInStore()
{
    if (FlashHandler_getData(NAMESPACE_GENARAL, KEY_SCHEDULE, &mySchedule)) {
        // printf("Got my jobs ok\n");
        return true;
    } else {
        printf("Get my jobs fail\n");
        return false;
    }
}

bool FlashHandler_saveMyJobsInStore()
{
    if (FlashHandler_setData(NAMESPACE_GENARAL, KEY_SCHEDULE, &mySchedule, sizeof(mySchedule))) {
        log_info("Save my jobs ok");
        return true;
    } else {
        log_error("Save my jobs false");
        return false;
    }
}

/***********************************************/

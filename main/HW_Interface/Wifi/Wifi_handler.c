/**
 ******************************************************************************
 * @file    Wifi_handler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "Wifi_Handler.h"
#include "HTG_Utility.h"
#include "gateway_config.h"
#include "MqttHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Wifi_Handler"

#ifndef DISABLE_LOG_ALL
#define WIFI_HANDLER_LOG_INFO_ON
#endif

#ifdef WIFI_HANDLER_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#define log_warning(format, ...)
#endif

#define SSID_SEPARATE   0x06

/*******************************************************************************
* Extern Variables
*******************************************************************************/
extern bool isWifiCofg;
extern char g_product_Id[PRODUCT_ID_LEN];
extern char g_macDevice[6];
extern infoFactoryDefault_t infoFactoryDefault;

/*******************************************************************************
* Variables
*******************************************************************************/
bool g_haveWifiList = false;
bool disableReconnect = false;
bool time_waitConnect = false;
char IP_Device[20];
uint8_t *wifi_list_char_str;
uint16_t Wifi_ListSsidLen = 0;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
Wifi_State wifiState = Wifi_State_None;

/*******************************************************************************
 * Wifi Event
 ******************************************************************************/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        esp_wifi_connect();
    } else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_SCAN_DONE)) {
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (apCount == 0) {
            Wifi_startScan();
            return;
        }
        wifi_ap_record_t *list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));

        int i;
        Wifi_ListSsidLen = 0;
        wifi_list_char_str = (uint8_t*)malloc(WIFI_LIST_MAX_LEN);
        if (wifi_list_char_str == NULL) {
            log_error("Wifi list string malloc fail");
        }
        log_warning("========================================================================================\n");
        log_warning("             SSID             |    RSSI    |           AUTH           |       MAC       \n");
        log_warning("========================================================================================\n");
        for (i = 0; i < apCount; i++) {
            char *authmode;
            switch (list[i].authmode) {
            case WIFI_AUTH_OPEN:
                authmode = "WIFI_AUTH_OPEN";
                break;
            case WIFI_AUTH_WEP:
                authmode = "WIFI_AUTH_WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                authmode = "WIFI_AUTH_WPA_PSK";
                break;
            case WIFI_AUTH_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA2_PSK";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA_WPA2_PSK";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                authmode = "WIFI_AUTH_WPA2_WPA3_PSK";
                break;
            default:
                authmode = "Unknown";
                break;
            }
            log_warning("%26.26s    |    % 4d    |    %22.22s|%x:%x:%x:%x:%x:%x\n", list[i].ssid, 
                                                                                    list[i].rssi, 
                                                                                    authmode,
                                                                                    list[i].bssid[0],
                                                                                    list[i].bssid[1],
                                                                                    list[i].bssid[2],
                                                                                    list[i].bssid[3],
                                                                                    list[i].bssid[4],
                                                                                    list[i].bssid[5]);
           
            if (wifi_list_char_str == NULL) {
                continue;
            }
            if ((Wifi_ListSsidLen + strlen((char*)list[i].ssid)) >= WIFI_LIST_MAX_LEN) {
                break;
            }
            buffer_add(wifi_list_char_str, &Wifi_ListSsidLen, list[i].ssid, strlen((char*)list[i].ssid));
            if (i != (apCount - 1)) {
                uint8_t tmp = SSID_SEPARATE;
                buffer_add(wifi_list_char_str, &Wifi_ListSsidLen, &tmp, 1);
            }
                
        }
        if (wifi_list_char_str != NULL) {
            log_warning("Wifi list string with len: %d", Wifi_ListSsidLen);
            g_haveWifiList = true;
        }
        free(list);
    }

    if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
        log_warning("IP_EVENT_STA_GOT_IP...");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_info( "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(IP_Device, "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        wifiState = Wifi_State_Got_IP;
        time_waitConnect = false;
        GatewayConfig_wifiConnectDone();
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        MQTT_Start();
    }

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_CONNECTED)) {
        log_warning("WIFI_EVENT_STA_CONNECTED...");
        time_waitConnect = true;
    }

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        log_error("WIFI_EVENT_STA_DISCONNECTED...");
        wifiState = Wifi_State_Started;
        time_waitConnect = false;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
}

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
void Wifi_disableReconnect()
{
    disableReconnect = true;
}

void Wifi_retryToConnect()
{
    printf(" retry_wifi...\n");
    wifi_config_t getcfg;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &getcfg);
    log_warning("SSID: %s, PW: %s", getcfg.sta.ssid, getcfg.sta.password);
    if (getcfg.sta.ssid[0] == '\0') {
        return;
    }
    Wifi_startConnect((char *)getcfg.sta.ssid, (char *)getcfg.sta.password);
}

void Wifi_getMacStr()
{
    uint8_t one_mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, one_mac);
    memcpy(g_macDevice, one_mac, sizeof(one_mac));
    printf("MAC:: %02X:%02X:%02X:%02X:%02X:%02X\n", one_mac[0], one_mac[1], one_mac[2], one_mac[3], one_mac[4], one_mac[5]);
}

void setProductId_defaultMac()
{
    sprintf(g_product_Id, "S%02X%02X%02X%02X", g_macDevice[2], g_macDevice[3], g_macDevice[4], g_macDevice[5]);
    printf("ProductId_Default: \"%s\"\n", g_product_Id);
}

void Wifi_Initialize()
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));  
    Wifi_getMacStr();
    setProductId_defaultMac();
}

void Wifi_start() 
{
    if (wifiState == Wifi_State_None) {
        wifiState = Wifi_State_Started;
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        log_warning("Start Wifi Success");
    }
}

void Wifi_stop() 
{
    if (wifiState != Wifi_State_None) {
        wifiState = Wifi_State_None;
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
        ESP_ERROR_CHECK(esp_wifi_stop());
        log_warning("Stop Wifi Success");
    }
}

void Wifi_startScan() 
{
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    for (uint8_t i = 0; i < 5; i++) {
        if (ESP_OK == esp_wifi_scan_start(&scanConf, false)) {
            break;
        } else {
            log_error("can not scan wifi");   
        }
    }
}

void Wifi_startConnect(char* ssid, char* password) 
{
    if (wifiState == Wifi_State_Got_IP) {
        esp_wifi_disconnect();
    }

    disableReconnect = false;
    wifi_config_t wifi_config = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    if (isWifiCofg || infoFactoryDefault.checkFactoryDefault) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void Wifi_reConnect() 
{
    esp_wifi_disconnect();
    vTaskDelay(100/portTICK_PERIOD_MS);
    esp_wifi_connect();
}

void Wifi_startConfigMode() 
{
    vTaskDelay(1000/portTICK_PERIOD_MS);
    if (wifiState == Wifi_State_Got_IP) {
        Wifi_startScan();
        return;
    } else {
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        Wifi_disableReconnect();
        Wifi_start();
        esp_wifi_disconnect();
        wifiState = Wifi_State_Started;
        Wifi_startScan();
    }
}

int Wifi_checkRssi()
{
    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata) == 0) {
        printf(" Rssi: %d\r\n", wifidata.rssi);
        return wifidata.rssi;
    }
    return 0;
}

void Wifi_updateInfoWifi()
{
    int rssi = Wifi_checkRssi();
    char data[150] = "";
    wifi_config_t getCfgWifi;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &getCfgWifi);
	sprintf(data,"{\"SSID\":\"%s\",\"MAC\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"IP\":\"%s\",\"RSSI\":\"%d\"}", getCfgWifi.sta.ssid, 
                                                                                                                g_macDevice[0], 
                                                                                                                g_macDevice[1], 
                                                                                                                g_macDevice[2], 
                                                                                                                g_macDevice[3], 
                                                                                                                g_macDevice[4], 
                                                                                                                g_macDevice[5], 
                                                                                                                IP_Device, 
                                                                                                                rssi);
    MQTT_PublishInfoWifi(data);
}

void Wifi_setStateDefault()
{
    wifi_config_t wifi_config_default;
    strcpy((char *)wifi_config_default.sta.ssid, "none");
    strcpy((char *)wifi_config_default.sta.password, "none");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_default));
    log_warning("Config Wifi Default Success");
}

Wifi_State getWifiState() 
{
    return wifiState;
}

/***********************************************/

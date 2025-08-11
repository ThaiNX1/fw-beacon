/**
 ******************************************************************************
 * @file    Beacon_Device_main.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "Global.h"
#include "FlashHandler.h"
#include "timeCheck.h"
#include "OutputControl.h"
#include "Wifi_Handler.h"
#include "HTG_Utility.h"
#include "gateway_config.h"
#include "WatchDog.h"
#include "BLE_handler.h"
#include "myCronJob.h"
#include "DateTime.h"
#include "MqttHandler.h"
#include "ProtocolHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Main App"

#ifndef DISABLE_LOG_ALL
#define MAIN_LOG_INFO_ON
#endif

#ifdef MAIN_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#define log_warning(format, ...)
#endif

#define INFO_RAM_DEFAULT        (MALLOC_CAP_DEFAULT)
#define INFO_RAM_INTERNAL       (MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
#define INFO_RAM_EXTERNAL       (MALLOC_CAP_SPIRAM) 

#define TIME_DELAY_TASK         200
#define TIME_ADV_SPECIAL        5000

/*******************************************************************************
 * Extern Variables
 ******************************************************************************/
extern bool begin_active_device, end_active_device;
extern bool isWifiCofg;
extern bool isHardReset;
extern bool g_haveWifiList;
extern bool time_waitConnect;
extern bool g_isMqttConnected;
extern bool g_newInfoWifiPass;
extern bool g_haveNewSsid;
extern bool checkStateOtaEsp;
extern bool state_bleWifi;
extern bool check_userId;
extern bool g_mqttHaveNewCertificate;
extern char g_new_ssid[32], g_new_pwd[32];
extern struct tm timeLocal;
extern infoFactoryDefault_t infoFactoryDefault;
extern confirmEndOta confirmEndOta_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool needSendInfoMqtt = true;
bool needUpdateDataActived = true;
bool needCheckVersionEspOTA = true;
bool needCheckStateEndOta = true;
bool getInfoMobileToEsp = false;
bool s_beaconIsActive = false;
char g_product_Id[PRODUCT_ID_LEN];
char g_password[PASSWORD_LEN] = DEVICE_PWD;
char g_macDevice[6];
uint16_t major_set = 0;
uint16_t minor_set = 0;
uint32_t g_user_ID = 0;
time_t time_now;

/*******************************************************************************
 * Print Data Device
 ******************************************************************************/
void printResetReason()
{
    log_error("Reset reason is: [%d]\n", rtc_get_reset_reason(0));
    /**************************************************************************/
    // NO_MEAN                =  0,
    // POWERON_RESET          =  1,    /**<1, Vbat power on reset*/
    // RTC_SW_SYS_RESET       =  3,    /**<3, Software reset digital core*/
    // DEEPSLEEP_RESET        =  5,    /**<5, Deep Sleep reset digital core*/
    // TG0WDT_SYS_RESET       =  7,    /**<7, Timer Group0 Watch dog reset digital core*/
    // TG1WDT_SYS_RESET       =  8,    /**<8, Timer Group1 Watch dog reset digital core*/
    // RTCWDT_SYS_RESET       =  9,    /**<9, RTC Watch dog Reset digital core*/
    // INTRUSION_RESET        = 10,    /**<10, Instrusion tested to reset CPU*/
    // TG0WDT_CPU_RESET       = 11,    /**<11, Time Group0 reset CPU*/
    // RTC_SW_CPU_RESET       = 12,    /**<12, Software reset CPU*/
    // RTCWDT_CPU_RESET       = 13,    /**<13, RTC Watch dog Reset CPU*/
    // RTCWDT_BROWN_OUT_RESET = 15,    /**<15, Reset when the vdd voltage is not stable*/
    // RTCWDT_RTC_RESET       = 16,    /**<16, RTC Watch dog reset digital core and rtc module*/
    // TG1WDT_CPU_RESET       = 17,    /**<17, Time Group1 reset CPU*/
    // SUPER_WDT_RESET        = 18,    /**<18, super watchdog reset digital core and rtc module*/
    // GLITCH_RTC_RESET       = 19,    /**<19, glitch reset digital core and rtc module*/
    // EFUSE_RESET            = 20,    /**<20, efuse reset digital core*/
    // USB_UART_CHIP_RESET    = 21,    /**<21, usb uart reset digital core */
    // USB_JTAG_CHIP_RESET    = 22,    /**<22, usb jtag reset digital core */
    // POWER_GLITCH_RESET     = 23,    /**<23, power glitch reset digital core and rtc module*/
    /**************************************************************************/
}

void printCommonMode()
{
    // không xử lý
}

void printCommonInfo()
{
    log_warning("Info_Device:: ");
    printf(" Device_Name: \"%s\", Model_Number: [%d], Model_Name: \"%s\"\n", DEVICE_NAME, MODEL_VER_NUMBER, MODEL_NAME);
    printf(" Serial_Number: \"%s\", Password: \"********\", User_ID: [%lu]\n", g_product_Id, g_user_ID);
    printf(" Hardware_Ver: [%d], Version_String: \"%s\", Common_Ver: [%d], Firmware_Ver: [%d]\n", HARD_VER, VER_STRING, COMMON_VER, FIRM_VER);

    wifi_config_t getDataCfgWifi;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &getDataCfgWifi);
    log_warning("Info Wifi:: ");
    printf(" Wifi_Name: \"%s\", Wifi_Password: \"%s\"\n", getDataCfgWifi.sta.ssid, getDataCfgWifi.sta.password);

    nvs_stats_t getNvsStatus;
    nvs_get_stats(NULL, &getNvsStatus);
    log_warning("Info Partition \"nvs\":: ");
    printf(" Used_Entries = (%d), Free_Entries = (%d), All_Entries = (%d), Amount_Name_Space = (%d)\n", getNvsStatus.used_entries, getNvsStatus.free_entries, getNvsStatus.total_entries, getNvsStatus.namespace_count);
}

void printDataDevice()
{
    printResetReason();
    printCommonMode();
    printCommonInfo();
}

/*******************************************************************************
 * Application Functions
 ******************************************************************************/
void app_processLedIndication()
{
    // Led Indication
    if (isHardReset) {
        Out_ledHardReset();
    } else if (isWifiCofg || infoFactoryDefault.checkFactoryDefault) {
        Out_ledWaitConnect();
    } else if (s_beaconIsActive) {
        Out_setColor(LED_COLOR_BLUE);
    } else {
        Out_setColor(LED_COLOR_RED);
    }
}

/*******************************************************************************
 * App Main Task Common
 ******************************************************************************/
static void app_mainTaskCommon(void *arg)
{
    bool retryConnectWhenHaveNewWifi = false;
    uint32_t timeOutConfigWifi = 0;
    uint32_t timeOutGetIpWifi = 0;
    uint32_t timeWifiDisconnected = 0;
    uint32_t timeCountRetryConnect = 0;
    uint32_t timeCountWaitConnect = 0;
    uint32_t timeCountHardReset = 0;
    
    while (1)
    {
        // khi hard reset, nếu time out sẽ reset lại mạch
        if (isHardReset) {
            if (timeCountHardReset == 0) {
                timeCountHardReset = xTaskGetTickCount();
            }
            if ((xTaskGetTickCount() - timeCountHardReset) > TIME_OUT_HARD_RESET) {
                log_warning("Timeout Hard Reset, Restart Switch...\n");
                ESP_resetChip();
            }
        }

        // khi isHardReset = true thì không chạy config wifi
        if ((isWifiCofg || infoFactoryDefault.checkFactoryDefault) && !isHardReset) {
            // sau khi nhận được new wifi cho phép retry connect liên tục để kết nối tới server cho tới khi time out
            if (retryConnectWhenHaveNewWifi && check_userId) { 
                // check User ID, nếu false thì không cần retry connect
                if (timeCountRetryConnect == 0) {
                    timeCountRetryConnect = xTaskGetTickCount();
                }
                if ((xTaskGetTickCount() - timeCountRetryConnect) > WIFI_RECONNECT_NEW_WIFI) {
                    // khi có kết nối thì không cần retry connect
                    if (!state_bleWifi) Wifi_retryToConnect();
                    timeCountRetryConnect = xTaskGetTickCount();
                }
            }
            // khi time out = 0, cho phép chạy scan wifi
            if (timeOutConfigWifi == 0) {
                Wifi_startConfigMode();
                g_newInfoWifiPass = false;
                timeOutConfigWifi = xTaskGetTickCount();
            }
            // khi có list wifi, phát BLE truy cập cả 2 profiles A, B
            if (g_haveWifiList) {
                BLE_startConfigMode();
                g_haveWifiList = false;
            }
            // khi có new wifi call back wifi start connect
            if (g_haveNewSsid) {
                g_haveNewSsid = false;
                GatewayConfig_haveWifiFromBLE(g_new_ssid, g_new_pwd);
                retryConnectWhenHaveNewWifi = true;
            }
            // kiểm tra xem mobile có gửi thông tin ssid, password hay không
            if (getInfoMobileToEsp) {
                if (timeOutGetIpWifi == 0) {
                    timeOutGetIpWifi = xTaskGetTickCount();
                }
                if ((xTaskGetTickCount() - timeOutGetIpWifi) > TIME_OUT_GET_DATA_WIFI) {
                    if (infoFactoryDefault.checkFactoryDefault) {
                        Wifi_setStateDefault();
                        Flash_deleteInfoFactoryDefault();
                    } else {
                        // isWifiCofg = true;
                        // Flash_saveConfigWifi();
                        // log_warning("Can't Connect To Router, Set Mode Return Config Wifi\n");
                    }
                    Out_ledFailConnect();
                    log_warning("Timeout Get IP From Router, Restart...\n");
                    ESP_resetChip();
                }
            } else if (!infoFactoryDefault.checkFactoryDefault) {
                if ((xTaskGetTickCount() - timeOutConfigWifi) > TIME_OUT_CONFIG_WIFI) {
                    isWifiCofg = false;
                    log_warning("Timeout Config Wifi, Restart...\n");
                    ESP_resetChip();
                }
            }
        }

        // thiết bị reconnect vào router khi router bị treo or full IP
        if (time_waitConnect) {
            if (timeCountWaitConnect == 0) {
                timeCountWaitConnect = xTaskGetTickCount();
            }
            if ((xTaskGetTickCount() - timeCountWaitConnect) > WIFI_RECONNECT_INTERVAL) {
                log_warning("Timeout Wifi Wait Connect...");
                Wifi_reConnect();
                log_warning("Reconnect...");
                timeCountWaitConnect = 0;
            }
        }

        // kiểm tra trạng thái wifi
        if (getWifiState() != Wifi_State_Got_IP) {
            // thời điểm mất kết nối
            if (timeWifiDisconnected == 0) {
                timeWifiDisconnected = xTaskGetTickCount();
            } else if (!isWifiCofg && !infoFactoryDefault.checkFactoryDefault) {
                // sau 1 khoảng thời gian, reconect wifi
                if ((xTaskGetTickCount() - timeWifiDisconnected) > WIFI_RECONNECT_INTERVAL) {
                    esp_wifi_connect();
                    timeWifiDisconnected = xTaskGetTickCount();
                }
            }
        } else {
            timeWifiDisconnected = 0;
        }

        // khi ở mode factory, sau khi thêm thiết bị thành công, set trạng thái mặc định
        if (infoFactoryDefault.checkFactoryDefault && !infoFactoryDefault.checkStateWifiDefault) {
            log_warning("Set Mode Device Default");
            Flash_setModeDefault();
        }

        // gửi thông tin lên server bằng mqtt
        if (needSendInfoMqtt && g_isMqttConnected) {
            MQTT_PublishVersion(MODEL_VER_NUMBER, HARD_VER, COMMON_VER, FIRM_VER);
            Wifi_updateInfoWifi();
            MQTT_PublishInfoBeacon(major_set);
            needSendInfoMqtt = false;
        }
        // sau khi kích hoạt, khi công tắc online sẽ gửi thông tin đã active lên server
        if (needUpdateDataActived && begin_active_device && !end_active_device && g_isMqttConnected) {
            updateInfoActiveDevice();
            needUpdateDataActived = false;
        }
        // sau khi OTA cho esp kết thúc,chạy hàm kiểm tra xem update thành công hay thất bại
        if (needCheckVersionEspOTA && checkStateOtaEsp && g_isMqttConnected) {
            checkUpdateVerFwEsp();
            needCheckVersionEspOTA = false;
        }
        // khi OTA xong, sẽ gửi trạng thái kết thúc cho server, nếu nhận đc phản hồi sẽ kết thúc bản tin
        if (needCheckStateEndOta && confirmEndOta_t.state && g_isMqttConnected) {
            checkEndOtaFromServer();
            needCheckStateEndOta = false;
        }

        vTaskDelay(TIME_DELAY_TASK/portTICK_PERIOD_MS);
    }
}

/*******************************************************************************
 * App Sub Task Common
 ******************************************************************************/
static void app_subTaskCommon(void *arg)
{
    bool cronjobIsStarted = false;
    uint8_t step_change_cid_ibeacon = 0;
    uint16_t countTime_1 = 0;
    uint32_t timeChangeAdvSpecial = 0;

    md5_checksum_serial_number();
    major_set = ht_check_crc16_mac_device();
    log_warning("Major Set: %d", major_set);

    while (1)
    {
        Out_button_process();

        // khi có mạng sẽ cập nhật thời gian online, start cronjob
        if (countTime_1++ >= 5) {
            countTime_1 = 0;
            time(&time_now);
            localtime_r(&time_now, &timeLocal);
            if (checkRealTimeLocal()) {
                if (cronjobIsStarted == false) {
                    // myScheduleStart();
                    cronjobIsStarted = true;
                }
            }
        }

        // phát bản tin quảng bá chấm công iBeacon
        if ((checkRealTimeLocal()) && (g_mqttHaveNewCertificate) && (!isHardReset && !infoFactoryDefault.checkFactoryDefault && !isWifiCofg)) {
            if ((timeChangeAdvSpecial == 0) && (step_change_cid_ibeacon == 0)) {
                timeChangeAdvSpecial = xTaskGetTickCount();
                minor_set = (uint16_t)ht_gen_topt();
                BLE_iBeaconSetSpecial(APPLE_ID, major_set, minor_set);
                step_change_cid_ibeacon = 1;
            }
            if (((xTaskGetTickCount() - timeChangeAdvSpecial) > TIME_ADV_SPECIAL) && (step_change_cid_ibeacon == 1)) {
                minor_set = (uint16_t)ht_gen_topt();
                BLE_iBeaconSetSpecial(NORDIC_ID, major_set, minor_set);
                step_change_cid_ibeacon = 2;
            }
            if (((xTaskGetTickCount() - timeChangeAdvSpecial) > (TIME_ADV_SPECIAL*2)) && (step_change_cid_ibeacon == 2)) {
                timeChangeAdvSpecial = 0;
                step_change_cid_ibeacon = 0;
            }
            s_beaconIsActive = true;
        } else {
            timeChangeAdvSpecial = 0;
            step_change_cid_ibeacon = 0;
            s_beaconIsActive = false;
        }

        app_processLedIndication();
        vTaskDelay((TIME_DELAY_TASK/10)/portTICK_PERIOD_MS);
    }
}

/*******************************************************************************
 * Application Start
 ******************************************************************************/
void App_Initialize()
{
    if ((!begin_active_device) || (!end_active_device)) {
        timerActiveDeviceStart();
    }

    BLE_startControlMode();
    Wifi_start();
    printDataDevice();

    xTaskCreate(app_mainTaskCommon, "app_mainTaskCommon", 5*1024, NULL, 5, NULL);
    xTaskCreate(app_subTaskCommon, "app_subTaskCommon", 3*1024, NULL, 5, NULL);
}

/*******************************************************************************
 * Main App
 ******************************************************************************/
void app_main()
{
    Flash_Initialize();
    GPIO_Initialize();
    Wifi_Initialize();
    My_CronJob_Initialize();
    MQTT_Initialize();
    Date_Time_Initialize();
    App_Initialize();

    while (1)
    {
        // Print data RAM
        log_warning("Timestamp: %lld, Free heap default: %d bytes", time_now, heap_caps_get_free_size(INFO_RAM_DEFAULT));

        printf("Free heap internal: [total - %d] [free - %d] [min free - %d]\n", heap_caps_get_total_size(INFO_RAM_INTERNAL), 
                                                                                heap_caps_get_free_size(INFO_RAM_INTERNAL), 
                                                                                heap_caps_get_minimum_free_size(INFO_RAM_INTERNAL));

        printf("Free heap external: [total - %d] [free - %d] [min free - %d]\n", heap_caps_get_total_size(INFO_RAM_EXTERNAL), 
                                                                                heap_caps_get_free_size(INFO_RAM_EXTERNAL), 
                                                                                heap_caps_get_minimum_free_size(INFO_RAM_EXTERNAL));

        // Print time local
        log_info("Time local: \"day of week: %d\" \"%s%02d-%02d-%02d %02d:%02d:%02d\"", timeLocal.tm_wday+1, 
                                                                                        timeLocal.tm_year >= 100 ? "20" : "19", 
                                                                                        timeLocal.tm_year >= 100 ?  timeLocal.tm_year - 100 : timeLocal.tm_year, 
                                                                                        timeLocal.tm_mon+1, timeLocal.tm_mday,
                                                                                        timeLocal.tm_hour, timeLocal.tm_min, timeLocal.tm_sec);

        vTaskDelay(30000/portTICK_PERIOD_MS);
        printf("\r\n");
    }

    return;
}

/*******************************************************************************
 * End Code
 ******************************************************************************/
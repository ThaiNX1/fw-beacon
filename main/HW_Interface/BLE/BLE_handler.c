/**
 ******************************************************************************
 * @file    BLE_handler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
 #include "BLE_handler.h"
 #include "HTG_Utility.h"
 #include "Wifi_handler.h"
 #include "gateway_config.h"
 #include "FlashHandler.h"
 #include "gateway_config.h"
 #include "timeCheck.h"
 
 /*******************************************************************************
  * Definitions
  ******************************************************************************/
 #define TAG "Ble_Handler"
 
 #ifndef DISABLE_LOG_ALL
 #define BLE_LOG_INFO_ON
 #endif
 
 #ifdef BLE_LOG_INFO_ON
 #define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
 #define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
 #define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
 #else
 #define log_info(format, ...)
 #define log_error(format, ...)
 #define log_warning(format, ...)
 #endif
 
 #define MANUFACTURER_DATA_LEN               17
 
 #define PROFILE_NUM                         2
 #define PROFILE_CONFIG_WIFI_APP_ID          0
 #define PROFILE_BLE_CONTROL_APP_ID          1
 #define MAX_INSTANCE_CHAR                   3
 
 /*------------------------- Service for config wifi -------------------------*/
 //service
 #define GATTS_SERVICE_UUID_CONFIG_WIFI      0x12BD
 //character
 #define GATTS_CHAR_UUID_WIFI_LIST           0xBD01
 #define GATTS_CHAR_UUID_COM                 0xBD02
 
 #define GATTS_NUM_HANDLE_CONFIG_WIFI        8
 //define for other property attr
 #define WIFI_LIST_CHAR_VAL_LEN_MAX          500
 #define CONFIG_WIFI_COM_CHAR_VAL_LEN_MAX    100
 static esp_bt_uuid_t cccd_uuid = {
     .len = ESP_UUID_LEN_16,
     .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}
 };
 
 /*---------------------------- Service for control ---------------------------*/
 //service
 #define GATTS_SERVICE_UUID_BLE_CONTROL      0x12CD
 //character
 #define GATTS_CHAR_UUID_CONTROL_LIST        0xCD01
 #define GATTS_CHAR_UUID_BLE_CONTROL_COM     0xCD02
 
 #define GATTS_NUM_HANDLE_BLE_CONTROL        8
 //define for other property attr
 #define BLE_CONTROL_CHAR_VAL_LEN_MAX        500
 #define BLE_CONTROL_COM_CHAR_VAL_LEN_MAX    100
 
 /*----------------------------------------------------------------------------*/
 #define PRE_CMD_USE_WIFI                    "_UWF:"
 #define PRE_CMD_HARD_RESET                  "_RST:"
 #define PRE_CMD_USER_ID                     "_UID:"
 #define PRE_CMD_END                         "_END:"
 
 #define END_OF_BLE_CONTROL_CMD              0x04
 #define BEGIN_OF_BLE_CONTROL_CMD            0X01
 
 #define END_OF_CMD                          0x04
 #define BEGIN_OF_CMD                        '_'
 #define LEN_OF_PRE_CMD                      5
 #define PARAM_SEPARATE                      0x06
 #define START_PARAM_CHAR                    ':'
 
 #define MAX_PARAM_NUM                       10
 #define NUM_OF_PRO                          2
 
 /*******************************************************************************
  * Extern Variables
  ******************************************************************************/
 extern bool isWifiCofg;
 extern bool getInfoMobileToEsp;
 extern bool isHardReset;
 extern char g_product_Id[PRODUCT_ID_LEN];
 extern uint8_t *wifi_list_char_str;
 extern uint16_t Wifi_ListSsidLen;
 extern infoFactoryDefault_t infoFactoryDefault;
 
 /*******************************************************************************
  * Typedef Variables
  ******************************************************************************/
 typedef struct
 {
     uint16_t char_handle;
     esp_bt_uuid_t char_uuid;
 } gatts_char_inst;
 
 struct gatts_profile_inst
 {
     esp_gatts_cb_t gatts_cb;
     uint16_t gatts_if;
     uint16_t app_id;
     uint16_t conn_id;
     uint16_t service_handle;
     esp_gatt_srvc_id_t service_id;
     gatts_char_inst charInsts[MAX_INSTANCE_CHAR];
     uint8_t numberOfChar;
     esp_gatt_perm_t perm;
     esp_gatt_char_prop_t property;
     uint16_t descr_handle;
     esp_bt_uuid_t descr_uuid;
     uint16_t cccd_handles[MAX_INSTANCE_CHAR]; // Store CCCD handles for each characteristic
 };
 
 typedef struct
 {
     uint8_t *prepare_buf;
     int prepare_len;
 } prepare_type_env_t;
 
 /*******************************************************************************
  * Variables
  ******************************************************************************/
 bool registeredConfigService = false;
 bool registeredControlService = false;
 bool s_bleConfigConnected = false;
 bool s_bleControlConnected = false;
 bool g_haveNewSsid = false;
 bool ble_inited = false;
 bool cccd_notifications_enabled = false;  // Lưu trạng thái CCCD (notify enable/disable) cho profile config
 char g_new_ssid[32], g_new_pwd[32];
 
 /*******************************************************************************
  * Prototypes
  ******************************************************************************/
 static void gatts_profile_config_wifi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
 static void gatts_profile_ble_control_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
//  void processCmd(uint8_t *cmdLine, uint16_t cmdLineLen, uint16_t *indexList, uint8_t *indexNum);
 void execute_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
 void writeToConfigWifiComCharEvent(uint8_t len, uint8_t *data);
 
 /*******************************************************************************
  * Declare Properties BLE
  ******************************************************************************/
 /*-------------------------- Service for config wifi -------------------------*/
 esp_attr_value_t wifi_list_char_val = {
     .attr_max_len = WIFI_LIST_CHAR_VAL_LEN_MAX,
     .attr_len = 0, // Initial length is 0 since no data initially
     .attr_value = NULL,
 };
 
 uint8_t com_str[CONFIG_WIFI_COM_CHAR_VAL_LEN_MAX] = "no data";
 esp_attr_value_t com_char_val = {
     .attr_max_len = CONFIG_WIFI_COM_CHAR_VAL_LEN_MAX,
     .attr_len = sizeof(com_str) - 1, // Length of "no data" string (excluding null terminator)
     .attr_value = com_str,
 };
 
 /*-------------------------- Service for control -----------------------------*/
 uint8_t ble_control_char_str[BLE_CONTROL_CHAR_VAL_LEN_MAX] = "no data";
 esp_attr_value_t ble_control_char_val = {
     .attr_max_len = BLE_CONTROL_CHAR_VAL_LEN_MAX,
     .attr_len = sizeof(ble_control_char_str) - 1, // Length of "no data" string (excluding null terminator)
     .attr_value = ble_control_char_str,
 };
 
 /*----------------------------------------------------------------------------*/
 static uint8_t config_wifi_service_uuid128[32] = {
     /* LSB <--------------------------------------------------------------------------------> MSB */
     //first uuid, 16bit, [12],[13] is the value - Service 12BD
     0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xBD, 0x12, 0x00, 0x00,
     //second uuid, 16bit, [12],[13] is the value - Service 12CD
     0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xCD, 0x12, 0x00, 0x00
 };
 
 // static uint8_t ble_control_service_uuid128[32] = { ... };
 // static uint8_t manufacturer_default[MANUFACTURER_DATA_LEN] = {0x00};
 
 static esp_ble_adv_data_t config_wifi_adv_data = {
     .set_scan_rsp = false,
     .include_name = true,
     .include_txpower = true,
     .min_interval = 80,
     .max_interval = 160,
     .appearance = 0x00,
     .manufacturer_len = 0,
     .p_manufacturer_data = NULL,
     .service_data_len = 0,
     .p_service_data = NULL,
     .service_uuid_len = 32,
     .p_service_uuid = config_wifi_service_uuid128,
     .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
 };
 
 // static esp_ble_adv_data_t ble_control_adv_data = { ... };
 
 /* One gatt-based profile one app_id and one gatts_if */
 static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
     [PROFILE_CONFIG_WIFI_APP_ID] = {
         .gatts_cb = gatts_profile_config_wifi_event_handler,
         .gatts_if = ESP_GATT_IF_NONE,
     },
     [PROFILE_BLE_CONTROL_APP_ID] = {
         .gatts_cb = gatts_profile_ble_control_event_handler,
         .gatts_if = ESP_GATT_IF_NONE,
     }
 };
 
 static esp_ble_adv_params_t ble_adv_params = {
     .adv_int_min        = 80,
     .adv_int_max        = 160,
     .adv_type           = ADV_TYPE_IND,
     .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
     .channel_map        = ADV_CHNL_ALL,
     .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
 };
 
 static prepare_type_env_t a_prepare_write_env;
 static prepare_type_env_t b_prepare_write_env;
 static int cccd_add_index = 0; // Biến đếm thứ tự thêm CCCD cho từng characteristic

 // Trạng thái CCCD cho 12BD: [0]=BD01, [1]=BD02
static uint16_t cfg_cccd[2] = {0, 0};
 
 /*******************************************************************************
  * BLE Event
  ******************************************************************************/
 static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
 {
     switch (event) {
     case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
         if (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS) {
             log_info("set adv successfully");
             esp_ble_gap_start_advertising(&ble_adv_params);
         } else {
             log_error("set adv fail");
         }
         break;
     case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
         if (param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS) {
             log_info("set adv raw successfully");
             esp_ble_gap_start_advertising(&ble_adv_params);
         } else {
             log_error("set adv raw fail");
         }
         break;
     case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
         esp_ble_gap_start_advertising(&ble_adv_params);
         break;
     case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
         if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
             log_error("Advertising start failed");
         } else {
             log_info("Advertising start ok");
         }
         break;
     case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
         if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
             log_error("Advertising stop failed");
         } else {
             log_info("Stop adv successfully");
         }
         break;
     case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         log_info("update connetion params status=%d, min_int=%d, max_int=%d, conn_int=%d, latency=%d, timeout=%d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
         break;
     default:
         break;
     }
 }
 
 void execute_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
 {
     if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
         esp_log_buffer_hex(TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
     } else {
         log_info("ESP_GATT_PREP_WRITE_CANCEL");
     }
 
     if (prepare_write_env->prepare_buf) {
         free(prepare_write_env->prepare_buf);
         prepare_write_env->prepare_buf = NULL;
     }
     prepare_write_env->prepare_len = 0;
 }
 
 static void gatts_profile_config_wifi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
 {
     const uint8_t *prf_char_config_wifi;
     uint16_t length_config_wifi = 0;
 
     switch (event) {
     case ESP_GATTS_REG_EVT:
         log_info("config_wifi: ESP_GATTS_REG_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_id.is_primary = true;
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_id.id.inst_id = 0x00;
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_CONFIG_WIFI;
 
         esp_ble_gap_config_adv_data(&config_wifi_adv_data);
         esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_id, GATTS_NUM_HANDLE_CONFIG_WIFI);
         break;
 
     case ESP_GATTS_READ_EVT: {
         // Trả lời đọc CCCD bằng esp_gatt_rsp_t (BY_APP)
         for (int i = 0; i < gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].numberOfChar; i++) {
             if (param->read.handle == gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].cccd_handles[i]) {
                 log_info("config_wifi: Reading CCCD descriptor for char %d", i);
                 esp_gatt_rsp_t rsp = {0};
                 rsp.attr_value.handle = param->read.handle;
                 rsp.attr_value.len = 2;
                 rsp.attr_value.value[0] = cccd_notifications_enabled ? 0x01 : 0x00; // notify bit
                 rsp.attr_value.value[1] = 0x00;
                 esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
                 return;
             }
         }
         log_error("config_wifi: CCCD descriptor not found for handle %d", param->read.handle);
         esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_NOT_FOUND, NULL);
         break;
     }
 
     case ESP_GATTS_WRITE_EVT:
         // Ghi CCCD: cập nhật bit notify
         for (int i = 0; i < gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].numberOfChar; i++) {
             if (param->write.handle == gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].cccd_handles[i]) {
                 log_info("config_wifi: Writing to CCCD descriptor for char %d, len=%d", i, param->write.len);
                 if (param->write.len >= 2) {
                     uint16_t cccd_value = param->write.value[0] | (param->write.value[1] << 8);
                     bool notify_en = (cccd_value & 0x0001) != 0;
                     cccd_notifications_enabled = notify_en; // (đơn giản hoá) trạng thái chung
                     log_info("config_wifi: CCCD value: 0x%04x (notifications %s)",
                              cccd_value, notify_en ? "enabled" : "disabled");
                     if (i == 1 && notify_en) {
                        const char* probe = "BD02: hello from ESP32";
                        // BLE_sendOnBD02((const uint8_t*)probe, (uint16_t)strlen(probe));
                     }
                 }
                 esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                 return;
             }
         }
         log_error("config_wifi: CCCD descriptor not found for handle %d", param->write.handle);
         esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_NOT_FOUND, NULL);
         break;
 
     case ESP_GATTS_EXEC_WRITE_EVT:
         log_info("config_wifi: ESP_GATTS_EXEC_WRITE_EVT");
         // Dùng đúng param->exec_write (không phải param->write)
         esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
         execute_write_event_env(&a_prepare_write_env, param);
         break;
 
     case ESP_GATTS_MTU_EVT:
         log_info("config_wifi: ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
         break;
 
     case ESP_GATTS_CONF_EVT:
         // CONF_EVT là confirm của indicate, không phải subscribe
         log_info("config_wifi: CONF_EVT status %d attr_handle %d (confirm of indicate)", param->conf.status, param->conf.handle);
         if (param->conf.status != ESP_GATT_OK) {
             log_error("config_wifi: CONF_EVT failed, status %d", param->conf.status);
             esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
         }
         break;
 
     case ESP_GATTS_UNREG_EVT:
         break;
 
     case ESP_GATTS_CREATE_EVT: {
         log_info("config_wifi: ESP_GATTS_CREATE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_handle = param->create.service_handle;
 
         // Reset mapping CCCD trước khi add
         cccd_add_index = 0;
         memset(gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].cccd_handles, 0,
                sizeof(gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].cccd_handles));
 
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[0].char_uuid.len = ESP_UUID_LEN_16;
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[0].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_WIFI_LIST;
 
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[1].char_uuid.len = ESP_UUID_LEN_16;
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[1].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_COM;
 
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].numberOfChar = 2;
 
         esp_ble_gatts_start_service(gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_handle);
 
         esp_attr_control_t configAttrControl = { .auto_rsp = ESP_GATT_AUTO_RSP };
         // add wifi list char
         esp_ble_gatts_add_char(gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_handle,
                                &gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[0].char_uuid,
                                ESP_GATT_PERM_READ,
                                ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                &wifi_list_char_val,
                                &configAttrControl);
         // add com char
         esp_ble_gatts_add_char(gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_handle,
                                &gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[1].char_uuid,
                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY, // chỉ notify
                                &com_char_val,
                                &configAttrControl);
         break;
     }
 
     case ESP_GATTS_ADD_INCL_SRVC_EVT:
         break;
 
     case ESP_GATTS_ADD_CHAR_EVT:
         log_info("config_wifi: ESP_GATTS_ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d, char_uuid 0x%04x",
                  param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle, param->add_char.char_uuid.uuid.uuid16);
 
         if (param->add_char.status) {
             log_error("config_wifi: ADD_CHAR failed status %d", param->add_char.status);
             break;
         }
         for (int i = 0; i < gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].numberOfChar; i++) {
             if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[i].char_uuid.uuid.uuid16) {
                 gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[i].char_handle = param->add_char.attr_handle;
                 log_info("config_wifi: Characteristic 0x%04x added handle %d", param->add_char.char_uuid.uuid.uuid16, param->add_char.attr_handle);
 
                 if (param->add_char.attr_handle == 0) {
                     log_error("config_wifi: Invalid characteristic handle for UUID 0x%04x", param->add_char.char_uuid.uuid.uuid16);
                     break;
                 }
 
                 // Add CCCD cho char có notify/indicate
                 if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_WIFI_LIST ||
                     param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_COM) {
                     log_info("config_wifi: Adding CCCD for char 0x%04x", param->add_char.char_uuid.uuid.uuid16);
                     esp_attr_control_t cccdAttrControl = { .auto_rsp = ESP_GATT_RSP_BY_APP }; // BY_APP vì ta tự send_response
                     esp_err_t ret = esp_ble_gatts_add_char_descr(
                         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].service_handle,
                         &cccd_uuid,
                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                         NULL, &cccdAttrControl);
                     if (ret != ESP_OK) {
                         log_error("config_wifi: Add CCCD failed, error: %s", esp_err_to_name(ret));
                     } else {
                         log_info("config_wifi: CCCD addition initiated");
                     }
                 }
             }
         }
 
         esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length_config_wifi, &prf_char_config_wifi);
         break;
 
     case ESP_GATTS_ADD_CHAR_DESCR_EVT:
         log_info("config_wifi: ESP_GATTS_ADD_CHAR_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                  param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
         if (param->add_char_descr.status == ESP_GATT_OK) {
             if (param->add_char_descr.descr_uuid.len == ESP_UUID_LEN_16 &&
                 param->add_char_descr.descr_uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                 // Gán CCCD cho đặc tính theo thứ tự thêm
                 if (cccd_add_index < gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].numberOfChar) {
                     gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].cccd_handles[cccd_add_index] = param->add_char_descr.attr_handle;
                     log_info("config_wifi: CCCD descriptor for char %d added handle %d", cccd_add_index, param->add_char_descr.attr_handle);
                     cccd_add_index++;
                 }
             }
         } else {
             log_error("config_wifi: CCCD descriptor addition failed status %d", param->add_char_descr.status);
         }
         break;
 
     case ESP_GATTS_DELETE_EVT:
         break;
 
     case ESP_GATTS_START_EVT:
         log_info("config_wifi: ESP_GATTS_START_EVT, status %d, service_handle %d",
                  param->start.status, param->start.service_handle);
         if (param->start.status == ESP_GATT_OK) {
             log_info("config_wifi: Service 12BD started");
         } else {
             log_error("config_wifi: Service 12BD start failed status %d", param->start.status);
         }
         break;
 
     case ESP_GATTS_STOP_EVT:
         break;
 
     case ESP_GATTS_CONNECT_EVT: {
         log_warning("config_wifi: ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
                     param->connect.conn_id,
                     param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                     param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
 
         if (registeredControlService && !isWifiCofg) {
             break;
         }
 
         esp_ble_conn_update_params_t conn_params;
         memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
         conn_params.latency = 0;
         conn_params.max_int = 0x50; // 100ms
         conn_params.min_int = 0x30; // 60ms
         conn_params.timeout = 400;  // 4s
 
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].conn_id = param->connect.conn_id;
         esp_ble_gap_update_conn_params(&conn_params);
 
         s_bleConfigConnected = true;
         break;
     }
 
     case ESP_GATTS_DISCONNECT_EVT:
         log_warning("config_wifi: ESP_GATTS_DISCONNECT_EVT");
         s_bleConfigConnected = false;
         cccd_notifications_enabled = false;  // Reset CCCD state
         if (!isWifiCofg) break;
         esp_ble_gap_start_advertising(&ble_adv_params);
         break;
 
     case ESP_GATTS_OPEN_EVT:
     case ESP_GATTS_CANCEL_OPEN_EVT:
     case ESP_GATTS_CLOSE_EVT:
     case ESP_GATTS_LISTEN_EVT:
     case ESP_GATTS_CONGEST_EVT:
     default:
         break;
     }
 }
 
 static void gatts_profile_ble_control_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
 {
     switch (event) {
     case ESP_GATTS_REG_EVT:
         log_info("ble_control: ESP_GATTS_REG_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.is_primary = true;
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.inst_id = 0x00;
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_BLE_CONTROL;
         esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id, GATTS_NUM_HANDLE_BLE_CONTROL);
         break;
 
     case ESP_GATTS_READ_EVT: {
         log_warning("ble_control: ESP_GATTS_READ_EVT, handle %d, offset %d",
                     param->read.handle, param->read.offset);
 
         // CCCD read?
         if (param->read.handle == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle) {
             esp_gatt_rsp_t rsp = {0};
             rsp.attr_value.handle = param->read.handle;
             rsp.attr_value.len = 2;
             rsp.attr_value.value[0] = 0x00; // default: notify off
             rsp.attr_value.value[1] = 0x00;
             esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
             log_info("ble_control: CCCD read response sent");
             break;
         } else if (gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle == 0) {
             log_error("ble_control: CCCD descriptor not found for handle %d", param->read.handle);
             esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_NOT_FOUND, NULL);
             break;
         }
         break;
     }
 
     case ESP_GATTS_WRITE_EVT: {
         log_warning("ble_control: ESP_GATTS_WRITE_EVT, handle %d, value len %d",
                     param->write.handle, param->write.len);
 
         // CCCD write?
         if (param->write.handle == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle) {
             log_info("ble_control: Writing to CCCD descriptor, len=%d", param->write.len);
             if (param->write.len >= 2) {
                 uint16_t cccd_value = param->write.value[0] | (param->write.value[1] << 8);
                 log_info("ble_control: CCCD value: 0x%04x", cccd_value);
             }
             esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
             log_info("ble_control: CCCD write response sent");
             break;
         } else if (gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle == 0) {
             log_error("ble_control: CCCD descriptor not found for handle %d", param->write.handle);
             esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_NOT_FOUND, NULL);
             break;
         }
 
         if (param->write.handle == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[0].char_handle) {
             // xử lý nếu cần
         }
         break;
     }
 
     case ESP_GATTS_EXEC_WRITE_EVT:
         log_info("ble_control: ESP_GATTS_EXEC_WRITE_EVT");
         // Dùng đúng param->exec_write
         esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
         execute_write_event_env(&b_prepare_write_env, param);
         break;
 
     case ESP_GATTS_MTU_EVT:
         log_info("ble_control: ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
         break;
 
     case ESP_GATTS_UNREG_EVT:
         break;
 
     case ESP_GATTS_CREATE_EVT:
         log_info("ble_control: ESP_GATTS_CREATE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle = param->create.service_handle;
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar = 1;
 
         esp_ble_gatts_start_service(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle);
 
         esp_attr_control_t controlAttrControl = { .auto_rsp = ESP_GATT_AUTO_RSP };
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[0].char_uuid.len = ESP_UUID_LEN_16;
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[0].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_BLE_CONTROL_COM;
         esp_ble_gatts_add_char(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle,
                                &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[0].char_uuid,
                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                                &ble_control_char_val,
                                &controlAttrControl);
         break;
 
     case ESP_GATTS_ADD_INCL_SRVC_EVT:
         break;
 
     case ESP_GATTS_ADD_CHAR_EVT:
         log_info("ble_control: ESP_GATTS_ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                  param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
 
         if (param->add_char.status) break;
 
         for (int i = 0; i < gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar; ++i) {
             if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[i].char_uuid.uuid.uuid16) {
                 gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[i].char_handle = param->add_char.attr_handle;
 
                 if (param->add_char.attr_handle == 0) {
                     log_error("ble_control: Invalid characteristic handle for UUID 0x%04x", param->add_char.char_uuid.uuid.uuid16);
                     break;
                 }
 
                 // Add CCCD cho BLE control COM
                 if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_BLE_CONTROL_COM) {
                     log_info("ble_control: Adding CCCD for BLE control COM characteristic");
                     esp_attr_control_t cccdAttrControl = { .auto_rsp = ESP_GATT_RSP_BY_APP };
                     esp_err_t ret = esp_ble_gatts_add_char_descr(
                         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle,
                         &cccd_uuid,
                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                         NULL, &cccdAttrControl);
                     if (ret != ESP_OK) {
                         log_error("ble_control: Failed to add CCCD, error: %s", esp_err_to_name(ret));
                     } else {
                         log_info("ble_control: CCCD addition initiated");
                     }
                 }
 
                 if (i == (gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar - 1)) {
                     log_info("init ble control done!");
                 }
             }
         }
         break;
 
     case ESP_GATTS_ADD_CHAR_DESCR_EVT:
         log_info("ble_control: ESP_GATTS_ADD_CHAR_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                  param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
         if (param->add_char_descr.status == ESP_GATT_OK) {
             gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle = param->add_char_descr.attr_handle;
             log_info("ble_control: CCCD descriptor added handle %d", param->add_char_descr.attr_handle);
             if (param->add_char_descr.descr_uuid.len == ESP_UUID_LEN_16) {
                 log_info("ble_control: CCCD descriptor UUID: 0x%04x", param->add_char_descr.descr_uuid.uuid.uuid16);
             }
         } else {
             log_error("ble_control: CCCD descriptor addition failed status %d", param->add_char_descr.status);
         }
         break;
 
     case ESP_GATTS_DELETE_EVT:
         break;
 
     case ESP_GATTS_START_EVT:
         log_info("ble_control: ESP_GATTS_START_EVT, status %d, service_handle %d",
                  param->start.status, param->start.service_handle);
         if (param->start.status == ESP_GATT_OK) {
             log_info("ble_control: Service 12CD started");
         } else {
             log_error("ble_control: Service 12CD start failed status %d", param->start.status);
         }
         break;
 
     case ESP_GATTS_STOP_EVT:
         break;
 
     case ESP_GATTS_CONNECT_EVT:
         log_warning("ble_control: ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
                     param->connect.conn_id,
                     param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                     param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
 
         if (isWifiCofg) break;
 
         esp_ble_conn_update_params_t conn_params;
         memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
         conn_params.latency = 0;
         conn_params.max_int = 24;  // ~30ms
         conn_params.min_int = 12;  // ~15ms
         conn_params.timeout = 400; // 4s
         esp_ble_gap_update_conn_params(&conn_params);
 
         gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].conn_id = param->connect.conn_id;
         s_bleControlConnected = true;
         break;
 
     case ESP_GATTS_CONF_EVT:
         log_info("ble_control: ESP_GATTS_CONF_EVT status %d attr_handle %d (confirm of indicate)", param->conf.status, param->conf.handle);
         if (param->conf.status != ESP_GATT_OK) {
             esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
         }
         break;
 
     case ESP_GATTS_DISCONNECT_EVT:
         s_bleControlConnected = false;
         log_warning("ble_control: ESP_GATTS_DISCONNECT_EVT");
         if (isWifiCofg) break;
         esp_ble_gap_start_advertising(&ble_adv_params);
         break;
 
     case ESP_GATTS_SET_ATTR_VAL_EVT: {
         log_info("ble_control: ESP_GATTS_SET_ATTR_EVT: status %d attr_handle %d", param->set_attr_val.status, param->set_attr_val.attr_handle);
         char bufAttr[100] = "";
         const uint8_t* pAttr; uint16_t len;
         esp_ble_gatts_get_attr_value(param->set_attr_val.attr_handle, &len, &pAttr);
         memcpy(bufAttr, pAttr, len);
         bufAttr[len] = '\0';
         printf("get attr: %s\n", bufAttr);
         break;
     }
 
     case ESP_GATTS_OPEN_EVT:
     case ESP_GATTS_CANCEL_OPEN_EVT:
     case ESP_GATTS_CLOSE_EVT:
     case ESP_GATTS_LISTEN_EVT:
     case ESP_GATTS_CONGEST_EVT:
     default:
         break;
     }
 }
 
 static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
 {
     /* If event is register event, store the gatts_if for each profile */
     if (event == ESP_GATTS_REG_EVT) {
         if (param->reg.status == ESP_GATT_OK) {
             gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
         } else {
             log_info("Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
             return;
         }
     }
 
     /* Dispatch to profiles */
     do {
         for (int idx = 0; idx < PROFILE_NUM; idx++) {
             if (gatts_if == ESP_GATT_IF_NONE ||
                 gatts_if == gl_profile_tab[idx].gatts_if) {
                 if (gl_profile_tab[idx].gatts_cb) {
                     gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                 }
             }
         }
     } while (0);
 }
 
 /*******************************************************************************
  * Application Functions
  ******************************************************************************/
 void BLE_init()
 {
     printf("Init Bluetooth\n");
 
     esp_err_t ret;
     ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
     esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
 
     ret = esp_bt_controller_init(&bt_cfg);
     if (ret) {
         log_error("%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
         return;
     }
 
     ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
     if (ret) {
         log_error("%s enable controller failed: %s", __func__, esp_err_to_name(ret));
         return;
     }
     ret = esp_bluedroid_init();
     if (ret) {
         log_error("%s init bluetooth failed", __func__);
         return;
     }
     ret = esp_bluedroid_enable();
     if (ret) {
         log_error("%s enable bluetooth failed", __func__);
         return;
     }
 
     esp_ble_gatts_register_callback(gatts_event_handler);
     esp_ble_gap_register_callback(gap_event_handler);
     esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
     if (local_mtu_ret) {
         log_error("set local MTU failed, error code = %x", local_mtu_ret);
     }
 
     char name[35] = "HT-";
     strcat(name, g_product_Id);
     printf("BLE Name: %s\n", name);
     esp_ble_gap_set_device_name(name);
     ble_inited = true;
 }
 
 void BLE_startConfigMode()
 {
     if (!ble_inited) {
         BLE_init();
     }
 
     if (!registeredConfigService) {
         wifi_list_char_val.attr_len = Wifi_ListSsidLen;
         wifi_list_char_val.attr_value = wifi_list_char_str;
         ble_adv_params.adv_type = ADV_TYPE_IND;
         esp_ble_gatts_app_register(PROFILE_CONFIG_WIFI_APP_ID);
         registeredConfigService = true;
     } else {
         printf("change advertising data config wifi\n");
         esp_ble_gap_config_adv_data(&config_wifi_adv_data);
     }
 }
 
 void BLE_startControlMode()
 {
     if (!registeredControlService) {
         if (!ble_inited) {
             BLE_init();
         }
         registeredControlService = true;
         esp_ble_gatts_app_register(PROFILE_BLE_CONTROL_APP_ID);
     } else {
         if (!ble_inited) {
             return;
         }
         printf("change advertising data BLE\n");
     }
 }
 
 void BLE_releaseBle()
 {
     if (ble_inited) {
         esp_bluedroid_disable();
         esp_bluedroid_deinit();
     }
     esp_bt_controller_disable();
     esp_bt_controller_deinit();
     esp_bt_mem_release(ESP_BT_MODE_BTDM);
 
     ble_inited = false;
     s_bleConfigConnected = false;
     s_bleControlConnected = false;
     registeredConfigService = false;
     registeredControlService = false;
     printf("release ble done\n");
 }
 
 void BLE_getMesParamIndex(uint8_t *cmdLine, uint16_t cmdLineLen, uint16_t *indexList, uint8_t *indexNum)
 {
     for (uint8_t i = 0; i < cmdLineLen; i++) {
         if ((cmdLine[i] == START_PARAM_CHAR) && (*indexNum == 0)) {
             indexList[*indexNum] = i + 1;
             *indexNum += 1;
         }
 
         if ((cmdLine[i] == PARAM_SEPARATE) && (*indexNum > 0)) {
             indexList[*indexNum] = i + 1;
             *indexNum += 1;
             if (*indexNum >= MAX_PARAM_NUM) {
                 return;
             }
         }
     }
 }
 
 void processCmd(uint8_t *cmdLine, uint16_t cmdLineLen)
 {
     printf("...new command line\n");
     uint16_t paramIndexList[MAX_PARAM_NUM];
     uint8_t paramIndexNum = 0;
     BLE_getMesParamIndex(cmdLine, cmdLineLen, paramIndexList, &paramIndexNum);
 
     if (paramIndexNum == 0) {
         log_error("command have no param");
         return;
     }
     char cmdTypeStr[LEN_OF_PRE_CMD + 1];
     if (paramIndexList[0] > (LEN_OF_PRE_CMD + 1)) {
         log_error("command type str too long");
         return;
     }
     memcpy(cmdTypeStr, cmdLine, paramIndexList[0]);
     cmdTypeStr[paramIndexList[0]] = 0;
 
     getInfoMobileToEsp = true;
     if (strncmp(PRE_CMD_USE_WIFI, cmdTypeStr, paramIndexList[0]) == 0) {
         if (isHardReset) {
             log_error("hard reset is running");
             return;
         }
         if (paramIndexNum != 2) {
             log_error("wifi password message, syntax error");
             return;
         }
         memcpy(g_new_ssid, cmdLine + paramIndexList[0], (paramIndexList[1] - paramIndexList[0] - 1));
         g_new_ssid[paramIndexList[1] - paramIndexList[0] - 1] = '\0';
         memcpy(g_new_pwd, cmdLine + paramIndexList[1], (cmdLineLen - paramIndexList[1]));
         g_new_pwd[cmdLineLen - paramIndexList[1]] = '\0';
 
         if (isWifiCofg || infoFactoryDefault.checkFactoryDefault) {
             g_haveNewSsid = true;
         }
     } else if (strncmp(PRE_CMD_END, cmdTypeStr, paramIndexList[0]) == 0) {
         if (isHardReset) {
             GatewayConfig_receivedHardResetDone();
             return;
         }
         GatewayConfig_receivedBleDone();
     }
 }
 
 void BLE_sentToMobile(const char *sentMes)
 {
     if (!ble_inited) {
         log_error("BLE not initialized");
         return;
     }
 
     if (!s_bleConfigConnected) {
         log_error("BLE not connected");
         return;
     }
 
     if (!cccd_notifications_enabled) {
         log_warning("Notifications not enabled by client, skipping send");
         return;
     }
 
     // Notify qua COM (0xBD02) mặc định — ứng với charInsts[1]
     uint16_t h = gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[1].char_handle;
     esp_err_t errRc = esp_ble_gatts_send_indicate(
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].gatts_if,
         gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].conn_id,
         h,
         strlen(sentMes),
         (uint8_t *)sentMes,
         false // notify (không cần confirm)
     );
 
     if (errRc != ESP_OK) {
         log_error("<< esp_ble_gatts_send_indicate: rc=%d, error=%s", errRc, esp_err_to_name(errRc));
     } else {
         log_info(" >> send notify ok: %s", sentMes);
     }
 }
 
 void writeToConfigWifiComCharEvent(uint8_t len, uint8_t *data)
 {
     static uint8_t cmdLine[100];
     static uint8_t cmdLineLen = 0;
     if (cmdLineLen == 0 && data[0] != BEGIN_OF_CMD) {
         return;
     }
     memcpy(cmdLine + cmdLineLen, data, len);
     cmdLineLen += len;
     ht_print_data(data, len);
     printf("data end: %x\n", data[len - 1]);
     if (data[len - 1] == END_OF_CMD) {
         processCmd(cmdLine, cmdLineLen - 1);
         cmdLineLen = 0;
     }
 }
 
 void BLE_reAdvertising()
 {
     if (!ble_inited) {
         log_error("wait BLE init...");
         return;
     }
     esp_ble_gap_stop_advertising();
     vTaskDelay(1000/portTICK_PERIOD_MS);
     char name[35] = "HT-";
     strcat(name, g_product_Id);
 
     if (isHardReset) {
         strcat(name, "-RS");
     }
     printf("BLE Name: %s\n", name);
     esp_ble_gap_set_device_name(name);
     esp_ble_gap_config_adv_data(&config_wifi_adv_data);
 }
 
 void BLE_startModeHardReset()
 {
     BLE_startControlMode();
     BLE_startConfigMode();
     BLE_reAdvertising();
 }
 
 void BLE_iBeaconSetSpecial(uint16_t cid, uint16_t major, uint16_t minor)
 {
     if (!ble_inited) {
         log_error("wait BLE init...");
         return;
     }
 
     uint8_t raw_ibeacon_data[] = {
         0x02, 0x01, 0x06,
         0x1A, 0xFF,
         0x4C, 0x00,
         0x02, 0x15,
         0xFC, 0x34, 0x9B, 0x5A, 0x80, 0x00, 0x05, 0x80, 0x00, 0x10, 0x91, 0x12, 0x20, 0x92, 0x10, 0x10,
         0xFF, 0x0C,
         0x24, 0x6E,
         0xC5
     };
 
     raw_ibeacon_data[5]  = (cid) & 0xff;
     raw_ibeacon_data[6]  = (cid>>8) & 0xff;
 
     raw_ibeacon_data[25] = (major>>8) & 0xff;
     raw_ibeacon_data[26] = (major) & 0xff;
 
     raw_ibeacon_data[27] = (minor>>8) & 0xff;
     raw_ibeacon_data[28] = (minor) & 0xff;
 
     ble_adv_params.adv_type = ADV_TYPE_NONCONN_IND;
     esp_err_t status = esp_ble_gap_config_adv_data_raw(raw_ibeacon_data, sizeof(raw_ibeacon_data));
 
     if (status == ESP_OK) {
         printf(" >> Config iBeacon:: Cid [0x%04x] Major [%d] Minor [%d]\n", cid, major, minor);
     } else {
         log_error("Config iBeacon Data Failed: %s", esp_err_to_name(status));
     }
 }

 // Gửi dữ liệu trên BD02 theo trạng thái CCCD (notify/indicate)
static void BLE_sendOnBD02(const uint8_t *data, uint16_t len) {
    if (!ble_inited || !s_bleConfigConnected) return;

    uint16_t c = cfg_cccd[1]; // BD02
    if ((c & 0x0001) == 0) { // chỉ kiểm tra notify
        log_warning("BD02: client not subscribed notify (CCCD=0x%04x), skip send", c);
        return;
    }

    uint16_t h = gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].charInsts[1].char_handle; // BD02

    esp_err_t err = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].gatts_if,
        gl_profile_tab[PROFILE_CONFIG_WIFI_APP_ID].conn_id,
        h,
        len,
        (uint8_t*)data,
        false // chỉ notify, không indicate
    );
    if (err != ESP_OK) {
        log_error("send_notify BD02 err=%d %s", err, esp_err_to_name(err));
    } else {
        log_info("BD02: sent %d bytes (notify)", len);
    }
}

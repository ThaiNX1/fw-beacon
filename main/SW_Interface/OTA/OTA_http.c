/**
 ******************************************************************************
 * @file    OTA_http.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "OTA_http.h"
#include "Wifi_Handler.h"
#include "BLE_handler.h"
#include "WatchDog.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Ota_Http"

#ifndef DISABLE_LOG_ALL
#define HTTP_LOG_INFO_ON
#endif

#ifdef HTTP_LOG_INFO_ON
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
extern char *s_linkDownload;

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool hasOtaTask = false;
esp_ota_handle_t update_handle = 0;
const esp_partition_t *update_partition = NULL;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
bool Init_Ota();
void End_Ota();

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
esp_err_t downloadUpdateFile_callback(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        log_error("HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        log_warning("HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        log_info("HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        log_info("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        log_info("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client)) {
            printf(" -> esp_ota_write.%d\n", evt->data_len);
            esp_ota_write(update_handle, (const void *)evt->data, (size_t)evt->data_len);
            vTaskDelay(2/portTICK_PERIOD_MS);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        log_warning("HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        log_error("HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            log_info("Last esp error code: 0x%x", err);
            log_info("Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

bool downloadUpdateFile(char *linkFile)
{
    if (Init_Ota() == false) {
        return false;
    }

    log_warning("start download file: %s", linkFile);
    bool result = false;

    esp_http_client_config_t config = {
        .url = linkFile,
        .event_handler = downloadUpdateFile_callback,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_warning("downloadUpdateFile status = %d, content_length = %lld", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201) {
            End_Ota();
            result = true;
        }
    } else {
        log_error("downloadUpdateFile request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return result;
}

bool Init_Ota()
{
    esp_err_t err;
    bool result = false;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */

    log_warning(" --> Starting OTA ESP...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        log_error("Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx", configured->address, running->address);
        log_error("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    printf(" ->> Running partition type %d subtype %d (offset 0x%08lx)\n", running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    printf(" -->> Writing to partition subtype %d at offset 0x%lx\n", update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        log_error("esp_ota_begin failed, error=%d", err);
        result = false;
        ESP_resetChip();
    } else {
        result = true;
        log_warning(" ->> esp_ota_begin success");
    }
    return result;
}

void End_Ota()
{
    if (esp_ota_end(update_handle) != ESP_OK) {
        log_error("esp_ota_end failed!");
        ESP_resetChip();
    }

    esp_err_t err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        log_error("esp_ota_set_boot_partition failed! err=0x%x", err);
        ESP_resetChip();
    }

    log_warning(" --->>>> Prepare to restart system..........");
    ESP_resetChip();
}

static void otaTask(void *arg)
{
    hasOtaTask = true;
    uint32_t maxTimeOtaForEsp = 0;

    WIFI_HANDLER_WAIT_CONECTED_NOMAL_FOREVER;
    BLE_releaseBle();
    vTaskDelay(100/portTICK_PERIOD_MS);

    if (downloadUpdateFile(s_linkDownload)) {
        log_warning("download file ok");
        while (true)
        {
            if (maxTimeOtaForEsp == 0) {
                maxTimeOtaForEsp = xTaskGetTickCount();
            }
            if (xTaskGetTickCount() - maxTimeOtaForEsp > MAX_TIME_OTA_FOR_ESP) {
                log_warning(" --> timeout ota for esp, restart...");
                ESP_resetChip();
            }
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
    } else {
        log_error(" --> download file fail");
        ESP_resetChip();
    }

    vTaskDelay(1000/portTICK_PERIOD_MS);  
    hasOtaTask = false;
    vTaskDelete(NULL);
}

static void OtaHttp_createDoOtaTask()
{
    if (!hasOtaTask) {
        xTaskCreate(otaTask, "otaTask", 4*1024, NULL, 5, NULL);
    }
}

void OTA_http_DoOTA(char *linkFile, uint8_t phase)
{
    if (hasOtaTask) {
        log_error("Task OTA is running");
        return;
    }

    log_info("Do OTA link: %s", linkFile);
    if (phase == PHASE_OTA_ESP) {
        log_info("Phase OTA ESP");
    } else {
        log_error("Phase OTA invalid");
        return;
    }
    
    OtaHttp_createDoOtaTask();
}

/***********************************************/
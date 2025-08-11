/**
 ******************************************************************************
 * @file    HttpHandler.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "HttpHandler.h"
#include "MqttHandler.h"
#include "ProtocolHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "HTTP_Handler"

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
extern uint16_t lenCert;
extern uint16_t lenKey;
extern mqtt_certKey_t *pMqttCertKey;

/*******************************************************************************
 * Application Funtions
 ******************************************************************************/
static esp_err_t downloadCert_callback(esp_http_client_event_t *evt)
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
            memcpy(pMqttCertKey->cert + lenCert, evt->data,evt->data_len);
            lenCert += evt->data_len;
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

bool Http_downloadFileCert(char *linkFile)
{
    bool result = false;
    esp_http_client_config_t config = {
        .url = linkFile,
        .event_handler = downloadCert_callback,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info("download ceritficate file Status = %d, content_length = %lld", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201) {
            result = true;
            pMqttCertKey->cert[lenCert] = '\0';
            log_info("download ceritficate file OK");
        }
    } else {
        log_error("download ceritficate file failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return result;
}

static esp_err_t downloadKey_callback(esp_http_client_event_t *evt)
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
            memcpy(pMqttCertKey->key + lenKey, evt->data,evt->data_len);
            lenKey += evt->data_len;
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
            log_info( "Last esp error code: 0x%x", err);
            log_info( "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

bool Http_downloadFileKey(char *linkFile)
{
    bool result = false;
    esp_http_client_config_t config = {
        .url = linkFile,
        .event_handler = downloadKey_callback,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "download key file Status = %d, content_length = %lld", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
            pMqttCertKey->key[lenKey] = '\0';
            log_info( "download key file OK");
        }
    } else {
        log_error( "download key file failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    
    return result;
}

/***********************************************/
/**
 ******************************************************************************
 * @file    HTG_Utility.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "HTG_Utility.h"
#include "MqttHandler.h"

/*******************************************************************************
 * Extern Variables
 ******************************************************************************/
extern char g_product_Id[PRODUCT_ID_LEN];
extern char g_macDevice[6];
extern time_t time_now;

/*******************************************************************************
 * Variables
 ******************************************************************************/
char md5_sn_result[33] = "";
mbedtls_md5_context ht_md5_ctx;

/*******************************************************************************
 * Common Functions
 ******************************************************************************/
void ht_print_data(uint8_t *data, uint32_t len) 
{
	printf("\n/**********************************************/\n");
	for (uint32_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
	printf("\n/**********************************************/\n");
}

void ht_print_binary(uint8_t data) 
{
	uint8_t bit[8];
	for (uint8_t i = 0; i < 8; i++) {
		bit[i] = (data>>i) & 0x01;
    }
	printf(" => Hex: 0x%02x, Dec: %d, Bin: 0b%d%d%d%d%d%d%d%d\n", data, data, bit[7], bit[6], bit[5], bit[4], bit[3], bit[2], bit[1], bit[0]);
}

uint16_t ht_check_crc16(uint8_t *data, size_t length) 
{
    uint16_t crc = 0xFFFF;
    uint16_t polynomial = 0x04C1;

    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint32_t ht_check_crc32(uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t polynomial = 0x04C11DB7;

    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint32_t)data[i] << 24);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

void ht_hmac_sha1(uint8_t *key, size_t key_len, uint8_t *message, size_t message_len, uint8_t *output) 
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, message, message_len);
    mbedtls_md_hmac_finish(&ctx, output);
    mbedtls_md_free(&ctx);
}

uint32_t ht_generate_totp(uint8_t *key, size_t key_len, uint64_t timestamp)
{
    uint64_t time_counter = timestamp/300;
    uint8_t message[8];

    for (int i = 7; i >= 0; i--) {
        message[i] = time_counter & 0xFF;
        time_counter >>= 8;
    }

    uint8_t hmac_result[20];
    ht_hmac_sha1(key, key_len, message, sizeof(message), hmac_result);

    int offset = hmac_result[19] & 0x0F;
    uint32_t otp = (hmac_result[offset] & 0x7F) << 24 | (hmac_result[offset + 1] & 0xFF) << 16 | (hmac_result[offset + 2] & 0xFF) << 8 | (hmac_result[offset + 3] & 0xFF);

    otp = otp % 65536;
    
    return otp;
}

/*******************************************************************************
 * MQTT Functions
 ******************************************************************************/
void pubTopicFromProductId(char* product_Id, char* eventType, char* propertyCode, char* topicName)
{
	sprintf(topicName, "device/%s/pub/%s/%s", product_Id, eventType, propertyCode);
}

void subTopicFromProductId(char* product_Id, char* topicName)
{
	sprintf(topicName, "device/%s/com/#", product_Id);
}

bool buffer_add(uint8_t *buff, uint16_t *buffLen, uint8_t *newData, uint16_t newDataLen)
{
	memcpy(buff + *buffLen, newData, newDataLen);
	*buffLen += newDataLen;
	return true;
}

/*******************************************************************************
 * Special Functions
 ******************************************************************************/
void md5_checksum_begin()
{
    mbedtls_md5_init(&ht_md5_ctx);
    mbedtls_md5_starts(&ht_md5_ctx);
}

void calculate_md5_checksum_multi(uint8_t *data_in, uint32_t data_len, uint8_t step)
{
	for (uint8_t i = 0; i < step; i++) {
        printf("data_in %p, data_len %lu, step %d\n", data_in+(data_len*i), data_len, i);
		mbedtls_md5_update(&ht_md5_ctx, data_in+(data_len*i), data_len);
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

void calculate_md5_checksum_single(uint8_t *data_in, uint32_t data_len)
{
    printf("data_in %p, data_len %lu\n", data_in, data_len);
    mbedtls_md5_update(&ht_md5_ctx, data_in, data_len);
	vTaskDelay(100/portTICK_PERIOD_MS);
}

void md5_checksum_finish(char *result) 
{
	uint8_t data_out[16];
    mbedtls_md5_finish(&ht_md5_ctx, data_out);
    mbedtls_md5_free(&ht_md5_ctx);

    char buf[10];
	for (uint8_t i = 0; i < 16; i++) {
        sprintf(buf, "%02x", data_out[i]);
        strncat(result, buf, 2);
    }
    printf(" (-_-) --> HT_Md5_Check_Sum: \"%s\" --> (^_^) \n", result);
}

uint16_t ht_check_crc16_mac_device()
{
	char mac_device_string[20] = "";
	sprintf(mac_device_string, "%02x%02x%02x%02x%02x%02x", g_macDevice[0], g_macDevice[1], g_macDevice[2], g_macDevice[3], g_macDevice[4], g_macDevice[5]);
	uint16_t result = ht_check_crc16((uint8_t*)mac_device_string, strlen(mac_device_string));
	return result;
}

void md5_checksum_serial_number()
{
    md5_checksum_begin();
    calculate_md5_checksum_single((uint8_t*)g_product_Id, strlen(g_product_Id));
    md5_checksum_finish(md5_sn_result);
}

uint32_t ht_check_crc32_sn_device()
{
	uint32_t result = ht_check_crc32((uint8_t*)md5_sn_result, strlen(md5_sn_result));
	return result;
}

uint32_t ht_gen_topt()
{
    char key_topt[20] = "";
    uint32_t crc32_sn = ht_check_crc32_sn_device();
    sprintf(key_topt, "HT-%08lx", crc32_sn);

    uint32_t result = ht_generate_totp((uint8_t*)key_topt, strlen(key_topt), (uint64_t)time_now);
    return result;
}

/*******************************************************************************
 * Generate Signature Functions
 ******************************************************************************/
char *ht_convert_escaped_newlines(const char *input, char *output)
{
    size_t len = strlen(input);
    if (!output) {
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\' && input[i + 1] == 'n') {
            output[j++] = '\n';
            i++; // skip 'n'
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    return output;
}

void ht_gen_signature(const char *rsa_private_key_pem)
{
    int ret;
    const char *pers = (const char*)DEVICE_PWD;
    char *output_pem = (char *)malloc(strlen(rsa_private_key_pem) + 1);
    char signature_string[1025] = "";
    char message_temp[100] = "";

    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    uint8_t hash[32];
    uint8_t signature[512];
    size_t sig_len = 0;

    sprintf(message_temp, "%s:%s", g_product_Id, DEVICE_KEY_SIGN);
    const uint8_t *message = (const uint8_t *)message_temp;

    // Init
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const uint8_t *)pers, strlen(pers));
    if (ret != 0) {
        printf("Seed failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    // Convert PEM Key
    char *pem_clean = ht_convert_escaped_newlines(rsa_private_key_pem, output_pem);
    if (!pem_clean) {
        printf("Memory allocation failed for PEM key conversion.\n");
        goto cleanup;
    }
    printf(">>PEM Key:\n%s\n", pem_clean);

    ret = mbedtls_pk_parse_key(&pk, (const uint8_t *)pem_clean, strlen(pem_clean) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printf("Key parse failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
        printf("Key is not an RSA private key!\n");
        goto cleanup;
    }

    // SHA-256 Hash
    ret = mbedtls_sha256(message, strlen((const char *)message), hash, 0);
    if (ret != 0) {
        printf("mbedtls_sha256 failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    printf(">>SHA256 hash of message:\n");
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");

    // Sign
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, sizeof(signature), &sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printf("RSA-SHA256 sign failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    printf(">>Signature (%d bytes):\n", (int)sig_len);
    MQTT_SubscribeTopicGetCert();
    vTaskDelay(100/portTICK_PERIOD_MS);
    for (size_t i = 0; i < sig_len; i++) {
        sprintf(signature_string + (i*2), "%02x", signature[i]);
    }
    MQTT_PublishRequestGetCert(signature_string);

cleanup:
    free(output_pem);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void build_payload_json(char* dest, char* model, char* url)
{
    sprintf(dest, "{\"model\":\"%s\",\"url\":\"%s\"}", model, url);
}

bool verify_ota_signature(char* model, char* url, char* received_sig_hex, char* ota_secret_key)
{
    char *json_payload = (char*)calloc(strlen(url) + strlen(model) + 50, 1);
    char hmac_hex[65] = "";
    uint8_t hmac_output[32] = {0};

    build_payload_json(json_payload, model, url);

    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        free(json_payload);
        return false;
    }

    int ret = mbedtls_md_hmac(md_info, (uint8_t*)ota_secret_key, strlen(ota_secret_key), (uint8_t*)json_payload, strlen(json_payload), hmac_output);
    if (ret != 0) {
        free(json_payload);
        return false;
    }

    for (int i = 0; i < 32; ++i) {
        sprintf(hmac_hex + (i*2), "%02x", hmac_output[i]);
    }

    free(json_payload);
    return strcmp(hmac_hex, received_sig_hex) == 0;
}

/***********************************************/
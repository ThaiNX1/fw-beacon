/**
 ******************************************************************************
 * @file    HTG_Utility.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __HTG_UTILITY_H
#define __HTG_UTILITY_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define HT_GET_BIT(val, bit)           ((val>>bit) & 0x01)
#define HT_SET_BIT(val, bit)           (val |= (0x01<<bit))
#define HT_CLEAR_BIT(val, bit)         (val &= ~(0x01<<bit))
#define HT_TOGGLE_BIT(val, bit)        (val ^= (0x01<<bit))

/* Exported functions ------------------------------------------------------- */
void ht_print_data(uint8_t *data, uint32_t len);
void ht_print_binary(uint8_t data);
uint16_t ht_check_crc16(uint8_t *data, size_t length);
uint32_t ht_check_crc32(uint8_t *data, size_t length);
void ht_hmac_sha1(uint8_t *key, size_t key_len, uint8_t *message, size_t message_len, uint8_t *output);
uint32_t ht_generate_totp(uint8_t *key, size_t key_len, uint64_t timestamp);

void pubTopicFromProductId(char* product_Id, char* eventType, char* propertyCode, char* topicName);
void subTopicFromProductId(char* product_Id, char* topicName);
bool buffer_add(uint8_t *buff, uint16_t *buffLen, uint8_t *newData, uint16_t newDataLen);

void md5_checksum_begin();
void calculate_md5_checksum_multi(uint8_t *data_in, uint32_t data_len, uint8_t step);
void calculate_md5_checksum_single(uint8_t *data_in, uint32_t data_len);
void md5_checksum_finish(char *result);

uint16_t ht_check_crc16_mac_device();
void md5_checksum_serial_number();
uint32_t ht_check_crc32_sn_device();
uint32_t ht_gen_topt();

void ht_gen_signature(const char *rsa_private_key_pem);
bool verify_ota_signature(char* model, char* url, char* received_sig_hex, char* ota_secret_key);

#endif /* __HTG_UTILITY_H */
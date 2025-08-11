/**
 ******************************************************************************
 * @file    OTA_http.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __OTA_HTTP_H
#define __OTA_HTTP_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    PHASE_OTA_NONE = 0,
    PHASE_OTA_ESP,
} phase_ota_mcu;

/* Exported macro ------------------------------------------------------------*/
#define MAX_TIME_OTA_FOR_ESP    (5*60000)

/* Exported functions ------------------------------------------------------- */
void OTA_http_DoOTA(char *linkFile, uint8_t phase);

#endif /* __OTA_HTTP_H */

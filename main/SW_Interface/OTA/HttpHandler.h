/**
 ******************************************************************************
 * @file    HttpHandler.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __HTTP_HANDLER_H
#define __HTTP_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
bool Http_downloadFileCert(char *linkFile);
bool Http_downloadFileKey(char *linkFile);

#endif /* __HTTP_HANDLER_H */
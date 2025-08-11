/**
 ******************************************************************************
 * @file    OutputControl.h
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/

#ifndef __OUTPUT_CONTROL_H
#define __OUTPUT_CONTROL_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    BT_STATE_RELEASE = 0,
    BT_STATE_PRESS,
    BT_STATE_HOLD,
} buttonState_t;

typedef enum {
    BUTTON_1 = 0,
    BUTTON_2,
    BUTTON_3,
    BUTTON_MAX
} buttonIndex_t;

typedef enum {
    LED_COLOR_OFF = 0,
    LED_COLOR_RED,
    LED_COLOR_BLUE,
    LED_COLOR_PINK,
} ledColor_t;

typedef struct
{
    buttonState_t state;
    uint32_t time_press;
} buttonControl_t;

/* Exported macro ------------------------------------------------------------*/
#define HOLD_TIME           (5000)

#define LED_RED             GPIO_NUM_4
#define LED_BLUE            GPIO_NUM_17

#define LED_T1_ON           GPIO_NUM_13
#define LED_T1_OFF          GPIO_NUM_14

#define LED_T2_ON           GPIO_NUM_26
#define LED_T2_OFF          GPIO_NUM_25

#define LED_T3_ON           GPIO_NUM_33
#define LED_T3_OFF          GPIO_NUM_32

#define TOUCH_1             GPIO_NUM_27
#define RELAY_1             GPIO_NUM_23

#define TOUCH_2             GPIO_NUM_35
#define RELAY_2             GPIO_NUM_22

#define TOUCH_3             GPIO_NUM_34
#define RELAY_3             GPIO_NUM_21

/* Exported functions ------------------------------------------------------- */
void GPIO_Initialize();

void Out_setColor(ledColor_t color);
void Out_ledWaitConnect();
void Out_ledFailConnect();
void Out_ledHardReset();

void Out_setRelay(buttonIndex_t btn, bool state);
void Out_toggleRelay(buttonIndex_t btn);
void Out_button_process();
void Out_publishStateRelayDefault();

#endif /* __OUTPUT_CONTROL_H */
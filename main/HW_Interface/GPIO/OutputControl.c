/**
 ******************************************************************************
 * @file    OutputControl.c
 * @author  HT EZLife Co.,LTD.
 * @date    January 1, 2025
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "OutputControl.h"
#include "gateway_config.h"
#include "MqttHandler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TAG "Output_Control"

#ifndef DISABLE_LOG_ALL
#define OUTPUT_CONTROL_LOG_INFO_ON
#endif

#ifdef OUTPUT_CONTROL_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define log_error(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#define log_warning(format, ...)
#endif

#define GPIO_OUTPUT_PIN_SEL  	((1ULL<<LED_RED) | (1ULL<<LED_BLUE) | 		\
					 			 (1ULL<<LED_T1_ON) | (1ULL<<LED_T1_OFF) | 	\
					 			 (1ULL<<LED_T2_ON) | (1ULL<<LED_T2_OFF) | 	\
								 (1ULL<<LED_T3_ON) | (1ULL<<LED_T3_OFF) | 	\
								 (1ULL<<RELAY_1) | (1ULL<<RELAY_2) | (1ULL<<RELAY_3))

#define GPIO_INPUT_PIN_SEL  	((1ULL<<TOUCH_1) | (1ULL<<TOUCH_2) | (1ULL<<TOUCH_3))

#define ESP_INTR_FLAG_DEFAULT 	0

/*******************************************************************************
 * Variables
 ******************************************************************************/
bool relay_state[BUTTON_MAX] = {false, false, false};

buttonControl_t buttons[BUTTON_MAX] = {
    [BUTTON_1] = {BT_STATE_RELEASE, 0},
    [BUTTON_2] = {BT_STATE_RELEASE, 0},
    [BUTTON_3] = {BT_STATE_RELEASE, 0}
};

static QueueHandle_t gpio_evt_queue = NULL;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void button_pressed_cb(buttonIndex_t btn);
void button_release_cb(buttonIndex_t btn);

/*******************************************************************************
 * Initialize GPIO
 ******************************************************************************/
void Out_setGpio(gpio_num_t gpio, uint8_t state)
{
	gpio_set_level(gpio, state);
}

void Out_start()
{
	Out_setColor(LED_COLOR_OFF);
    Out_setRelay(BUTTON_1, false);
    Out_setRelay(BUTTON_2, false);
    Out_setRelay(BUTTON_3, false);
}

static buttonIndex_t get_button_index(gpio_num_t gpio)
{
    switch (gpio) {
	case TOUCH_1: 
		return BUTTON_1;
	case TOUCH_2: 
		return BUTTON_2;
	case TOUCH_3: 
		return BUTTON_3;
	default: 
		return BUTTON_MAX;
    }
}

static void IRAM_ATTR bt_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void task_process_bt(void* arg)
{
    uint32_t bt_num = 0;

    while (1)
	{
        if (xQueueReceive(gpio_evt_queue, &bt_num, portMAX_DELAY)) {
            buttonIndex_t btn = get_button_index((gpio_num_t)bt_num);
            if (btn >= BUTTON_MAX) {
				continue;
			}

            if (gpio_get_level(bt_num) == 0) {
                button_pressed_cb(btn);
            } else {
                button_release_cb(btn);
            }
        }
    }
}

void GPIO_Initialize()
{
	gpio_config_t io_conf_out;
	//disable interrupt
	io_conf_out.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf_out.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set
	io_conf_out.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf_out.pull_down_en = GPIO_PULLDOWN_DISABLE;
	//disable pull-up mode
	io_conf_out.pull_up_en = GPIO_PULLUP_DISABLE;
	//configure GPIO with the given settings
	gpio_config(&io_conf_out);

	gpio_config_t io_conf_in;
	//interrupt of rising edge
    io_conf_in.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins
    io_conf_in.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf_in.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf_in.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf_in);

	//create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(task_process_bt, "process_bt", 3*1024, NULL, 10, NULL);

	//install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(TOUCH_1, bt_isr_handler, (void*) TOUCH_1);
	gpio_isr_handler_add(TOUCH_2, bt_isr_handler, (void*) TOUCH_2);
	gpio_isr_handler_add(TOUCH_3, bt_isr_handler, (void*) TOUCH_3);

    Out_start();
    log_info("GPIO Initialized");
}

/*******************************************************************************
 * Led Functions
 ******************************************************************************/
void Out_setColor(ledColor_t color)
{
    switch (color) {
	case LED_COLOR_OFF:
		Out_setGpio(LED_RED, 0);
		Out_setGpio(LED_BLUE, 0);
		break;
	case LED_COLOR_RED:
		Out_setGpio(LED_RED, 1);
		Out_setGpio(LED_BLUE, 0);
		break;
	case LED_COLOR_BLUE:
		Out_setGpio(LED_RED, 0);
		Out_setGpio(LED_BLUE, 1);
		break;
	case LED_COLOR_PINK:
		Out_setGpio(LED_RED, 1);
		Out_setGpio(LED_BLUE, 1);
		break;
    }
}

void Out_ledWaitConnect()
{
	static uint32_t last_tick_wcnt = 0;
    static bool led_toggle = false;
    const uint32_t interval = 1000;

    uint32_t now = xTaskGetTickCount();

    if (now - last_tick_wcnt >= interval) {
        last_tick_wcnt = now;
        led_toggle = !led_toggle;

        if (led_toggle) {
            Out_setColor(LED_COLOR_RED);
        } else {
            Out_setColor(LED_COLOR_BLUE);
        }
    }
}

void Out_ledFailConnect()
{
    for (int i = 0; i < 5; i++) {
        Out_setColor(LED_COLOR_RED);
        vTaskDelay(200/portTICK_PERIOD_MS);
        Out_setColor(LED_COLOR_OFF);
        vTaskDelay(200/portTICK_PERIOD_MS);
    }
    Out_setColor(LED_COLOR_OFF);
}

void Out_ledHardReset()
{
    static uint32_t last_tick_hrst = 0;
    static bool led_on = false;
    const uint32_t interval = 1000;
    uint32_t now = xTaskGetTickCount();

    if (now - last_tick_hrst >= interval) {
        last_tick_hrst = now;
        led_on = !led_on;

        if (led_on) {
            Out_setColor(LED_COLOR_PINK);
        } else {
            Out_setColor(LED_COLOR_OFF);
        }
    }
}

/*******************************************************************************
 * Button Functions
 ******************************************************************************/
void Out_setGpioRelay(buttonIndex_t btn, bool state)
{
    gpio_num_t led_on_pins[BUTTON_MAX]  = {LED_T1_ON, LED_T2_ON, LED_T3_ON};
    gpio_num_t led_off_pins[BUTTON_MAX] = {LED_T1_OFF, LED_T2_OFF, LED_T3_OFF};

    Out_setGpio(led_on_pins[btn], state ? 1 : 0);
    Out_setGpio(led_off_pins[btn], state ? 0 : 1);
}

void Out_setRelay(buttonIndex_t btn, bool state)
{
    gpio_num_t relay_pins[BUTTON_MAX] = {RELAY_1, RELAY_2, RELAY_3};

    relay_state[btn] = state;
    Out_setGpio(relay_pins[btn], state);
	Out_setGpioRelay(btn, state);
    log_warning(" [RELAY_%d,%d]", btn + 1, state);
    MQTT_PublishSwitchState(btn + 1, state);
}

void Out_toggleRelay(buttonIndex_t btn)
{
    gpio_num_t relay_pins[BUTTON_MAX] = {RELAY_1, RELAY_2, RELAY_3};

    relay_state[btn] = !relay_state[btn];
    Out_setGpio(relay_pins[btn], relay_state[btn]);
	Out_setGpioRelay(btn, relay_state[btn]);
    log_warning(" [RELAY_%d,%d]", btn + 1, relay_state[btn]);
    MQTT_PublishSwitchState(btn + 1, relay_state[btn]);
}

void button_pressed_cb(buttonIndex_t btn)
{
    buttons[btn].state = BT_STATE_PRESS;
    buttons[btn].time_press = xTaskGetTickCount();
}

void button_release_cb(buttonIndex_t btn)
{
    if (buttons[btn].state == BT_STATE_PRESS) {
        Out_toggleRelay(btn);
    }
    buttons[btn].state = BT_STATE_RELEASE;
}

void button_hold_cb(uint32_t time_hold)
{
    if (time_hold == HOLD_TIME) {
		log_warning(" [CONFIG_INIT,1]");
        GatewayConfig_init();
    }
}

void Out_button_process()
{
	for (buttonIndex_t btn = 0; btn < BUTTON_MAX; btn++) {
		if (buttons[btn].state != BT_STATE_RELEASE) {
			if ((xTaskGetTickCount() - buttons[btn].time_press) > HOLD_TIME) {
				if (buttons[btn].state == BT_STATE_PRESS) {
					buttons[btn].state = BT_STATE_HOLD;
					button_hold_cb(HOLD_TIME);
				}
			}
		}
	}
}

void Out_publishStateRelayDefault()
{
    for (buttonIndex_t btn = 0; btn < BUTTON_MAX; btn++) {
        MQTT_PublishSwitchState(btn + 1, relay_state[btn]);
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
    log_info("Publish state relay default");
}

/***********************************************/
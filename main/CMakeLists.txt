idf_component_register(SRCS     "Beacon_Device_main.c" 
                                HW_Interface/BLE/BLE_handler.c 
								HW_Interface/Flash_Driver/FlashHandler.c 
								HW_Interface/GPIO/OutputControl.c 
								HW_Interface/WatchDog/WatchDog.c 
								HW_Interface/Wifi/Wifi_handler.c 
								SW_Interface/DateTime/DateTime.c 
								SW_Interface/DateTime/myCronJob.c 
                                SW_Interface/Mqtt/MqttHandler.c 
								SW_Interface/Mqtt/ProtocolHandler.c 
								SW_Interface/OTA/HttpHandler.c 
								SW_Interface/OTA/OTA_http.c 
								SW_Interface/Wifi_Config/gateway_config.c  
								Utility/HTG_Utility.c 
								Utility/timeCheck.c 
                    INCLUDE_DIRS "."
                                HW_Interface/BLE 
								HW_Interface/Flash_Driver 
								HW_Interface/GPIO 
								HW_Interface/WatchDog 
								HW_Interface/Wifi 
								SW_Interface/DateTime 
								SW_Interface/Mqtt 
								SW_Interface/OTA 
								SW_Interface/Wifi_Config 
								Utility)

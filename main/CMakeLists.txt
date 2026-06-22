# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: MIT

#set(COMPONENT_INCLUDE_DIRS
#    .
#    ../config
#)

idf_component_register(SRCS "wifi_actor.c" "console_actor.c" "B394_DigitalInput.c" "main.c" "console_Actor.c" "UART_Actor.c" "wifi_Actor.c" "File_Manager_Actor.c" "195_Actor.c"
							"SD_Card_Actor.c" "JFS_FLASH_Actor.c" "HTTP_Actor.c" "LED_Actor.c" "Pushbutton_Actor.c"
							"iHub_Actor.c" "BLE_Actor.c" "ble/gatt_service.c" "ble/bsp.c" "ble/bsp_file.c" "ble/menu_builder.c"
							"ble/m225_methods.c" "ble/ble_comms_push.c" "ble/nxp_bridge.c" "ble/ble_security.c" "NTP_Actor.c"
							 "SMTP_Actor.c" "base64/base64.c"  "ping/ping.c" "UDP_Actor.c" "Tcp_Server_Actor.c" "Web_server_Actor.c"
							  "SQL_Actor.c" "ETH_Actor.c" "trimLightActor.c" "led_strip_encoder.c" "SYSTEM_Actor.c"
							 "EVENT_Actor.c" "SPIFFS_Actor.c" "System_Files_Actor.c" "rtc/pcf8563.c" "rtc/i2cdev.c" 
							 "split_argv.c"
#							 "cJsonMemPool.c"
#							 "Interface.c" "Coproc_Actor.c" 
#							 "cmd_Line.c"  
#							 "connect.c"  
#							 "File_Actor.c"  "file_server.c"  
#							 "EEPROM_Actor.c" "RF_Switch_Actor.c" "IDLE_Actor.c" 			 				    				 						 
							 
                    INCLUDE_DIRS "."  "protocol_examples_common" "protocol_examples_common/include" "ping" "base64"
                    			"ble" "ble/ble_services" "ble/utils" "rtc"
#                    				 "Config" "FS" "Sample/Driver/NOR" "Sample/OS" "SEGGER" "Sample/Driver/RAM" "Radio"
                    				
                    REQUIRES  "hal" "esp_wifi" "driver" "freertos" "log" "nvs_flash" "vfs" "console" "spi_flash" "esp_eth" "fatfs" "esp_https_server" "spiffs" "mbedtls" "json" "esp_timer"
                    			  "esp_http_client" "esp_hw_support" "esp_netif" "esp_partition" "esp_https_ota" "bt" "esp_common" "app_update"	"sqlite3" "esp_psram" "lwip"		
                               esp_event esp_wifi freertos nvs_flash coreMQTT azure-sdk-for-c azure-iot-middleware-freertos sample-azure-iot driver esp_gdbstub
				    EMBED_FILES "upload_script1.html" "products.json" 
                     "upload_script2.html"
                      "upload_script.css"
                      "script1.js"
                      "script.js"
                       "server.js"
                       "config.json"
                       "example.jpg"
					PRIV_REQUIRES "JFS"
                    EMBED_TXTFILES server_root_cert.pem local_server_cert.pem esp_logo.png)
                    
                    component_compile_definitions(SET_TRUSTED_CERT_IN_SAMPLES)	

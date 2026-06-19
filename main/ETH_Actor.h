  /*
 *  ETH_Actor.h
 *  Created on: Sep 08, 2022
 *  Author: Suraj
 */

#ifndef MAIN_ETH_ACTOR_H_
#define MAIN_ETH_ACTOR_H_

#include "esp_netif.h"

#define PRINT_LN printf("\r\n");

#define CONFIG_USE_SPI_ETHERNET

#define CONFIG_USE_W5500 				y
#define CONFIG_ETH_SPI_CLOCK_MHZ		25//36//12
#define CONFIG_ETH_SPI_PHY_ADDR0		1
#define CONFIG_SPI_ETHERNETS_NUM 		1

//#if defined(B480)
#define CONFIG_ETH_SPI_CS0_GPIO			GPIO_NUM_10
#define CONFIG_ETH_SPI_SCLK_GPIO		GPIO_NUM_12//18
#define CONFIG_ETH_SPI_MISO_GPIO		GPIO_NUM_13//19
#define CONFIG_ETH_SPI_MOSI_GPIO		GPIO_NUM_11//23
#define CONFIG_ETH_SPI_INT0_GPIO		GPIO_NUM_14//39
#define CONFIG_ETH_SPI_PHY_RST0_GPIO	-1//9//-1
#define CONFIG_ETH_SPI_HOST				SPI3_HOST//2//2

extern bool esp_event_loop_created;

void ETH_ConsoleWriteToActor_xface(void *msg); // An interface function for the console to send msg to the actor
esp_netif_t *get_ethernet_netif_handle(void);
/** Last IPv4 from IP_EVENT_ETH_GOT_IP (s_Para.IP); use when netif lookup is not ready. */
const char *get_ethernet_device_ip(void);
/** Ethernet gateway MAC resolved via ARP on IP_EVENT_ETH_GOT_IP (s_Para.ETH_GATEWAY_MAC_ADDR_a8). */
const char *get_ethernet_gateway_mac(void);

#endif /* MAIN_ETH_ACTOR_H_ */

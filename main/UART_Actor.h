/*
 * UART_ACtor.h
 *
 *  Created on: 19-Jul-2022
 *      Author: Ashwini
 */


#ifndef _UART_ACTOR_H_
#define _UART_ACTOR_H_

// if we make this static, got this error (commands.c:206: undefined reference to `UartArbiter')


//#define UART0_COM				UART_NUM_0
#define ECHO_READ_TOUT				(3)
#define UART_OBJ_QUE_COUNT 			300

#define UART0_TEST_TXD 			(CONFIG_EXAMPLE_UART_TXD)  //1
#define UART0_TEST_RXD 			(CONFIG_EXAMPLE_UART_RXD)  //3
#define UART0_TEST_RTS 			(UART_PIN_NO_CHANGE)
#define UART0_TEST_CTS 			(UART_PIN_NO_CHANGE)
#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM) //0

#if defined(B480) || defined(B553)
#define ECHO_UART_BAUD_RATE      460800// 115200//(CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE     2048
#define BUF_SIZE (120)
#else
#define ECHO_UART_BAUD_RATE      115200//(CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE     2048
#define BUF_SIZE (2048)		//(1024)
#endif
//BaseType_t uartMonitor;
//TaskHandle_t uartHandle;
//QueueHandle_t uart_Tx_queue, uart_Rx_queue;              //Uart Tx and Rx queue
//static Actor_st s_uart;                          // uart Actor structure

void uart_main(void);
int UartArbiter(int argc, char **argv, const char* source_actor);
void UART_ConsoleWriteToActor_xface(void *msg);
void Debug_Uart_Init		(void *a, void *b);
//void Uart0_init(void);

//int uart_NTP_actor_methods(int argc, char **argv) ;
//int uart_led_actor_methods(int argc, char **argv) ;
//int uart_http_actor_methods(int argc, char **argv);
//int uart_IDLE_actor_methods(int argc, char **argv);
//int uart_PB_actor_methods(int argc, char **argv);
//int uart_console_actor_methods(int argc, char **argv);
//int uart_eeprom_actor_methods(int argc, char **argv);
//int uart_RF_actor_methods(int argc, char **argv);
//int uart_coproc_actor_methods(int argc, char **argv);
//int uart_FILE_actor_methods(int argc, char **argv);
//int uart_eth_actor_methods(int argc, char **argv);
//int uart_btc_actor_methods(int argc, char **argv);
//int uart_wifi_actor_methods(int argc, char **argv);
//int uart_file_server_actor_methods(int argc, char **argv);
//int uart_smtp_actor_methods(int argc, char **argv);
//int uart_SD_actor_methods(int argc, char **argv);
//int uart_FLASH_actor_methods(int argc, char **argv);
//int uart_iHub_actor_methods(int argc, char **argv);



#endif /* MAIN_UART_ACTOR_H_ */


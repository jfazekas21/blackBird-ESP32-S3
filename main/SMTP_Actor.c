/*
 * SMTP_Actor.c
 *
 *  Created on: 23-Sep-2022
 *      Author: ACER
 */

#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "SMTP_Actor.h"
#include "FS_Int.h"
#include <regex.h>
#include "FS.h"

#define SMTP_MAX_FILENAME_LENGTH 255

static int isValidFileName(const char *fileName);
static int isValidFilePath(const char *filePath);
static int write_and_get_response(mbedtls_net_context *sock_fd, unsigned char *buf, size_t len);
static int write_ssl_and_get_response(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len);
static int write_ssl_data(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len);
static int perform_tls_handshake(mbedtls_ssl_context *ssl);

BaseType_t SMTP_Monitor, SendMail_Method, Send_File;
QueueHandle_t smtp_Rx_queue, Send_Mail_Que = NULL;  //smtp_Tx_queue,
static StaticTask_t xSMTPTaskBuffer,xSendMailReqTaskBuffer,xSendFileReqTaskBuffer;  //// Declare a static task control block
PSRAM_ATTR_BSS static StackType_t SendmailTaskStack[SMTP_SENDMAIL_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t SendFileTaskStack [SMTP_SENDFILE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xTaskStack[SMTP_TASK_STACK_DEPTH];
TaskHandle_t SMTP_Handle = NULL, SendHandle = NULL, MailHandle = NULL;
static int FirstSMTPEntry = 0;
PSRAM_ATTR char readBuff[MAX_BUFF_SIZE];
int First_MW_Entry = 0;
PSRAM_ATTR char file[128];
static uint8_t 	System_DeviceId[32];
static int NET_status = -1;
int ret, len;
size_t base64_len;
QueueHandle_t addMailReqToQueue = NULL;
static void Analyse_Response(AMessage_st* s_Message_Rx);
static AMessage_st s_Message_Tx;	// 		s_Message_Rx,// ACtors Message structure
static void sendmail(void *pvParameters __attribute__((unused)));
static void Send_File_Task(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static StaticQueue_t addMailReqToQueueBuffer,smtp_Rx_queueBuffer,Send_Mail_QueBuffer;
uint32_t filesize_rem=0;
#define FILE_BYTE       1023

bool send_attachment=0;
bool smtp_Response=0, filedata_free = 0;
int32_t size = 0;

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_x509_crt cacert;
mbedtls_ssl_config conf;
mbedtls_net_context server_fd;
mbedtls_ssl_context ssl;

#define BUF_SIZE            5*1024 //1026 //10240
static const char * THIS_ACTOR 	= "SMTP_CLIENT";
static const char 			THIS_ACTOR_ID 	= 	SMTP_CLIENT;  // assign src id
/* Constants that are configurable in menuconfig */
#define MAIL_SERVER  		"smtp.googlemail.com"//CONFIG_SMTP_SERVER
#define MAIL_PORT           "587"//CONFIG_SMTP_PORT_NUMBER
#define SENDER_MAIL         "dev.saiagrotel@gmail.com" //"t.esp.com@gmail.com"
#define SENDER_PASSWORD     "mpwhujipbisqgjzh" //"ynmkndnfbebnngoz"
#define RECIPIENT_MAIL      "anita@saiagrotel.in"//CONFIG_SMTP_RECIPIENT_MAIL
#define RECIPIENT_MAIL2 	"ashwinimortale19@gmail.com"
#define SERVER_USES_STARTSSL               1
//#define SMTP_OBJ_QUE_COUNT                	2
static void monitor(void *pvParameters __attribute__((unused)));
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					//	Change a parameter
static void get(char *prop, char *val);							//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void init(void *a, void *b);
extern char FirstTime;
PSRAM_ATTR_BSS static struct smtp{
	char mail_server[32];
	char mail_port[32];
	char sender_mail[32];
	char sender_pass[32];
	char recp_mail[32];
	char cc_mail[40];
	char subject[50];
	char Mail_body[256];
	char file_name[50];
	int file_size;
}smtp_para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &smtp_para.mail_port,     "MAIL_PORT",        STRING, "RW", "Port number of the mail server" },
    { &smtp_para.mail_server,   "MAIL_SERVER",      STRING, "RW", "Address of the mail server" },
    { &smtp_para.sender_mail,   "SENDER_MAIL",      STRING, "RW", "Sender's email address" },
    { &smtp_para.sender_pass,   "SENDER_PASS",      STRING, "RW", "Sender's email password" },
    { &smtp_para.recp_mail,     "RECP_MAIL",        STRING, "RW", "Recipient's email address" },
    { &smtp_para.cc_mail,       "CC_MAIL",          STRING, "RW", "Carbon copy email address" },
    { &smtp_para.subject,       "SUBJECT",          STRING, "RW", "Subject of the email" },
    { &smtp_para.Mail_body,     "MAIL_BODY",        STRING, "RW", "Body of the email" },
    { &smtp_para.file_name,     "ATTACH_FILE_NAME", STRING, "R", "Name of the file to be attached" },
    { &smtp_para.file_size,     "FILE_SIZE",        INT,    "RW", "Size of the attached file" },
};
PSRAM_ATTR_BSS static char payload [1024];
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Send_Mail[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Send_File[512];
PSRAM_ATTR_BSS static char File_buffer[BUF_SIZE];
PSRAM_ATTR_BSS static char filedata[BUF_SIZE];
PSRAM_ATTR_BSS static uint8_t addMailReqToQueueStorage [ADDMAILREQTOQUEUE_LENGTH * 500],smtp_Rx_queueStorage [SMTP_OBJ_QUE_COUNT * sizeof(AMessage_st)],Send_Mail_QueStorage[SMTP_MAIL_QUE_COUNT * SMTP_MAIL_QUE_COUNT_ITEMSIZE];
PSRAM_ATTR_BSS static char Send_Mail_data_buffer[4096];
PSRAM_ATTR_BSS static char Send_File_data_buffer[4096];
PSRAM_ATTR_BSS static char buf[BUF_SIZE];

#define VALIDATE_MBEDTLS_RETURN(ret, min_valid_ret, max_valid_ret, goto_label) do { \
    if ((ret) < 0) {               /* true mbedTLS/PSA error */                    \
        goto goto_label;                                                              \
    }                                                                                 \
    if (!((ret) >= (min_valid_ret) && (ret) <= (max_valid_ret))) {                    \
        /* unexpected SMTP status code */                                             \
        goto goto_label;                                                              \
    }                                                                                 \
} while (0)

/**
 * Root cert for s_SMTP.googlemail.com, taken from server_root_cert.pem
 *
 * The PEM file was extracted from the output of this command:
 * openssl s_client -showcerts -connect s_SMTP.googlemail.com:587 -starttls smtp
 *
 * The CA root cert is the last cert given in the chain of certs.
 *
 * To embed it in the app binary, the PEM file is named
 * in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */

extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

extern const uint8_t esp_logo_png_start[] asm("_binary_esp_logo_png_start");
extern const uint8_t esp_logo_png_end[]   asm("_binary_esp_logo_png_end");

extern const uint8_t SMTP_log_txt_start[] asm("_binary_SMTP_log_txt_start");
extern const uint8_t SMTP_log_txt_end[]   asm("_binary_SMTP_log_txt_end");

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
//	char str[100] ={0};
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(property, prop[i].str_name)) {

			if (!strcmp(prop[i].access, "RW")) {

			parameter_found = 1; // Set flag to indicate actor is found
			switch (prop[i].type) {

			case U_INT8:
				*(uint8_t*) prop[i].name = atoi(value);
				break;

			case U_INT16:
				*(uint16_t*) prop[i].name = atoi(value);
				break;

			case U_INT32:
				*(uint32_t*) prop[i].name = atoi(value);
				break;

			case INT:
				*(int*) prop[i].name = atoi(value);
				break;
			case FLOAT:
				*(float*) prop[i].name = atof(value);
				break;

			case STRING:
				strcpy((char*) prop[i].name, value);
				break;

			default:
				break;
			}
		}
		else
		{
			return 2;
		}
	}
	}
	if(parameter_found)
		return 1;
	else
		return 0;
}//	set

static void get(char *str_prop, char *val_a8) {
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(str_prop, prop[i].str_name)) {
			switch (prop[i].type) {

			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case INT:
				sprintf(val_a8, "%d", *(int*) prop[i].name);
				break;

			case FLOAT:
				sprintf(val_a8, "%f", *(float*) prop[i].name);
				break;

			case STRING:
				strcpy(val_a8, prop[i].name);
				break;

			default:
				break;
			}
		}
	}
}//	get


void init(void *a, void *b){
	if (FirstSMTPEntry == 0)
	{
		strcpy(smtp_para.mail_port , (const char*)MAIL_PORT);
		strcpy(smtp_para.mail_server , (const char*)MAIL_SERVER);
		strcpy(smtp_para.sender_pass , (const char*)SENDER_PASSWORD);
		strcpy(smtp_para.recp_mail , (const char*)RECIPIENT_MAIL);
		strcpy(smtp_para.sender_mail , (const char*)SENDER_MAIL);
		strcpy(smtp_para.cc_mail , (const char*)RECIPIENT_MAIL2);
		strcpy(smtp_para.subject,"SMTP services using JFS");
		smtp_para.file_size = -1;
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor
		addMailReqToQueue = xQueueCreateStatic(ADDMAILREQTOQUEUE_LENGTH, 500, addMailReqToQueueStorage, &addMailReqToQueueBuffer);
		smtp_Rx_queue = xQueueCreateStatic(SMTP_OBJ_QUE_COUNT, sizeof(AMessage_st), smtp_Rx_queueStorage, &smtp_Rx_queueBuffer);
		SMTP_Handle = xTaskCreateStaticPinnedToCore(
						monitor,                 // Task function
						"SMTP Monitor",            // Task name
						SMTP_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						SMTP_TASK_PRIORITY,                       // Task priority
						xTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xSMTPTaskBuffer,             // Pointer to task control block
						1
		);

			if (SMTP_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				    printf("Failed to create task\n");
#endif
				    // Handle error
				}
		FirstSMTPEntry = 1;
	}
}
void SMTP_CLIENT_ConsolWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
		s_Message = (AMessage_st*)msg;
		if (FirstSMTPEntry == 0)
	    {
			init(0,0);
		}

	uint8_t state = xQueueSend(smtp_Rx_queue, (AMessage_st*)msg, QUE_DELAY);//100, 5

	if (state != pdTRUE)
		{
			if(s_Message->payload_p8 != NULL)
			{
				free(s_Message->payload_p8);
				s_Message->payload_p8 = NULL;
			}
			if (state == errQUEUE_FULL)
			{
				printf("<SMTP.ERROR(SMTP RX Queue is full)\n");
			}
			else
			{

				printf("<SMTP.ERROR(SMTP RX Queue send unsuccessful) \n");
			}
		}
}//	JSF_ConsolWriteToActor

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
//	cJSON *file_data_value = NULL;
	char temp[5] = {0}, keyValue[30]={0};
	char str[200] ={0};
	
	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	  if(s_Message_Rx->payload_p8 != NULL)     // response from console (other actors)
	   {
		  	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		  	if (in_JSON == NULL) {
		  		printf("\n SMTP s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				  //  return 1;
				return;
			}
		  	else
			 {
					// Obtain the COMMAND and RESPONSE keys
					cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (commandKey != NULL && responseKey != NULL)
					{				
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
					}
					// Free the parsed JSON
					cJSON_Delete(in_JSON);
				}

			if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
				{
					in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
					if (in_JSON == NULL) {
						sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
						  //  return 1;
						return;
					}
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (responseKey != NULL && cJSON_IsString(responseKey->child))
					{
						name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
						if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
						{
							strcpy((char*)temp,name_JSON->valuestring);
							NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
						}
					}
					cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
					if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
					{
						cJSON *responseKey2 = cJSON_GetObjectItem(in_JSON, "RESPONSE");
						if (responseKey2 != NULL && cJSON_IsString(responseKey2->child))
						{
							cJSON *JSON_JFS_DBG = cJSON_GetObjectItemCaseSensitive(responseKey2, "JFS_DBG");
							if (cJSON_IsString(JSON_JFS_DBG) && (JSON_JFS_DBG->valuestring != NULL)) {
							}
						}
					}
					cJSON_Delete(in_JSON);
					return;
				}
			if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
			{
				in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (in_JSON == NULL) {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
					  //  return 1;
					return;
				}
				cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
				if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
				{
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (responseKey != NULL && cJSON_IsString(responseKey->child))
					{
						name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICEID");
						if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
						{
							strcpy((char*)System_DeviceId,name_JSON->valuestring);
						}
					}
				}
				cJSON_Delete(in_JSON);
				return;
			}
			if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
			{
				in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (in_JSON == NULL) {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
					  //  return 1;
					return;
				}
				cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

				if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SMTP_READ")==0))
				{
					cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (root != NULL)
					{
						name_JSON 		= cJSON_GetObjectItem(root, "FILE_SIZE");

						if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
						{
							//printf("Value: %d\n", name_JSON->valueint);
							sprintf(keyValue, "%d", name_JSON->valueint);
							set(name_JSON->string, keyValue,s_Message_Rx);
						}
//						else
//						{
//							strcpy(payLoadData,"Delete");
//							if(Send_Mail_Que != NULL)
//							{
//								xQueueSend(Send_Mail_Que, payLoadData, QUE_DELAY);
//							}
//						}
					}
			    }
//			   if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SMTP_READ")==0))
//			   {
//				   cJSON *jmsg_object = cJSON_GetObjectItem(in_JSON, "RESPONSE");
//				  // printf("\njmsg_object type= %d", jmsg_object->type);
//
//				   if ((jmsg_object != NULL)  && (cJSON_IsObject(jmsg_object)))
//				   	{
//					   file_data_value = cJSON_GetObjectItemCaseSensitive(jmsg_object, "RESP");
//						// Check if "File Data" value exists
//						if ((file_data_value != NULL) && (smtp_Response == 0))
//						{
//							memset(payLoadData,0,sizeof(payLoadData));
//							cJSON_PrintPreallocated(file_data_value, payLoadData, sizeof(payLoadData), false);
//							size  = strlen(payLoadData);
//							if(Send_Mail_Que != NULL)
//							{
//								xQueueSend(Send_Mail_Que, payLoadData, QUE_DELAY);
//							}
//						}
//						else
//						{
//							memset(payLoadData,0,sizeof(payLoadData));
//							cJSON_PrintPreallocated(jmsg_object, payLoadData, sizeof(payLoadData), false);
//							size  = strlen(payLoadData);
//							if(Send_Mail_Que != NULL)
//							{
//								xQueueSend(Send_Mail_Que, payLoadData, QUE_DELAY);
//							}
//						}
//				   	}
//			   }
				cJSON_Delete(in_JSON);
				return;
			}
	   }
}

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(smtp_Rx_queue, (void*) (s_Message_Rx),portMAX_DELAY)) {          // Uart Tx queue Monitor
//						printf("SMTP msg_Rx_Queue S = %s, D = %s, C = %s, size = %d \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8, s_Message_Rx->payload_size);
//						if(s_Message_Rx->payload_p8 != NULL)
//						{
//							if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//								printf("DT = %s\n",s_Message_Rx->payload_p8);
//						}
			memcpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
				if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT")){                 // Init

					if (FirstSMTPEntry == 0) {
						init(0,0);
						if(FirstSMTPEntry)
						{
							Add_Response_msg("SMTP initialization is done.", s_Message_Rx, payLoadData);
						}
					}
				}else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET"))
				{            // Get Actor Properties
					Get_Property(s_Message_Rx);
				}else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
				{
					u8Result =0;
					name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
					if (name_JSON == NULL) {
						sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
					    }
					else{
						head_JSON = name_JSON->child;
					   // Loop through each key-value pair
						do {
							// Check if the value string is not NULL
							if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0)){
								// Set the key-value pair
								u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
								if(u8Result==2){
									sprintf(str,"'%s' is a read only property", head_JSON->string);
									 Add_Response_msg(str, s_Message_Rx, payLoadData);
								}
							} else {
								// Handle the case where value string is NULL (e.g., log an error or take appropriate action)
								sprintf(str, "Invalid parameter '%s'", head_JSON->string);
								Add_Response_msg(str,s_Message_Rx, payLoadData);
								// Handle the error as per your application's requirements
							}
							head_JSON = head_JSON->next;
						} while (head_JSON != 0);

						if(u8Result==1){
						console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
						}
						// Free the parsed JSON
						cJSON_Delete(name_JSON);
					}
			}
//				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//				{
//					uint8_t *val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//					getAll((char*)prop, (char*) val_p8,s_Message_Rx);
//					free(val_p8);
//				}
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SENDMAIL"))
				{
					 {
						  // Send the payload to the queue
						if(addMailReqToQueue!=NULL)
						{
							if (xQueueSend(addMailReqToQueue, (char*)s_Message_Rx->payload_p8, portMAX_DELAY) != pdPASS) {
							Add_Response_msg("Failed to queue SENDMAIL payload.", s_Message_Rx, payLoadData);
						}
						else
						{
							cJSON *my_JSON1  	= cJSON_CreateArray();
							if (my_JSON1 == NULL) {
							        continue;
							    }
							cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("NET_STATUS"));
							cJSON *jsonObject = cJSON_CreateObject();
							cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
							cJSON_Delete(jsonObject);
							Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE",payLoadData,strlen(payLoadData),"GET");

							my_JSON1  	= cJSON_CreateArray();
							if (my_JSON1 == NULL) {
							        continue;
							    }
							cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
							jsonObject = cJSON_CreateObject();
							cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
							cJSON_Delete(jsonObject);
							Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM",payLoadData,strlen(payLoadData),"GET");
							if(SendHandle == NULL)
							{
								AMessage_st s_Message_Rx_data;
								memset(Send_Mail_data_buffer,0,sizeof(Send_Mail_data_buffer));
								if((char*)s_Message_Rx->payload_p8 != NULL)
								{
									strncpy(Send_Mail_data_buffer, (char*)s_Message_Rx->payload_p8,4095);
								}
								s_Message_Rx_data.payload_p8 = (uint8_t*)Send_Mail_data_buffer;
								strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*)s_Message_Rx->dest_Actor_a8);
								strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*)s_Message_Rx->cmdFun_a8);
								strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*)s_Message_Rx->src_Actor_a8);
								SendHandle = xTaskCreateStaticPinnedToCore(
										sendmail,                 // Task function
										"Send Mail",            // Task name
										SMTP_SENDMAIL_TASK_STACK_DEPTH,        // Stack size in words
										&s_Message_Rx_data,                    // Task parameters (not used here)
										SMTP_SENDMAIL_TASK_PRIORITY,                       // Task priority
										SendmailTaskStack,              // Pointer to task stack (allocated in PSRAM)
										&xSendMailReqTaskBuffer,             // Pointer to task control block
										1
										);
								if (SendHandle == NULL) {
					#ifdef ENABLE_PRINT_MSG
										printf("Failed to create sendmail task\n");
					#endif
										Add_Response_msg("Failed to create sendmail task", s_Message_Rx, payLoadData);
										continue;
									}
							}
							else
							{
								Add_Response_msg("Task for SENDMAIL command is already created.", s_Message_Rx, payLoadData);
							}
						}
					 }
				   }
				}
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE"))
				{
					Analyse_Response(s_Message_Rx);
			    }
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
				{
					help(s_Message_Rx);
				}
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
				 {
					get_actor_properties(s_Message_Rx);
				 }
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "MAIL_BINARY"))
				 {
					size  = s_Message_Rx->payload_size;
					if(Send_Mail_Que != NULL)
					{
						char retry = 0;
						do
						{
							uint8_t state = xQueueSend(Send_Mail_Que, s_Message_Rx->payload_p8, QUE_DELAY); //5,1
							if (state == pdTRUE)
							{
//								size  = s_Message_Rx->payload_size;
								break;
							}
							else
							{
								if (state == errQUEUE_FULL)
								{
									printf("<SMTP.ERROR(Send_Mail_Que is full. Resetting the ESP...)\n");
									vTaskDelay(1000 / portTICK_PERIOD_MS);
								}
								else
								{
									printf("<SMTP.ERROR(Send_Mail_Que send unsuccessful)\n");
									vTaskDelay(1000 / portTICK_PERIOD_MS);
								}
							}
						} while(++retry < 3);
					}
				 }
				else
				{
					 Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
				}
		}
	}
}



static int write_and_get_response(mbedtls_net_context *sock_fd, unsigned char *buf, size_t len)
{
    int ret;
    const size_t DATA_SIZE = 128;
    unsigned char data[DATA_SIZE];
    char code[4];
    size_t i, idx = 0;

    if (len && (ret = mbedtls_net_send(sock_fd, buf, len)) <= 0) {
       // ESP_LOGE(ACTOR_TAG, "mbedtls_net_send failed with error -0x%x", -ret);
        return ret;
    }

    do {
        len = DATA_SIZE - 1;
        ret = mbedtls_net_recv(sock_fd, data, len);

        if (ret <= 0) {
            //ESP_LOGE(ACTOR_TAG, "mbedtls_net_recv failed with error -0x%x", -ret);
            goto exit;
        }

        data[len] = '\0';
        len = ret;
        for (i = 0; i < len; i++) {
            if (data[i] != '\n') {
                if (idx < 4) {
                    code[idx++] = data[i];
                }
                continue;
            }

            if (idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ') {
                code[3] = '\0';
                ret = atoi(code);
                goto exit;
            }

            idx = 0;
        }
    } while (1);

exit:
    return ret;
}

static int write_ssl_and_get_response(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    int ret;
    const size_t DATA_SIZE = 128;
    unsigned char data[DATA_SIZE];
    char code[4];
    size_t i, idx = 0;

    while (len && (ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            //ESP_LOGE(ACTOR_TAG, "mbedtls_ssl_write failed with error -0x%x", -ret);
            goto exit;
        }
    }

    do {
        len = DATA_SIZE - 1;
        ret = mbedtls_ssl_read(ssl, data, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (ret <= 0) {
            //ESP_LOGE(ACTOR_TAG, "mbedtls_ssl_read failed with error -0x%x", -ret);
            goto exit;
        }

        len = ret;
        for (i = 0; i < len; i++) {
            if (data[i] != '\n') {
                if (idx < 4) {
                    code[idx++] = data[i];
                }
                continue;
            }

            if (idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ') {
                code[3] = '\0';
                ret = atoi(code);
                goto exit;
            }
            idx = 0;
        }
    } while (1);

exit:
    return ret;
}

static int write_ssl_data(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    int ret;
    while (len && (ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
#ifdef ENABLE_PRINT_MSG
            printf( "\n 1 mbedtls_ssl_write failed with error -0x%x \n ", -ret);
#endif
            return ret;
        }
    }
    return 0;
}

static int perform_tls_handshake(mbedtls_ssl_context *ssl)
{
    int ret = -1;
    uint32_t flags;
    char *buf = NULL;
    buf = (char *) heap_caps_calloc(1, BUF_SIZE,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        //ESP_LOGE(ACTOR_TAG, "heap_caps_calloc failed for size %d", BUF_SIZE);
    	printf("Memory allocation failed\n");
        goto exit;
    }
    fflush(stdout);
    while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            //ESP_LOGE(ACTOR_TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
            goto exit;
        }
    }
   if ((flags = mbedtls_ssl_get_verify_result(ssl)) != 0) {
        /* In real life, we probably want to close connection if ret != 0 */
        //ESP_LOGW(ACTOR_TAG, "Failed to verify peer certificate!");
        mbedtls_x509_crt_verify_info(buf, BUF_SIZE, "  ! ", flags);
        //ESP_LOGW(ACTOR_TAG, "verification info: %s", buf);
    }
    ret = 0; /* No error */
exit:
    if (buf) {
        free(buf);
    }
    return ret;
}

// encode the file name
void url_encode(const char *input, char *output, size_t output_size) {
    size_t output_index = 0;

    for (size_t i = 0; i < strlen(input); i++) {
        char current_char = input[i];

        // Encode special characters
        if ((current_char >= 'a' && current_char <= 'z') ||
            (current_char >= 'A' && current_char <= 'Z') ||
            (current_char >= '0' && current_char <= '9') ||
            current_char == '-' || current_char == '_' || current_char == '.' || current_char == '~') {
            if (output_index + 1 < output_size) {
                output[output_index++] = current_char;
            }
        } else {
            // Encode non-alphanumeric characters
            if (output_index + 3 < output_size) {
                output[output_index++] = '/';
                output[output_index++] = '/';  //hex_chars[(current_char >> 4) & 0xF];
                //output[output_index++] = hex_chars[current_char & 0xF];
            }
        }

        // Ensure null-terminated string
        if (output_index < output_size) {
            output[output_index] = '\0';
        }
    }
}

static void sendmail(void *pvParameters __attribute__((unused)))
{
	AMessage_st *s_Message_Rx = (AMessage_st*)pvParameters;
	char MAX_retry_Cnt = 5, Connect_retry = 0;
    unsigned char base64_buffer[128], task_done = 0;
    char str[1026], str1[100];
    cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	while(1)
	{
		memset(payload, 0 ,sizeof(payload));
		if (xQueueReceive(addMailReqToQueue, (void*) payload, portMAX_DELAY) == pdPASS) {
    	send_attachment=0;
    	task_done = 0;
       in_JSON = cJSON_Parse((char*) payload);
       if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData_Send_Mail);
			  //  return 1;
			goto exit;
		}
       else{
       name_JSON = cJSON_GetObjectItem(in_JSON, "mail");
       if(name_JSON!=0)
       {
    	   strcpy(smtp_para.recp_mail, name_JSON->valuestring);
       }
       else
       {
    	   strcpy(smtp_para.recp_mail,"");
       }
       name_JSON = cJSON_GetObjectItem(in_JSON, "CC");
       if(name_JSON!=0)
       {
    	   strcpy(smtp_para.cc_mail, name_JSON->valuestring);
       }
       else
       {
    	   strcpy(smtp_para.cc_mail,"");

       }
       name_JSON = cJSON_GetObjectItem(in_JSON, "BODY_TEXT");
        if(name_JSON!=0)
        {
        	strcpy(smtp_para.Mail_body, name_JSON->valuestring);
        }
        else
        {
        	 strcpy(smtp_para.Mail_body,"");
        }
        name_JSON = cJSON_GetObjectItem(in_JSON, "SUBJECT_TEXT");
        if(name_JSON!=0)
        {
         strcpy(smtp_para.subject, name_JSON->valuestring);
        }
        else
        {
        	 strcpy(smtp_para.subject,"");
        }

      //check for attachment

        cJSON *attachment = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
        if (attachment) {
                if (isValidFileName(attachment->valuestring) &&
                		isValidFilePath(attachment->valuestring)) {
                    strcpy(smtp_para.file_name, attachment->valuestring);
                    send_attachment = 1;
                } else {
                    send_attachment = 0; // Invalid file name or path
                    Add_Response_msg("Received Invalid File name/path Sending mail without attachment",s_Message_Rx, payLoadData_Send_Mail);
                    Add_Response_msg("Kindly enter the correct filepath & filename. Use drive 'A:/' for JFS and 'B:/' for SD card.",s_Message_Rx, payLoadData_Send_Mail);
                }
            } else {
                send_attachment = 0; // No attachment
            }
            cJSON_Delete(in_JSON);
       }
	   Connect_retry = 0;
	   do
	   {
			mbedtls_ssl_init(&ssl);
			mbedtls_x509_crt_init(&cacert);
			mbedtls_ctr_drbg_init(&ctr_drbg);
			mbedtls_ssl_config_init(&conf);
			mbedtls_entropy_init(&entropy);
			if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
											 NULL, 0)) != 0) {
				sprintf(str,"Error! mbedtls_ctr_drbg_seed returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}
			ret = mbedtls_x509_crt_parse(&cacert, server_root_cert_pem_start,
										 server_root_cert_pem_end - server_root_cert_pem_start);

			if (ret < 0) {
				sprintf(str,"Error! mbedtls_x509_crt_parse returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}
			/* Hostname set here should match CN in server certificate */
			if ((ret = mbedtls_ssl_set_hostname(&ssl, smtp_para.mail_server)) != 0) {
				sprintf(str,"Error! mbedtls_ssl_set_hostname returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}
			if ((ret = mbedtls_ssl_config_defaults(&conf,
												   MBEDTLS_SSL_IS_CLIENT,
												   MBEDTLS_SSL_TRANSPORT_STREAM,
												   MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
				sprintf(str,"Error! mbedtls_ssl_config_defaults returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}

			mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
			mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
			mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
		#ifdef CONFIG_MBEDTLS_DEBUG
			mbedtls_esp_enable_debug_log(&conf, 4);
		#endif

			if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
				sprintf(str,"Error! mbedtls_ssl_setup returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}

			mbedtls_net_init(&server_fd);
			if ((ret = mbedtls_net_connect(&server_fd, smtp_para.mail_server,
					smtp_para.mail_port, MBEDTLS_NET_PROTO_TCP)) != 0) {
				sprintf(str,"Error! mbedtls_net_connect returned -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto retry;
			}

		retry:
				if(ret == 0)
				{
					break;
				}
				else
				{
					mbedtls_net_free(&server_fd);
					mbedtls_x509_crt_free(&cacert);
					mbedtls_ssl_free(&ssl);
					mbedtls_ssl_config_free(&conf);
					mbedtls_ctr_drbg_free(&ctr_drbg);
					mbedtls_entropy_free(&entropy);
					Connect_retry++;
					vTaskDelay(2000/portTICK_PERIOD_MS);
				}
		}while(Connect_retry < MAX_retry_Cnt);
		if((Connect_retry >= MAX_retry_Cnt) && (ret != 0))
		{
			sprintf(str,"mbedtls_net_connect returned -0x%x",-ret);
			Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
			goto exit;
		}
		Add_Response_msg("Connected", s_Message_Rx, payLoadData_Send_Mail);
		mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
		memset(buf,0,sizeof(buf));

		#if SERVER_USES_STARTSSL
			/* Get response */
			ret = write_and_get_response(&server_fd, (unsigned char *) buf, 0);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
			len = snprintf((char *) buf, BUF_SIZE, "EHLO %s\r\n", "ESP32");
			ret = write_and_get_response(&server_fd, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
			len = snprintf((char *) buf, BUF_SIZE, "STARTTLS\r\n");
			ret = write_and_get_response(&server_fd, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
			ret = perform_tls_handshake(&ssl);
			if (ret != 0) {
				goto exit;
			}

			#else /* SERVER_USES_STARTSSL */
				ret = perform_tls_handshake(&ssl);
				if (ret != 0) {
					goto exit;
				}

				/* Get response */
				ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, 0);
				VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Writing EHLO to server...");
				//ESP_LOGI(aTag, "Writing EHLO to server...");

				len = snprintf((char *) buf, BUF_SIZE, "EHLO %s\r\n", "ESP32");
				ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
				VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

			#endif /* SERVER_USES_STARTSSL */

			/* Authentication */
			Add_Response_msg("Write AUTH LOGIN", s_Message_Rx, payLoadData_Send_Mail);
			len = snprintf( (char *) buf, BUF_SIZE, "AUTH LOGIN\r\n" );
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit);
			ret = mbedtls_base64_encode((unsigned char *) base64_buffer, sizeof(base64_buffer),
										&base64_len, (unsigned char *) smtp_para.sender_mail, strlen(smtp_para.sender_mail));
			if (ret != 0) {
				sprintf(str,"Error in mbedtls encode! ret = -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto exit;
			}
			len = snprintf((char *) buf, BUF_SIZE, "%s\r\n", base64_buffer);
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 300, 399, exit);

			//ESP_LOGI(ACTOR_TAG, "Write PASSWORD");
			ret = mbedtls_base64_encode((unsigned char *) base64_buffer, sizeof(base64_buffer),
				&base64_len, (unsigned char *) smtp_para.sender_pass, strlen(smtp_para.sender_pass));
			if (ret != 0) {
				sprintf(str,"Error in mbedtls encode! ret = -0x%x",-ret);
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
				goto exit;
			}
			len = snprintf((char *) buf, BUF_SIZE, "%s\r\n", base64_buffer);
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit);

			/* Compose email */
			len = snprintf((char *) buf, BUF_SIZE, "MAIL FROM:<%s>\r\n", smtp_para.sender_mail);
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

			len = snprintf((char *) buf, BUF_SIZE, "RCPT TO:<%s>\r\n", smtp_para.recp_mail);
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

			len = snprintf((char *) buf, BUF_SIZE, "RCPT TO:<%s>\r\n", smtp_para.cc_mail);
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			Add_Response_msg("Write DATA", s_Message_Rx, payLoadData_Send_Mail);
			len = snprintf((char *) buf, BUF_SIZE, "DATA\r\n");
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 300, 399, exit);
			/* We do not take action if message sending is partly failed. */
			  len = snprintf((char *) buf, BUF_SIZE,
				   "From: Haven Developer <%s>\r\n"
				   "Subject: %s\r\n"
				   "To: %s\r\n"
				   "Cc: %s\r\n"
				   "MIME-Version: 1.0 (mime-construct 1.9)\r\n",
				   smtp_para.sender_mail, smtp_para.subject, smtp_para.recp_mail, smtp_para.cc_mail);
			/**
			 * Note: We are not validating return for some ssl_writes.
			 * If by chance, it's failed; at worst email will be incomplete!
			 */
			ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			/* Multipart boundary */
			if(send_attachment==1)
			{
			   len = snprintf((char *) buf, BUF_SIZE,
							  "Content-Type: multipart/mixed;boundary=XYZabcd1234\n"
							  "--XYZabcd1234\n");
			   ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			   /* Text */
			   len = snprintf((char *) buf, BUF_SIZE,
							  "Content-Type: text/plain\n"
							  "%s\r\n"
							  "\r\n"
							  "\n\n--XYZabcd1234\n", smtp_para.Mail_body);
			   ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			}
			else
			{
				 len = snprintf((char *) buf, BUF_SIZE,
									  "Content-Type: text/plain\n"
									  "%s\r\n"
									  "\r\n"
									 , smtp_para.Mail_body);
			   ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			}

		   if(send_attachment==1)
		   {
			   char fname[60] = {0};
			   strcpy(fname,smtp_para.file_name );
			   char new_file_name[200] = {0};
			   // Extract the base filename and extension
			   char *base_file_name = strrchr(fname, '/');
			   if (base_file_name != NULL) {
				   base_file_name++;  // Skip the '/'
			   } else {
				   base_file_name = fname;  // No '/' found, use the entire name
			   }
			   char *dot1 = strrchr(base_file_name, '.');
			   char base[60] = {0};
			   char extension[10] = {0};

			   if (dot1 != NULL) {
				   strncpy(base, base_file_name, dot1 - base_file_name);
				   strcpy(extension, dot1);  // Include the dot in the extension
			   } else {
				   strcpy(base, base_file_name);  // No extension found
			   }
			   // Construct the new filename
			   snprintf(new_file_name, sizeof(new_file_name), "%s_%s%s", base, System_DeviceId, extension);
			   const char *dot = strrchr((char*)new_file_name, '.');
				// Check if the dot is present and if it is followed by "bin" or "db"
				if (dot != NULL && ((strcmp(dot + 1, "bin") == 0) || (strcmp(dot + 1, "db") == 0)))
				{
				   len = snprintf((char *) buf, BUF_SIZE,
						" Content-Type: application/octet-stream;name=%s \r\n"  //application/octet-stream
						"Content-Transfer-Encoding: base64\n"
					   "Content-Disposition: attachment;filename=%s \r\n\n",new_file_name, new_file_name);
				}
				else
				{
				   len = snprintf((char *) buf, BUF_SIZE,
						" Content-Type: application/octet-stream;name=%s \r\n"  //application/octet-stream
					   "Content-Disposition: attachment;filename=%s \r\n\n",new_file_name, new_file_name);
				}
				ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
		   }
		   if(send_attachment==1)
			{
				if(Send_Mail_Que == NULL)
				{
					Send_Mail_Que = xQueueCreateStatic(SMTP_MAIL_QUE_COUNT, SMTP_MAIL_QUE_COUNT_ITEMSIZE, Send_Mail_QueStorage, &Send_Mail_QueBuffer);
				}
				if (Send_Mail_Que == NULL)
					Add_Response_msg("ERROR! Send_Mail_Que is not created",s_Message_Rx, payLoadData_Send_Mail);

				if(MailHandle == NULL)
				{
					xQueueReset(Send_Mail_Que);  // reset the previous data of que
					size = 0;
					AMessage_st s_Message_Rx_data;
					memset(Send_File_data_buffer,0,sizeof(Send_File_data_buffer));
					if((char*)s_Message_Rx->payload_p8 != NULL)
					{
						strncpy(Send_File_data_buffer, (char*)s_Message_Rx->payload_p8,4095);
					}
					s_Message_Rx_data.payload_p8 = (uint8_t*)Send_File_data_buffer;
					strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*)s_Message_Rx->dest_Actor_a8);
					strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*)s_Message_Rx->cmdFun_a8);
					strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*)s_Message_Rx->src_Actor_a8);
					MailHandle = xTaskCreateStaticPinnedToCore(
							Send_File_Task,                 // Task function
							"Send attachment",            // Task name
							SMTP_SENDFILE_TASK_STACK_DEPTH,        // Stack size in words
							&s_Message_Rx_data,                    // Task parameters (not used here)
							SMTP_SENDFILE_TASK_PRIORITY,                       // Task priority
							SendFileTaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xSendFileReqTaskBuffer,             // Pointer to task control block
							1
							);
//									Send_File = xTaskCreate(Send_File_Task, (const char*) "Send attachment", SMTP_SENDFILE_TASK_STACK_DEPTH, s_Message_Rx, SMTP_SENDFILE_TASK_PRIORITY, &MailHandle);
					if (MailHandle == NULL) {
		#ifdef ENABLE_PRINT_MSG
							printf("Failed to create task\n");
		#endif
							Add_Response_msg("ERROR! Send_File_Task is not created",s_Message_Rx, payLoadData_Send_Mail);
							goto exit;
						}
				}
				else
				 {
					Add_Response_msg("Task for send mail command is already created.", s_Message_Rx, payLoadData_Send_Mail);
				 }
				do{
					vTaskDelay(600 / portTICK_PERIOD_MS);
				}while(MailHandle!=NULL);
				UBaseType_t itemsCount = uxQueueMessagesWaiting(addMailReqToQueue);
				if (itemsCount == 0) {
					if(SendHandle!=NULL)
					{
						xQueueReset(addMailReqToQueue);
						SendHandle = NULL;
						vTaskDelete(NULL);
					}
				}
				else
				{
					continue;
				}
		}
		if(send_attachment==0)
		{
		len = snprintf((char *) buf, BUF_SIZE, "\r\n.\r\n");
		ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
		}
	   VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit);
	   Add_Response_msg("Email sent!", s_Message_Rx, payLoadData_Send_Mail);

		/* Close connection */
		mbedtls_ssl_close_notify(&ssl);
		ret = 0; /* No errors */

exit:
		mbedtls_net_free(&server_fd);
		mbedtls_x509_crt_free(&cacert);
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
			if (ret != 0)
			{
				if (ret < 0) {
					// true TLS/mbedTLS error
					mbedtls_strerror(ret, str1, sizeof(str1));
					sprintf(str, "TLS error: ret=%#x (%s)", ret, str1);
				} else {
					// SMTP status code
					sprintf(str, "SMTP error: %d", ret);
				}
				Add_Response_msg(str, s_Message_Rx, payLoadData_Send_Mail);
			}
			putchar('\n'); /* Just a new line */
			task_done = 1;
			if(task_done == 1)
			{
				UBaseType_t itemsCount = uxQueueMessagesWaiting(addMailReqToQueue);
				if (itemsCount == 0) {
					if(SendHandle!=NULL)
					{
						xQueueReset(addMailReqToQueue);
						SendHandle = NULL;
						vTaskDelete(NULL);
					}
				}
				else
				{
					continue;
				}
			}
		}  // end of que receive
	} // end of outer while (1)
}

static void Get_Property(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	char val_p8[256] = {0};
	int Array_size = 0;

	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	out_JSON 	= cJSON_CreateObject();
	head_JSON = cJSON_GetObjectItem(in_JSON, "Property_Names");
	Array_size = cJSON_GetArraySize(head_JSON);
	if(Array_size > 0)
	{
		for(int i=0; i<Array_size; i++)
		{
			cJSON *element = cJSON_GetArrayItem(head_JSON, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL))
			{
				if(strlen(element->valuestring) == 0)
					continue;
				memset(val_p8, 0, sizeof (val_p8));
				get(element->valuestring, val_p8);
				cJSON_AddStringToObject(out_JSON, element->valuestring, (char*) val_p8);
			}
		}
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
	{
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx, payLoadData);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

static void Extract_folders(const char *path) {
    // Make a copy of the path to tokenize it (strtok modifies the string)
    char path_copy[100] = {0}, Dir_name[100] ={0};
    strcpy(Dir_name,"Root");
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0'; // Ensure null termination

    // Tokenize the string by '/'
    char *token = strtok(path_copy, "/");

    // Iterate through all tokens except the last one (file name)
    while (token != NULL) {
    	strcat(Dir_name, "/");
        // Find the next token
        char *next_token = strtok(NULL, "/");

        // If next_token is NULL, we are at the last token (file name)
        if (next_token == NULL) {
            break;
        }

        // Print the folder name
        strcat(Dir_name, token);
        int err = FS_CreateDir(Dir_name);
		if (err < 0) {
//			#ifdef ENABLE_PRINT_MSG
			printf("Error! Create Dir- %s [%d]:[%s].\n", Dir_name, err,FS_ErrorNo2Text(err));
//			#endif
		}
//		else
//		{
//			printf("Created Dir [%s].\n", Dir_name);
//		}
        // Move to the next token
        token = next_token;
    }
}

static void Delete_folders(const char *path) {
    // Make a copy of the path to tokenize it (strtok modifies the string)
    char path_copy[100] = {0}, Dir_name[100] ={0};
    strcpy(Dir_name,"Root");
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0'; // Ensure null termination

    // Tokenize the string by '/'
    char *token = strtok(path_copy, "/");

    // Iterate through all tokens except the last one (file name)
    while (token != NULL) {
    	strcat(Dir_name, "/");
        // Find the next token
        char *next_token = strtok(NULL, "/");

        // If next_token is NULL, we are at the last token (file name)
        if (next_token == NULL) {
            break;
        }

        // Print the folder name
        strcat(Dir_name, token);
        int err = FS_DeleteDir(Dir_name,1);
		if (err < 0) {
			#ifdef ENABLE_PRINT_MSG
			printf("Error! Create Dir [%d]:[%s].\n", err,FS_ErrorNo2Text(err));
			#endif
		}
        // Move to the next token
        token = next_token;
    }
}
static void Send_File_Task(void *pvParameters __attribute__((unused)))
{
	AMessage_st *s_Message_Rx =  (AMessage_st*)pvParameters;
	Add_Response_msg("E-mail the file is in progress. Please wait ...", s_Message_Rx, payLoadData_Send_File);
	char str[150]={0}, str1[50]={0};
	char binary_file_fg = 0; // filedata[3000] = {0}; // File_buffer[BUF_SIZE] = {0};
	FS_FILE * pFile 	= 	NULL;
	char Filename[100] = {0};
	char File_name[100] = {0};
	uint32_t Read_len 		= 0;
	char src_file[100] = {0}, dest_file[100] = {0}, dest_file_temp[100] = {0};
	int retry = 0;
	char path[100] = {0};
	int i =0, found =0;

	//Extract A:/ from file path
	strcpy(Filename, "Root/");
	strcpy((char*)path,smtp_para.file_name);
    for(i =0; i< strlen(path); i++)
	{
		if(path[i] == '/')
		{
			found = 1;
		    break;
		}
	}
    if(found == 1)
    {
		strcat(Filename, path+i+1);
		strcat(File_name, path+i+1);
    }
    else
    {
    	Add_Response_msg("Kindly enter the correct file path. Use drive 'A:/' for JFS.", s_Message_Rx, payLoadData);
    	goto exit1;
    }

//    printf("\n Filename = %s \n\n", Filename);
	FS_FAT_SupportLFN();    // support for long file name
	FS_FOpenEx(Filename, "r", &pFile);		// Open the file in read mode
	if (pFile == NULL) {
		Add_Response_msg("Error! File is not present.", s_Message_Rx, payLoadData_Send_File);
		strcpy(filedata, "Error! File is not present.");
		len = strlen(filedata);
		ret = write_ssl_data(&ssl, (unsigned char *) filedata, len);
		len = snprintf((char *) buf, BUF_SIZE, "\n--XYZabcd1234\n");
		ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
		len = snprintf((char *) buf, BUF_SIZE, "\r\n.\r\n");
		ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
	    VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit1);
	    Add_Response_msg("Email sent!", s_Message_Rx, payLoadData_Send_File);

		/* Close connection */
		mbedtls_ssl_close_notify(&ssl);
		ret = -1; /* No errors */
		if (ret != 0)
		{
			 mbedtls_strerror(ret, str1, 100);
			 sprintf(str, "Last error was: -0x%x - %s", -ret, str1);
			 Add_Response_msg(str, s_Message_Rx, payLoadData_Send_File);
		}
		goto exit1;
	}

	if((FS_GetFileSize(pFile) + JFS_BUFFER_SIZE) < FS_GetVolumeFreeSpace( "nor:0:"))  //If file size is less than free memory size, then copy the file.
	{
		// create a copy of file to be mailed
		strcat(src_file, Filename);
		strcpy(dest_file, "Root/");
		strcat(dest_file, "Temp/");
		strcat(dest_file, File_name);

		strcat(dest_file_temp, "Temp/");
		strcat(dest_file_temp, File_name);
		Extract_folders(dest_file_temp);
		//vTaskDelay(100/ portTICK_PERIOD_MS);
//		printf("\n\n src_file = %s, dest_file = %s, dest_file_temp = %s ,File_name =%s\n\n", src_file, dest_file, dest_file_temp,File_name);
		retry = 0;
		do
		{
			ret = FS_CopyFile(src_file, dest_file); //"ram:\\Dest.txt"
			if(ret == ESP_OK)
			{
//				printf("\n\n file copied \n\n");
				break;
			}
			else
			{
				retry++;
//				printf("\n error in file copied retry = %d, ret = %d\n", retry, ret);
				vTaskDelay(500 / portTICK_PERIOD_MS);
			}
		}while(retry < 3);

		if((retry >= 3) && ((ret != ESP_OK) && (ret != FS_ERRCODE_FILE_DIR_NOT_FOUND)))
		{
			sprintf(filedata, "Could not copy the file. Operation on %s file is in progress. Error is (%s). Kindly try again.", Filename, FS_ErrorNo2Text(ret));
			len = strlen(filedata);
			ret = write_ssl_data(&ssl, (unsigned char *) filedata, len);
			len = snprintf((char *) buf, BUF_SIZE, "\n--XYZabcd1234\n");
			ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			len = snprintf((char *) buf, BUF_SIZE, "\r\n.\r\n");
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
			VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit1);
			Add_Response_msg("Email sent!", s_Message_Rx, payLoadData_Send_File);

			/* Close connection */
			mbedtls_ssl_close_notify(&ssl);
			ret = -1; /* No errors */
			if (ret != 0)
			{
				 mbedtls_strerror(ret, str1, 100);
				 sprintf(str, "Last error was: -0x%x - %s", -ret, str1);
				 Add_Response_msg(str, s_Message_Rx, payLoadData_Send_File);
			}
			Delete_folders(dest_file_temp);
			FS_FClose(pFile);
			goto exit1;
		}
		FS_FClose(pFile);  // close original file
		FS_FOpenEx(dest_file, "r", &pFile);		// Open the copied file in read mode
	}
    const char *dot = strrchr((char*)smtp_para.file_name, '.');
	// Check if the dot is present and if it is followed by "bin"
	if (dot != NULL && ((strcmp(dot + 1, "bin") == 0) || (strcmp(dot + 1, "db") == 0)))
	{
	   binary_file_fg = 1;
	}

//	printf("\n binary_file_fg = %d \n",binary_file_fg);
	int total_read_cnt = 0;

	while(1)
	{
		memset(filedata,0,BUF_SIZE);
		memset(File_buffer,0,BUF_SIZE);
		Read_len = FS_FRead(filedata, 1, 2040, pFile); // Read data from JFS Flash memory
		if(Read_len != 0)
		{
			total_read_cnt = total_read_cnt + Read_len;
//			printf(" total_read_cnt = %d \n",total_read_cnt);
			ret = -1;
			memset(buf,0,BUF_SIZE);
			if(binary_file_fg == 1)
			{
				ret = 	mbedtls_base64_encode((unsigned char *) File_buffer, BUF_SIZE, &base64_len, (unsigned char *)filedata, Read_len);// strlen(filedata)
				if (ret != 0)
				{
					sprintf(str,"Error in mbedtls encode! ret = -0x%x", -ret);
					Add_Response_msg(str, s_Message_Rx, payLoadData_Send_File);
				}
			}
			else
			{
				strcpy(File_buffer,  (char*)filedata);
			}
			len = snprintf((char *) buf, BUF_SIZE, "%s", File_buffer);
			ret = write_ssl_data(&ssl, (unsigned char *) buf, len);    //len, strlen(buf)
		}
		else
		{
			FS_FClose(pFile); // close the file
			ret = FS_Remove(dest_file);
	        if((ret != 0) && (Read_len == 0))
	        {
	        	sprintf(str, "Error! Could not delete %s file", dest_file_temp);
				Add_Response_msg(str,s_Message_Rx, payLoadData_Send_File);
	        }
	        Delete_folders(dest_file_temp);
			len = snprintf((char *) buf, BUF_SIZE, "\n--XYZabcd1234\n");
			ret = write_ssl_data(&ssl, (unsigned char *) buf, len);
			len = snprintf((char *) buf, BUF_SIZE, "\r\n.\r\n");
			ret = write_ssl_and_get_response(&ssl, (unsigned char *) buf, len);
		    VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit1);
		    Add_Response_msg("Email sent!", s_Message_Rx, payLoadData_Send_File);

			/* Close connection */
			mbedtls_ssl_close_notify(&ssl);
			ret = 0; /* No errors */
			if (ret != 0)
			{
				 mbedtls_strerror(ret, str1, 100);
				 sprintf(str, "Last error was: -0x%x - %s", -ret, str1);
				 Add_Response_msg(str, s_Message_Rx, payLoadData_Send_File);
			}
			break;
		}
}  // end of while(1)
exit1:
	mbedtls_net_free(&server_fd);
//	printf("\n\n free allocations in sendfiletask\n\n");
	mbedtls_x509_crt_free(&cacert);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	Add_Response_msg("E-mail the file is completed. Deleting the send file task ...", s_Message_Rx, payLoadData_Send_File);
	xQueueReset(Send_Mail_Que);
	MailHandle = NULL;
	vTaskDelete(MailHandle);
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  = (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	strcpy((char*)newpointer, response);
	s_Message_Tx_new.payload_p8 	=  newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface(&s_Message_Tx_new);
}

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[256] = {0};
	char typeString[20] = {0};

	int no_of_elements = sizeof(prop) / sizeof(struct property);

	    // Create JSON arrays
	    cJSON *jsonArrayName = cJSON_CreateArray();
	    cJSON *jsonArrayType = cJSON_CreateArray();
	    cJSON *jsonArrayValue = cJSON_CreateArray();
	    cJSON *jsonArrayAccess = cJSON_CreateArray();
	    cJSON *jsonArrayHelpString = cJSON_CreateArray();

	    for (int i = 0; i < no_of_elements; i++) {
			cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(prop[i].str_name));
			// Convert DataType enum to string representation for property type
			// Add value based on data type for property name
			switch (prop[i].type) {
				case U_INT8:
					strcpy(typeString, "U_INT8");
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					strcpy(typeString, "U_INT16");
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					strcpy(typeString, "U_INT32");
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case INT:
					strcpy(typeString, "INT");
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case FLOAT:
					strcpy(typeString, "FLOAT");
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(typeString, "STRING");
					strcpy(val_a8, prop[i].name);
					break;

				default:
					break;
				}
			cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
			cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(val_a8));
			cJSON_AddItemToArray(jsonArrayAccess, cJSON_CreateString(prop[i].access));
			cJSON_AddItemToArray(jsonArrayHelpString, cJSON_CreateString(prop[i].HelpString));
		}
		// Create a JSON object and add the array to it
		cJSON *jsonObject = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonObject, "Name", jsonArrayName);
		cJSON_AddItemToObject(jsonObject, "Type", jsonArrayType);
		cJSON_AddItemToObject(jsonObject, "Value", jsonArrayValue);
		cJSON_AddItemToObject(jsonObject, "Access", jsonArrayAccess);
		cJSON_AddItemToObject(jsonObject, "Help String", jsonArrayHelpString);
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the SMTP_CLIENT actor.");
	cJSON_AddStringToObject(responseObject, "SET(string SENDER_MAIL)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "SENDMAIL(string MAIL,  string CC, string BODY_TEXT, string SUBJECT_TEXT, string FILE_NAME)", "Send mail  from ESP without attachment.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	helpBODY_TEXT

static int isValidFileName(const char *fileName) {
    if (fileName == NULL || strlen(fileName) == 0) {
        return 0; // Invalid if null or empty
    }
    if (strlen(fileName) > MAX_FILE_NAME_LEN) {
        return 0; // Invalid if filename is too long
    }
    return 1; // Valid file name
}

static int isValidFilePath(const char *filePath) {
    if (filePath == NULL || strlen(filePath) == 0) {
        return 0; // Invalid if null or empty
    }

    // Check for valid file path characters starting with A:/ or B:/
    regex_t regex;
    int reti;
    reti = regcomp(&regex, "^[a-bA-B]:/[a-zA-Z0-9_./:%+-]*$", REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return 0;
    }

    reti = regexec(&regex, filePath, 0, NULL, 0);
    regfree(&regex);

    if (reti) {
        return 0; // Invalid if regex doesn't match
    }

    return 1; // Valid file path
}


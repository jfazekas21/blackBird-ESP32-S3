/*
 * SQL_Actor.c
 *
 *  Created on: 31-Oct-2023
 *      Author: Priyanka
 */

#include "actor.h"
#include "Config.h"
#include "SQL_Actor.h"
#include "Console_Actor.h"
#include "math.h"
#include "FS.h"

#define MAX_RECORD_COUNT 10000
#define Database_Dir	"Root/Database/"
static const char * THIS_ACTOR = "SQL";
static const char 			THIS_ACTOR_ID 	= 	SQL;
static SemaphoreHandle_t db_mutex;
BaseType_t sqlMonitor;
TaskHandle_t sqlHandle;
QueueHandle_t sql_Rx_Queue; //sql_Tx_Queue;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [SQL_TASK_QUEUE_LENGTH * sizeof(AMessage_st)];

static StaticQueue_t Monitor_pxQueueBuffer;
static StaticTask_t xSQLTaskBuffer;
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_CallBack[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static StackType_t xTaskStack[SQL_TASK_STACK_DEPTH];
static AMessage_st s_Message_Tx;//s_Message_Rx; //, s_Message_Tx;
AMessage_st s_Message_Tx_SQL;
static int FirstEntry = 0;
static long int last_time = 0;
static uint32_t sql_cb_row_count = 0;
const char* data = "Callback function called";
static cJSON *root_DB 		= 	NULL;
typedef struct {
    uint32_t action_table_version;
    uint16_t ScriptID;
    uint16_t ActionID;
    uint64_t Target_Bitfield;
    uint8_t u8Page_no;
    uint8_t Colour_indx;
    uint8_t Brightness;
    uint8_t On_Off_type;
    uint8_t Dummy_1;
    uint8_t Dummy_2;
    uint8_t Dummy_3;
    uint8_t Dummy_4;
    uint8_t ActionType;
    uint8_t Dummy_5;
    uint8_t Dummy_6;
    uint8_t Dummy_7;
    uint8_t Dummy_8;
    uint8_t Dummy_9;
    uint8_t Dummy_10;
    uint8_t Dummy_11;
    const char *Name;
} Struct_ACTION_TABLE;

static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
void replaceSymbol(char *str, char find, char replace);
static uint8_t Parse_DB_Exe(AMessage_st* s_Message_Rx);
static int callback(void *data, int argc, char **argv, char **azColName);
static void create_db(AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Save_Database_Record(AMessage_st* s_Message_Rx);
static void Excecute_database_command(AMessage_st* s_Message_Rx);
static void Read_DB_Sync_Status(AMessage_st* s_Message_Rx);
static void Print_Database(AMessage_st* s_Message_Rx);
static int db_exec(sqlite3 *db, const char *sql, AMessage_st* s_Message_Rx);
static int db_open(const char *filename, sqlite3 **db, AMessage_st* s_Message_Rx);
static int extract_json_string_field(const char *src, const char *key, char *out, size_t out_sz);
static int extract_loose_string_field(const char *src, const char *key, char *out, size_t out_sz);
static void unescape_json_once(const char *src, char *dst, size_t dst_sz);
static int is_likely_complete_query(const char *query);
static int extract_query_until_semicolon(const char *src, char *out, size_t out_sz);

PSRAM_ATTR_BSS static struct Sql_parameter {
	uint8_t  file_name 	[SQL_FILENAME_LEN ];
	uint8_t  query 	[SQL_QUERY_LEN];
} sql_para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &sql_para.file_name,    "FILE_NAME",    STRING,  "R", "Name of the SQL file" },
    { &sql_para.query,        "QUERY",        STRING,  "R", "SQL query" },
};

sqlite3 *db1, *db2;
static char s_last_db_file_name[SQL_FILENAME_LEN] = {0};

void init_mutex() {
    db_mutex = xSemaphoreCreateMutex();
}

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
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
				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
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


static void init(void *a, void *b) {
	if (FirstEntry == 0){
	if (sql_Rx_Queue == NULL)
	{
		sql_Rx_Queue = xQueueCreateStatic(SQL_TASK_QUEUE_LENGTH, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

		if (sql_Rx_Queue == NULL) {
			printf("SQL RX Queue is not created.\n");
		}
	}
	sqlHandle = xTaskCreateStatic(
			monitor,                 // Task function
			"SQL Monitor",            // Task name
			SQL_TASK_STACK_DEPTH,        // Stack size in words
			NULL,                    // Task parameters (not used here)
			SQL_TASK_PRIORITY,                       // Task priority
			xTaskStack,              // Pointer to task stack (allocated in PSRAM)
			&xSQLTaskBuffer             // Pointer to task control block
		);
	if (sqlHandle == NULL) {
			printf("Failed to create task\n");
		   // free(xTaskStack);
			// Handle error
		}

	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor
	strcpy(s_last_db_file_name, Database_Dir "Virtual_Table.db");

	FirstEntry = 1;
	init_mutex();
	sqlite3_initialize();
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
		if (pdTRUE == xQueueReceive(sql_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
//			printf("SQL msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("SQL DT = %s\n",s_Message_Rx->payload_p8);
//			}

			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DB_EXECUTE"))
			{
				Excecute_database_command(s_Message_Rx);
			}

			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DB_OPEN"))
			{
				create_db(s_Message_Rx);
				db_open((char*)&sql_para.file_name, &db1, s_Message_Rx);

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DB_CLOSE"))
			{
			  sqlite3_close(db1);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties
				if (FirstEntry == 0)
					init(0,0);
				else if(FirstEntry==1){
			    Add_Response_msg("SQL Initialization Done.",s_Message_Rx);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx);
				    }
				else{
				head_JSON = name_JSON->child;
			   // Loop through each key-value pair
				do {
					// Check if the value string is not NULL
					if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
					{
						// Set the key-value pair
						u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
						if(u8Result==2){
							sprintf(str,"'%s' is a read only property", head_JSON->string);
							 Add_Response_msg(str, s_Message_Rx);
						}
					} else {
						// Handle the case where value string is NULL (e.g., log an error or take appropriate action)
						sprintf(str, "Invalid parameter '%s'", head_JSON->string);
						Add_Response_msg(str,s_Message_Rx);
						// Handle the error as per your application's requirements
					}
					head_JSON = head_JSON->next;
				} while (head_JSON != 0);

				// Free the parsed JSON
				cJSON_Delete(name_JSON);
				if(u8Result==1){
				//  save parameters to JFS
				console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
			}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			{
				get_actor_properties(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "SAVE_DB_RECORD"))
			{
				Save_Database_Record(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_DB_RECORDS"))
			{
				root_DB = cJSON_CreateArray();
				Excecute_database_command(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "GET_SYNC_STATUS"))
			{
				Read_DB_Sync_Status(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PRINT"))
			{
				Print_Database(s_Message_Rx);
			}
			else
			{
			     Add_Response_msg("invalid method", s_Message_Rx);
			}
		}
	}
}

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[SQL_QUERY_LEN] = {0};
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
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the SQL actor.");
	cJSON_AddStringToObject(responseObject, "SET(U32 RECORD_COUNT)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "DB_OPEN(string FILE_NAME)","Open the database.");
	cJSON_AddStringToObject(responseObject, "DB_EXECUTE(string FILE_NAME, string QUERY)", "Obtain the device information.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "PRINT(string FILE_NAME)", "Print the database file");
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

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
		Add_Response_msg(str,s_Message_Rx);
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
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

void SQL_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(sql_Rx_Queue, s_Message, QUE_DELAY);

	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<SQL.ERROR(SQL RX Queue is full) \n");
		}
		else
		{
			printf("<SQL.ERROR(SQL RX Queue send unsuccessful) \n");
		}
	}
}//	SQL_ConsolWriteToActor

//========================= SQL Functions ============================================//
static int callback(void *data, int argc, char **argv, char **azColName)  // argc = number of columns
{
   int i;
   AMessage_st* s_Message_Rx = (AMessage_st*)data;
   cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
   if (responseObject == NULL)
   {
	   return 0;
   }

   for (i = 0; i<argc; i++)
   {
	 	cJSON_AddStringToObject(responseObject, azColName[i], argv[i] ? argv[i] : "NULL");
//	 	if(READ_DB_RECORDS_Flag == 1)
//	    {
//			if(i == 1)  // store record
//				record = argv[i];
//			if(i == 0)  // store record id
//				id = atoi(argv[i]);
//	    }
   }
//   if(READ_DB_RECORDS_Flag == 1)
//   {
//		cJSON *recordObject = cJSON_CreateObject();
//		cJSON_AddItemToArray(root_DB, recordObject);
//		cJSON_AddStringToObject(recordObject, "DATA", record);
//		cJSON_AddNumberToObject(recordObject, "RecordNumber", id);
//		memset(payLoadData_CallBack,0,sizeof(payLoadData_CallBack));//\0';
//		cJSON_PrintPreallocated(root_DB, payLoadData_CallBack, sizeof(payLoadData_CallBack), false);
//		if(strlen(payLoadData_CallBack) > 4096)  // set the maximum size of records to be sent to the server
//		{
//			READ_DB_RECORDS_Flag = 0;
//			READ_DB_RECORDS_Done = 1;
//			//printf("\n string = %s",string);
//		}
//   }
//   else if(READ_DB_RECORDS_Done == 0)
   {
		memset(payLoadData_CallBack,0,sizeof(payLoadData_CallBack));//\0';
		cJSON_PrintPreallocated(responseObject, payLoadData_CallBack, sizeof(payLoadData_CallBack), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_CallBack);
		console_send_responce_to_console_xface(s_Message_Rx);
		sql_cb_row_count++;
		/* Avoid starving other high-priority tasks when large SELECTs stream many rows. */
		if ((sql_cb_row_count % 8U) == 0U)
		{
			vTaskDelay(1);
		}
   }
   cJSON_Delete(responseObject);
   return 0;
}

static int db_open(const char *filename, sqlite3 **db, AMessage_st* s_Message_Rx) {

   int rc = sqlite3_open(filename, db);
   if (rc) {
	   Add_Response_msg("Can't open database",s_Message_Rx);
       return rc;
   }
   return rc;
}

static int db_exec(sqlite3 *db, const char *sql, AMessage_st* s_Message_Rx) {
    char *zErrMsg = 0;  // Pointer to hold any error messages
    int rc;
    int retry_count = 0;
    const int max_retries = 3;  // Maximum number of retries
    char str[200] = {0};

    // Retry loop
    do {
    	sql_cb_row_count = 0;
    	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        rc = sqlite3_exec(db, sql, callback, (void*)s_Message_Rx, &zErrMsg);  // Execute the SQL command
        sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
        if (rc == SQLITE_OK) {
            break;
        }


        // Handle specific error codes
        switch (rc) {
            case SQLITE_BUSY:
                // Database is busy, retry with backoff
                vTaskDelay(pdMS_TO_TICKS(100 * (retry_count + 1)));  // Exponential backoff
                break;
            case SQLITE_NOMEM:
                Add_Response_msg("Out of memory during database operation", s_Message_Rx);
                return rc;  // No point in retrying
            case SQLITE_IOERR:
                Add_Response_msg("I/O error during database operation", s_Message_Rx);
                return rc;  // Retry likely won't fix hardware issues
            case SQLITE_ERROR:
                Add_Response_msg("SQL syntax error, check the query", s_Message_Rx);
                return rc;  // No retries for syntax errors
            case SQLITE_CONSTRAINT:
                Add_Response_msg("Database constraint violation", s_Message_Rx);
                return rc;  // No retries for constraint violations
            case SQLITE_FULL:
                Add_Response_msg("Disk is full, unable to write to database", s_Message_Rx);
                return rc;  // Disk is full, no point in retrying
            case SQLITE_CORRUPT:
            	sprintf(str, "Database is corrupted for query %s.", sql);
                Add_Response_msg(str, s_Message_Rx);
                return rc;  // No retries for corrupt databases
            default:
            	sprintf(str, "Operation not successful. Error is '%s'", zErrMsg);
            	Add_Response_msg(str, s_Message_Rx);
                break;  // Other errors might be handled by retrying
        }

        // Free the error message memory allocated by sqlite3_exec
        if (zErrMsg) {
            sqlite3_free(zErrMsg);
            zErrMsg = 0;  // Reset the error message pointer
        }

        retry_count++;

    } while (rc != SQLITE_OK && retry_count < max_retries);

    if (rc != SQLITE_OK) {
        Add_Response_msg("Operation not successful after retries", s_Message_Rx);
    }
    return rc;  // Return the final result code of the SQLite execution
}

static uint8_t Parse_DB_Exe(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char	str[100]={0};
	char	str_Fname[100]={0};
	char    json_sanitized[SQL_QUERY_LEN] = {0};
	memset((char*)&sql_para.file_name, 0, sizeof(sql_para.file_name));
	memset((char*)&sql_para.query, 0, sizeof(sql_para.query));
	/* Normalize payload and parse only the balanced JSON object block. */
	const char *raw = (char*)s_Message_Rx->payload_p8;
	size_t out = 0;
	for (size_t i = 0; (raw != NULL) && (raw[i] != '\0') && (out < (sizeof(json_sanitized) - 1)); i++)
	{
		char ch = raw[i];
		if ((ch == '\r') || (ch == '\n'))
		{
			ch = ' ';
		}
		/* Drop non-printable control bytes that can break cJSON parse. */
		if (((unsigned char)ch < 0x20) && (ch != '\t') && (ch != ' '))
		{
			continue;
		}
		json_sanitized[out++] = ch;
	}
	json_sanitized[out] = '\0';

	char *json_start = strchr(json_sanitized, '{');
	if (json_start != NULL)
	{
		int depth = 0;
		int in_quotes = 0;
		int escape_next = 0;
		char *json_end = NULL;
		for (char *p = json_start; *p != '\0'; ++p)
		{
			char ch = *p;
			if (escape_next)
			{
				escape_next = 0;
				continue;
			}
			if ((ch == '\\') && in_quotes)
			{
				escape_next = 1;
				continue;
			}
			if (ch == '"')
			{
				in_quotes = !in_quotes;
				continue;
			}
			if (in_quotes)
			{
				continue;
			}
			if (ch == '{')
			{
				depth++;
			}
			else if (ch == '}')
			{
				depth--;
				if (depth == 0)
				{
					json_end = p;
					break;
				}
			}
		}
		if (json_end != NULL)
		{
			*(json_end + 1) = '\0';
			in_JSON = cJSON_Parse(json_start);
		}
	}
	if (in_JSON == NULL)
	{
		char file_name_local[SQL_FILENAME_LEN] = {0};
		char query_local[SQL_QUERY_LEN] = {0};
		int has_file = extract_json_string_field(json_sanitized, "FILE_NAME", file_name_local, sizeof(file_name_local));
		int has_query = extract_json_string_field(json_sanitized, "QUERY", query_local, sizeof(query_local));
		if (!(has_file && has_query))
		{
			has_file = extract_loose_string_field(json_sanitized, "FILE_NAME", file_name_local, sizeof(file_name_local));
			has_query = extract_loose_string_field(json_sanitized, "QUERY", query_local, sizeof(query_local));
		}
		if (!(has_file && has_query))
		{
			char json_unescaped[SQL_QUERY_LEN] = {0};
			char work[SQL_QUERY_LEN] = {0};
			strncpy(work, json_sanitized, sizeof(work)-1);
			for (int pass = 0; pass < 3; pass++)
			{
				unescape_json_once(work, json_unescaped, sizeof(json_unescaped));
				if (strcmp(work, json_unescaped) == 0)
				{
					break;
				}
				has_file = extract_json_string_field(json_unescaped, "FILE_NAME", file_name_local, sizeof(file_name_local));
				has_query = extract_json_string_field(json_unescaped, "QUERY", query_local, sizeof(query_local));
				if (!(has_file && has_query))
				{
					has_file = extract_loose_string_field(json_unescaped, "FILE_NAME", file_name_local, sizeof(file_name_local));
					has_query = extract_loose_string_field(json_unescaped, "QUERY", query_local, sizeof(query_local));
				}
				if (has_file && has_query)
				{
					break;
				}
				strncpy(work, json_unescaped, sizeof(work)-1);
			}
			if (!(has_file && has_query))
			{
				in_JSON = cJSON_Parse(json_unescaped);
			}
		}
		if (has_query)
		{
			if (has_file)
			{
				strcpy((char*)&sql_para.file_name, Database_Dir);
				char *filename = strrchr(file_name_local, '/');
				if (filename != NULL)
				{
					filename++;
					strcat((char*)&sql_para.file_name, filename);
				}
				else
				{
					strcat((char*)&sql_para.file_name, file_name_local);
				}
			}
			else
			{
				/* If FILE_NAME is mangled but QUERY is valid, reuse previous DB file path. */
				strcpy((char*)&sql_para.file_name, s_last_db_file_name);
			}
			strcpy((char*)&sql_para.query, query_local);
			if (!is_likely_complete_query((char*)&sql_para.query))
			{
				char semicolon_query[SQL_QUERY_LEN] = {0};
				if (extract_query_until_semicolon(json_sanitized, semicolon_query, sizeof(semicolon_query)) &&
					is_likely_complete_query(semicolon_query))
				{
					strcpy((char*)&sql_para.query, semicolon_query);
					if (strlen((char*)&sql_para.file_name) > 0)
					{
						strncpy(s_last_db_file_name, (char*)&sql_para.file_name, sizeof(s_last_db_file_name)-1);
					}
					return 1;
				}
				Add_Response_msg("Incomplete SQL query payload. Please retry command.", s_Message_Rx);
				return 0;
			}
			if (strlen((char*)&sql_para.file_name) > 0)
			{
				strncpy(s_last_db_file_name, (char*)&sql_para.file_name, sizeof(s_last_db_file_name)-1);
			}
			return 1;
		}
		if (in_JSON == NULL)
		{
			in_JSON = cJSON_Parse(json_sanitized);
		}
	}
	if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
			return 0;
	}

	strcpy((char*)&sql_para.file_name, Database_Dir);
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		strcpy(str_Fname,name_JSON->valuestring);
	}

    // Use strrchr to find the last occurrence of '/'
    char *filename = strrchr(str_Fname, '/');

    if (filename != NULL) {
        // Move the pointer one position forward to skip the '/'
        filename++;
        strcat((char*)&sql_para.file_name, filename);
    }
    if (strlen((char*)&sql_para.file_name) > 0)
    {
    	strncpy(s_last_db_file_name, (char*)&sql_para.file_name, sizeof(s_last_db_file_name)-1);
    }
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "QUERY");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
		strcpy((char*)&sql_para.query,name_JSON->valuestring);
	else
	{
		Add_Response_msg("Invalid Json input: QUERY missing", s_Message_Rx);
		cJSON_Delete(in_JSON);
		return 0;
	}
	cJSON_Delete(in_JSON);
	return 1;
}

static int is_likely_complete_query(const char *query)
{
	if (query == NULL)
	{
		return 0;
	}
	size_t len = strlen(query);
	if (len < 6)
	{
		return 0;
	}
	int depth = 0;
	int in_quotes = 0;
	int escape_next = 0;
	for (size_t i = 0; i < len; i++)
	{
		char ch = query[i];
		if (escape_next)
		{
			escape_next = 0;
			continue;
		}
		if ((ch == '\\') && in_quotes)
		{
			escape_next = 1;
			continue;
		}
		if (ch == '\'')
		{
			in_quotes = !in_quotes;
			continue;
		}
		if (in_quotes)
		{
			continue;
		}
		if (ch == '(')
		{
			depth++;
		}
		else if (ch == ')')
		{
			depth--;
			if (depth < 0)
			{
				return 0;
			}
		}
	}
	return (depth == 0) && (in_quotes == 0);
}

static int extract_query_until_semicolon(const char *src, char *out, size_t out_sz)
{
	if ((src == NULL) || (out == NULL) || (out_sz == 0))
	{
		return 0;
	}
	const char *k = strstr(src, "QUERY");
	if (k == NULL)
	{
		return 0;
	}
	const char *p = strchr(k, ':');
	if (p == NULL)
	{
		return 0;
	}
	p++;
	while ((*p == ' ') || (*p == '\t') || (*p == '\\') || (*p == '"') || (*p == '\''))
	{
		p++;
	}
	size_t idx = 0;
	while ((*p != '\0') && (idx < (out_sz - 1)))
	{
		char ch = *p++;
		if (ch == ';')
		{
			out[idx++] = ';';
			out[idx] = '\0';
			return 1;
		}
		if ((ch == '\r') || (ch == '\n'))
		{
			continue;
		}
		out[idx++] = ch;
	}
	out[idx] = '\0';
	return 0;
}

static int extract_json_string_field(const char *src, const char *key, char *out, size_t out_sz)
{
	if ((src == NULL) || (key == NULL) || (out == NULL) || (out_sz == 0))
	{
		return 0;
	}
	char key_token[64] = {0};
	snprintf(key_token, sizeof(key_token), "\"%s\"", key);
	const char *k = strstr(src, key_token);
	if (k == NULL)
	{
		return 0;
	}
	const char *p = strchr(k + strlen(key_token), ':');
	if (p == NULL)
	{
		return 0;
	}
	p++;
	while ((*p == ' ') || (*p == '\t'))
	{
		p++;
	}
	if (*p != '"')
	{
		return 0;
	}
	p++;
	size_t idx = 0;
	int escape = 0;
	while (*p != '\0')
	{
		char ch = *p++;
		if (escape)
		{
			if (idx < (out_sz - 1))
			{
				out[idx++] = ch;
			}
			escape = 0;
			continue;
		}
		if (ch == '\\')
		{
			escape = 1;
			continue;
		}
		if (ch == '"')
		{
			out[idx] = '\0';
			return 1;
		}
		if (idx < (out_sz - 1))
		{
			out[idx++] = ch;
		}
	}
	return 0;
}

static int extract_loose_string_field(const char *src, const char *key, char *out, size_t out_sz)
{
	if ((src == NULL) || (key == NULL) || (out == NULL) || (out_sz == 0))
	{
		return 0;
	}
	const char *k = strstr(src, key);
	if (k == NULL)
	{
		return 0;
	}
	const char *p = strchr(k, ':');
	if (p == NULL)
	{
		return 0;
	}
	p++;
	while ((*p == ' ') || (*p == '\t') || (*p == '\\'))
	{
		p++;
	}
	char quote = 0;
	if ((*p == '"') || (*p == '\''))
	{
		quote = *p;
		p++;
	}
	size_t idx = 0;
	int escape = 0;
	while (*p != '\0')
	{
		char ch = *p++;
		if (escape)
		{
			if (idx < (out_sz - 1))
			{
				out[idx++] = ch;
			}
			escape = 0;
			continue;
		}
		if (ch == '\\')
		{
			escape = 1;
			continue;
		}
		if (quote != 0)
		{
			if (ch == quote)
			{
				out[idx] = '\0';
				return (idx > 0);
			}
		}
		else if ((ch == ',') || (ch == '}'))
		{
			out[idx] = '\0';
			return (idx > 0);
		}
		if (idx < (out_sz - 1))
		{
			out[idx++] = ch;
		}
	}
	out[idx] = '\0';
	return (idx > 0);
}

static void unescape_json_once(const char *src, char *dst, size_t dst_sz)
{
	if ((src == NULL) || (dst == NULL) || (dst_sz == 0))
	{
		return;
	}
	size_t w = 0;
	for (size_t r = 0; (src[r] != '\0') && (w < (dst_sz - 1)); r++)
	{
		if ((src[r] == '\\') && (src[r + 1] != '\0'))
		{
			char next = src[r + 1];
			if ((next == '"') || (next == '\\') || (next == '/') || (next == '\''))
			{
				dst[w++] = next;
				r++;
				continue;
			}
		}
		dst[w++] = src[r];
	}
	dst[w] = '\0';
}

static void create_db(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char str[100]={0};

		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
			return;
		}
		if(strcmp(in_JSON->child->string,"FILE_NAME")==0)
		{
			name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
			strcpy((char*)&sql_para.file_name,name_JSON->valuestring);
		}
		cJSON_Delete(in_JSON);
	return;
}

void replaceSymbol(char *str, char find, char replace) {
    char *position;

    // Find the first occurrence of the symbol in the string
    position = strchr(str, find);

    // Continue replacing until no more occurrences are found
    while (position != NULL) {
        *position = replace; // Replace the symbol

        // Search for the next occurrence
        position = strchr(position + 1, find);
    }
}

// Callback function for SQLite3
static int countCallback(void *data, int argc, char **argv, char **azColName) {
    int *count = (int *)data;
    *count = atoi(argv[0]);
    return 0;
}
// Function to delete the record with the smallest ID
static void deleteSmallestRecord(sqlite3 *db, char* Database_table) {
    // Step 1: Find the smallest ID
    char selectSql[100] = {0}; // = "SELECT ID FROM %s ORDER BY ID ASC LIMIT 1;";
    sprintf(selectSql, "SELECT ID FROM %s ORDER BY ID ASC LIMIT 1;", Database_table);
    int smallestID = 0;
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    int rc = sqlite3_exec(db, selectSql, countCallback, &smallestID, NULL);
    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
    //    printf("Error finding smallest ID: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Step 2: Delete the record with the smallest ID
//    const char *deleteSql = "DELETE FROM Test_table WHERE ID = ?;";
    sprintf(selectSql, "DELETE FROM %s WHERE ID = ?;", Database_table);
    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
  //      printf("Error preparing delete statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int(stmt, 1, smallestID);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
#ifdef ENABLE_PRINT_MSG
    	printf("Error deleting record: %s\n", sqlite3_errmsg(db));
#endif
    }
    sqlite3_finalize(stmt);
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n SQL s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
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
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
			printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
		}
		// Free the parsed JSON
		cJSON_Delete(in_JSON);
	}
	return;
}

static void Save_Database_Record(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char dbName[100] = {0}, str[600] = {0}, Database_name[50] = {0};
	char Record[500] = {0};
	int rc = 0;
	sqlite3 *db;
	char *errMsg = NULL;
	char Database_table[50] = {0}; //, db_metadata_table[50] = {0};
	int recordCount = 0, Min_ID = 0, Max_ID = 0; // table_exits = -1;
	sqlite3_stmt *stmt;
	int post_to_http = 1; // HTTP_retry_interval = 600, POST_AS_D2C = 0;
	in_JSON 	  = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}

	strcpy(dbName, Database_Dir);    // database name
	name_JSON 	  = cJSON_GetObjectItem(in_JSON, "DBNAME");
	if((name_JSON != NULL)&&(name_JSON->valuestring != NULL))
	{
		strcat(dbName, name_JSON->valuestring);    // database name
		strcat(Database_table, name_JSON->valuestring);		 // database table name
		strcpy(Database_name, name_JSON->valuestring);
	}
	strcat(dbName,".db");
	strcat(Database_name,".db");
	strcat(Database_table,"_table");
	name_JSON 	  = cJSON_GetObjectItem(in_JSON, "RECORD_DATA");
	if((name_JSON != NULL)&&(name_JSON->valuestring != NULL))
		strcpy(Record, name_JSON->valuestring);
	cJSON_Delete(in_JSON);

	// Insert metadata in the database
	 uint64_t start = esp_timer_get_time();
	if (db_open(dbName, &db, s_Message_Rx))
		return;

	 // Set the journal mode to WAL
	 rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL;", 0, 0, 0);

	 if (rc != SQLITE_OK) {
		 // Handle error setting journal mode
#ifdef ENABLE_PRINT_MSG
		 printf("\n PRAGMA journal_mode = WAL");
#endif
		 Add_Response_msg("Error in setting journal mode",s_Message_Rx);
		 goto exit;
	 }

	 // Set the cache size using PRAGMA cache_size
	 rc = sqlite3_exec(db, "PRAGMA cache_size = 10000;", 0, 0, 0);

	 if (rc != SQLITE_OK)
	 {
#ifdef ENABLE_PRINT_MSG
		 printf("\n PRAGMA cache_size = 10000");
#endif
		 Add_Response_msg("Error in setting PRAGMA cache_size",s_Message_Rx);
		 goto exit;
	 }

	 rc = sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);

	 //Create database
	 sprintf(str, "CREATE TABLE IF NOT EXISTS %s ("
			"ID INTEGER PRIMARY KEY,"
			 "Record TEXT"
			 ");",Database_table);
	 rc = sqlite3_exec(db, str, callback, 0, &errMsg);
	 if (rc != SQLITE_OK) {
		sprintf(str,"SQL error: %s", errMsg);
		sqlite3_free(errMsg);
		errMsg = NULL;
		Add_Response_msg(str,s_Message_Rx);
		goto exit;
	 }
	 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);

	 //Check record count
	 sprintf(str,"SELECT COUNT(*) FROM %s;",Database_table);
	 sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	 rc = sqlite3_exec(db, str, countCallback, &recordCount, NULL);
	 if (rc != SQLITE_OK) {
		sqlite3_close(db);
		Add_Response_msg("Error in checking number of record counts",s_Message_Rx);
		return;
	 }
	 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
	 if (rc == SQLITE_OK && ((recordCount) > (MAX_RECORD_COUNT-1)))  // to limit the number records to MAX_RECORD_COUNT
	 {
	   deleteSmallestRecord(db, Database_table);
	 }

	 sprintf(str,"INSERT INTO %s (Record) VALUES ('%s');",Database_table, Record);
	 rc = sqlite3_prepare_v2(db, str, strlen(str), &stmt, NULL);
	 if (rc != SQLITE_OK)
	 {
		sprintf(str, "Error preparing insert statement: %s", sqlite3_errmsg(db));
		Add_Response_msg(str,s_Message_Rx);
		goto exit;
	 }
	sqlite3_bind_text(stmt, 1, Record, strlen(Record), SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		sprintf(str,"Error inserting record: %s\n", sqlite3_errmsg(db));
		Add_Response_msg(str,s_Message_Rx);
		sqlite3_finalize(stmt);
		goto exit;
	}
	sqlite3_clear_bindings(stmt);
	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		goto exit;
	}
	sqlite3_finalize(stmt);
	last_time = esp_timer_get_time()-start;

	// get min and max IDs of database
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	 sprintf(str,"SELECT MIN(ID) FROM %s;",Database_table);
		 rc = sqlite3_exec(db, str, countCallback, &Min_ID, NULL);
		 if (rc != SQLITE_OK) {
			sqlite3_close(db);
			Add_Response_msg("Error in getting minimum record ID",s_Message_Rx);
			return;
		 }
		 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
	 sprintf(str,"SELECT MAX(ID) FROM %s;",Database_table);
	 sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		 rc = sqlite3_exec(db, str, countCallback, &Max_ID, NULL);
		 if (rc != SQLITE_OK) {
			sqlite3_close(db);
			Add_Response_msg("Error in getting maximum record ID",s_Message_Rx);
			return;
		 }
		 rc = sqlite3_exec(db, "END TRANSACTION", 0, 0, 0);
exit:
	if(errMsg != NULL)
	  sqlite3_free(errMsg);
	sqlite3_close(db);
	cJSON *responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "DB_NAME",Database_name);
	cJSON_AddStringToObject(responseObject, "RECORD",Record);
	cJSON_AddNumberToObject(responseObject, "POST_TO_HTTP",post_to_http);
	cJSON_AddNumberToObject(responseObject, "MIN_ID",Min_ID);
	cJSON_AddNumberToObject(responseObject, "MAX_ID",Max_ID);
	cJSON_AddNumberToObject(responseObject, "Execution Time (msec)",(last_time/1000));
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
	return;
}

static void Excecute_database_command(AMessage_st* s_Message_Rx)
{
    int rc = -1;
    // Take the mutex before executing the database command
    if (xSemaphoreTake(db_mutex, portMAX_DELAY) == pdTRUE) {
        if (!Parse_DB_Exe(s_Message_Rx)) {
            xSemaphoreGive(db_mutex);
            return;
        }

        sqlite3 *db;
        rc = db_open((char*)&sql_para.file_name, &db, s_Message_Rx);
        if (rc != SQLITE_OK) {
            Add_Response_msg("Error! Cannot open the database.", s_Message_Rx);
            xSemaphoreGive(db_mutex);
            return;
        }

//        int64_t start = esp_timer_get_time();
        rc = db_exec(db, (char*)&sql_para.query, s_Message_Rx);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            xSemaphoreGive(db_mutex);
            return;
        }

//        long int last_time1 = esp_timer_get_time() - start;

//        if (READ_DB_RECORDS_Flag == 1 || READ_DB_RECORDS_Done == 1) {
//            READ_DB_RECORDS_Flag = 0;
//            READ_DB_RECORDS_Done = 0;
//
//            // Extract the filename from the full path
//            char *filename = strrchr((char*)&sql_para.file_name, '/');
//            if (filename != NULL) {
//                filename++; // Skip the '/' character
//            } else {
//                filename = (char*)&sql_para.file_name; // Use the full string if no '/' found
//            }
//            strcpy(dBName, filename);
//
//            // Remove the file extension to form the table name
//            char *dot_position = strchr(dBName, '.');
//            if (dot_position != NULL) {
//                strncpy(dB_table, dBName, dot_position - dBName);
//                dB_table[dot_position - dBName] = '\0';
//            } else {
//                strcpy(dB_table, dBName);
//            }
//            strcat(dB_table, "_table");
//
//            // Fetch the minimum ID from the table
//            snprintf(str, sizeof(str), "SELECT MIN(ID) FROM %s;", dB_table);
//            sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
//            rc = sqlite3_exec(db, str, countCallback, &Min_ID, NULL);
//            if (rc != SQLITE_OK) {
//                sqlite3_close(db);
//                Add_Response_msg("Error in getting minimum record ID", s_Message_Rx);
//                xSemaphoreGive(db_mutex);
//                return;
//            }
//            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
//            // Fetch the maximum ID from the table
//            snprintf(str, sizeof(str), "SELECT MAX(ID) FROM %s;", dB_table);
//            sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
//            rc = sqlite3_exec(db, str, countCallback, &Max_ID, NULL);
//            if (rc != SQLITE_OK) {
//                sqlite3_close(db);
//                Add_Response_msg("Error in getting maximum record ID", s_Message_Rx);
//                xSemaphoreGive(db_mutex);
//                return;
//            }
//            sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
//            // Create a JSON object with the results
//            cJSON *root = cJSON_CreateObject();
//            if(root == NULL)
//            {
//                 Add_Response_msg("Error! failed to create cJSON object in SQL.", s_Message_Rx);
//				 sqlite3_close(db);
//				 xSemaphoreGive(db_mutex);
//				 return;
//            }
//            cJSON_AddStringToObject(root, "DB_NAME", dBName);
//            cJSON_AddItemToObject(root, "DB_RECORDS", root_DB);
//            cJSON_AddNumberToObject(root, "MIN_ID", Min_ID);
//            cJSON_AddNumberToObject(root, "MAX_ID", Max_ID);
//
//			memset(payLoadData,0,sizeof(payLoadData));//\0';
//			cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
//			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//
//            cJSON_Delete(root);
//            console_send_responce_to_console_xface(s_Message_Rx);
//        }
        sqlite3_close(db);
        // Release the mutex after the command is executed
        xSemaphoreGive(db_mutex);
    } else {
        Add_Response_msg("Error! Unable to obtain database mutex.", s_Message_Rx);
    }
}


static void Read_DB_Sync_Status(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	cJSON *Sync_Status_Array 	= NULL;
	char str[150] = {0}, DB_Name[100] = {0}, DB_table[100] = {0}, Database_name[50] = {0};
	char* token = NULL;
	sqlite3 *db;
	char *errMsg = 0;
	int rc = -1, Min_ID = 0, Max_ID = 0, count = 0;

	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);  //It is deleted in http_stream_reader() function
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "DB_NAME");
	if((name_JSON != NULL) && (cJSON_IsArray(name_JSON)))
	{
		int arraySize = cJSON_GetArraySize(name_JSON);
		Sync_Status_Array = cJSON_CreateArray();

		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++)
		{
			strcpy(DB_Name, Database_Dir);
			cJSON *Object_element = cJSON_GetArrayItem(name_JSON, i);
			if (cJSON_IsObject(Object_element))
			{
				cJSON *element = cJSON_GetObjectItem(Object_element, "name");
				if (cJSON_IsString(element) && (element->valuestring != NULL))  // exclude metadata.db files
				{
					strcat(DB_Name, element->valuestring);
					strcpy(Database_name, element->valuestring);
					token = strtok(element->valuestring, ".");
					strcpy(DB_table, token);
					strcat(DB_table, "_table");
					 // Open connection to SQLite database
					    rc = sqlite3_open(DB_Name, &db); // Replace "path/to/your/database.db" with the path to your SQLite database file
					    if (rc) {
					        sprintf(str, "Can't open database: %s\n", sqlite3_errmsg(db));
					        sqlite3_close(db);
					        Add_Response_msg(str,s_Message_Rx);
					        goto exit;
					    }

					    sprintf(str, "SELECT COUNT(*) FROM %s;",DB_table);
					    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
					    rc = sqlite3_exec(db, str, countCallback, &count, &errMsg);
					    sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
						if(count == 0)
						{
							Min_ID = 0;
							Max_ID = 0;
							goto next;
						}

						 sprintf(str,"SELECT MIN(ID) FROM %s;",DB_table);
						 sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
						 rc = sqlite3_exec(db, str, countCallback, &Min_ID, &errMsg);
						 if (rc != SQLITE_OK)
						 {
							sprintf(str,"Error in getting minimum record ID : %s", errMsg);
							Add_Response_msg(str,s_Message_Rx);
							sqlite3_free(errMsg);
							Min_ID = 0;
							goto next;
						 }
						 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
						 sprintf(str,"SELECT MAX(ID) FROM %s;",DB_table);
						 sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
						 rc = sqlite3_exec(db, str, countCallback, &Max_ID, &errMsg);
						 if (rc != SQLITE_OK)
						 {
							sprintf(str,"Error in getting maximum record ID : %s", errMsg);
							Add_Response_msg(str,s_Message_Rx);
							sqlite3_free(errMsg);
							Max_ID = 0;
							goto next;
						 }
						 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);

			next:		 sqlite3_close(db);
						cJSON *recordObject = cJSON_CreateObject();
						cJSON_AddItemToArray(Sync_Status_Array, recordObject);
						cJSON_AddStringToObject(recordObject, "DbName", Database_name);
						cJSON_AddNumberToObject(recordObject, "MinRecord", Min_ID);
						cJSON_AddNumberToObject(recordObject, "MaxRecord", Max_ID);
				}  // end of if (strstr(element->valuestring, substring) == NULL)
			}
		}  // end of for loop
	}
	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "SYNC_STATUS", Sync_Status_Array); 
  	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(root);
	console_send_responce_to_console_xface(s_Message_Rx);
 exit:cJSON_Delete(in_JSON);
}

static void Print_Database(AMessage_st* s_Message_Rx)
{
	char file_name[30] = {0}, str[100] = {0}, table_name[30] = {0}, Database[30] = {0}, file_name_path[30] = {0};
	int  End_Record = 0, Start_Record = 1, records_to_fetch = 20;
	sqlite3 *db;
	int rc = -1;

	if((char*)s_Message_Rx->payload_p8 == NULL)
	{
		Add_Response_msg("Error! Payload is empty",s_Message_Rx);
		return;
	}
	cJSON *in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8 );
	if (in_JSON == NULL)
	{
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}
	cJSON *name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		strcpy(file_name,name_JSON->valuestring);
		strcpy(file_name_path,name_JSON->valuestring);

	}
	cJSON_Delete(in_JSON);

    // Use strrchr to find the last occurrence of '/'
    char *filename = strrchr(file_name, '/');

    if (filename != NULL) {
        // Move the pointer one position forward to skip the '/'
        filename++;
//        printf("Extracted filename: %s\n", filename);
        strcpy(file_name, filename);
    }

    // Remove the file extension to form the table name
    char *dot_position = strchr(file_name, '.');
    if (dot_position != NULL) {
        strncpy(table_name, file_name, dot_position - file_name);
        table_name[dot_position - file_name] = '\0';
    } else {
        strcpy(table_name, file_name);
        strcat(file_name,".db");
    }

	strcpy(Database, Database_Dir);
	strcat(Database, file_name);
	if (db_open(Database, &db, s_Message_Rx))
		return;
	 //Check record count
	 sprintf(str,"SELECT COUNT(*) FROM %s_table;",table_name);
	 sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	 rc = sqlite3_exec(db, str, countCallback, &End_Record, NULL);
	 if (rc != SQLITE_OK) {
		sqlite3_close(db);
		Add_Response_msg("Error in checking number of record counts",s_Message_Rx);
		return;
	 }
	 sqlite3_exec(db, "END TRANSACTION;", NULL, NULL, NULL);
	 sqlite3_close(db);
	// printf("\n\n recordCount = %d \n\n",End_Record);

	 if(End_Record == 0)
	 {
		 sprintf(str, "Table is empty in %s database", file_name);
		 Add_Response_msg(str,s_Message_Rx);
		 sqlite3_close(db);
		 return;
	 }

	 while(Start_Record <= End_Record)
	 {
//		 printf("\n\n Start_Record = %d, End_Record = %d \n\n",Start_Record, End_Record);
		cJSON *root = cJSON_CreateObject();
		if (root != NULL)
		{
			cJSON_AddStringToObject(root, "FILE_NAME", file_name_path);
			sprintf(str, "SELECT * FROM %s_table  WHERE rowid >= %d LIMIT %d;", table_name, Start_Record, records_to_fetch);
			cJSON_AddStringToObject(root, "QUERY", str);
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			Excecute_database_command(s_Message_Rx);
			cJSON_Delete(root);
		}
		else
			Add_Response_msg("Error! Failed to allocate memory for JSON.",s_Message_Rx);

		Start_Record += records_to_fetch;
		//vTaskDelay(10);
	 }
}

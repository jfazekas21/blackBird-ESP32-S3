/*
 * File_server_Actor.c
 *
 *  Created on: 16-Sep-2022
 *      Author: Ashwini
 */

#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "Web_server_Actor.h"
#include "esp_ota_ops.h"
#include "ping.h"
#include <esp_http_server.h>
#include "esp_app_format.h"
//static const char aTag[] = "FILE_SERVER";

PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static StackType_t xTaskStack[WEBSERVER_TASK_STACK_DEPTH];
static const char * THIS_ACTOR = "WEB_SERVER";
static const char 			THIS_ACTOR_ID 	= 	WEB_SERVER;
TaskHandle_t file_serverHandle= NULL, ServerHandle = NULL;
BaseType_t file_serverMonitor, Start_server;
QueueHandle_t fileserver_Rx_queue;  //fileserver_Tx_queue
static StaticTask_t xWEBSERVERTaskBuffer;  //// Declare a static task control block

static AMessage_st s_Message_Tx;//s_Message_Rx,
//static Actor_st s_Fileserver;						// File server Actor structure
char keysTemp[MAX_OUT_SIZE];
char keysValue[MAX_OUT_SIZE];
static int FirstEntry = 0;
static int WIFI_status = -1;
static char Web_Server_operation = 0;
static char display_msg = 0;
static int NET_status = -1;
static char u8stopWebServer =0;
httpd_handle_t server = NULL;
#define OBJ_QUE_COUNT                	5



#define FILE_PATH_MAX (200)
/* Max length a file path can have on storage */
//#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
//#define MAX_FILE_SIZE   (200*1024)  // 200 KB
//#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  1024 * 10 //20480

struct file_server_data {
    /* Base path of file storage */
char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
char scratch[SCRATCH_BUFSIZE];
};

//static uint8_t *Monitor_pucQueueStorage = NULL;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [OBJ_QUE_COUNT * sizeof(AMessage_st)];

static StaticQueue_t Monitor_pxQueueBuffer;
//static StackType_t *xTaskStack;


static struct fileserver{
	uint8_t		connectStatus_u8;
}s_Para;

static struct property prop[] = // Actor Property
{
	{ &s_Para.connectStatus_u8  	,   "CONN_STATUS"   ,   U_INT8,  	"R",  	"Connection status of web server" },
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];

static void monitor(void *pvParameters __attribute__((unused)));
//static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					//	Change a parameter
static void get(char *prop, char *val);							//	Read a Parameter
//static void print(char *str_buf);           //	Initialized Actor to default Values
static void help(AMessage_st* s_Message_Rx);
//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);
static void init(void *a, void *b);
static void pingServer(AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
//static void Add_response_to_Tx_Queue(const char *Destactor, char *response, int16_t size, char *CmdFunc);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
//static void get_from_other_actor(const char* dest_Actor,const char* parameter);
static void Start_Web_Server(void *pvParameters __attribute__((unused)));
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
//static esp_err_t products_get_handler(httpd_req_t *req);
static esp_err_t index_html_get_handler(httpd_req_t *req);
static void periodic_timer_callback(void* arg);
static void Deinit_Actor(AMessage_st* s_Message_Rx);
//static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer);
//static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
PSRAM_ATTR_BSS static char ota_buff[2048];

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)
//static Actor_st s_Fileserver = { prop, get, set, init, print, monitor, help,     // UART Actor property parameter
//		&fileserver_Tx_queue, &fileserver_Rx_queue, getAll, 0, 0, 0, 0, 0 };

const esp_timer_create_args_t periodic_timer_args_new = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"
};
//static esp_err_t index_html_get_handler(httpd_req_t *req)
//{
//    //httpd_resp_set_status(req, "307 Temporary Redirect");
//    //httpd_resp_set_hdr(req, "Location", "/");
//	    extern const unsigned char favicon_ico_start1[] asm("_binary_upload_script_css_start");
//	    extern const unsigned char favicon_ico_end1[]   asm("_binary_upload_script_css_end");
//	    const size_t favicon_ico_size1 = (favicon_ico_end1 - favicon_ico_start1);
//	    //httpd_resp_set_type(req, "image/x-icon");
//	    httpd_resp_send(req, (const char *)favicon_ico_start1, favicon_ico_size1);
//        return ESP_OK;
//}

static esp_err_t config_html_get_handler(httpd_req_t *req)
{
    //httpd_resp_set_status(req, "307 Temporary Redirect");
    //httpd_resp_set_hdr(req, "Location", "/");
	    extern const unsigned char config_ico_start1[] asm("_binary_config_json_start");
	    extern const unsigned char config_ico_end1[]   asm("_binary_config_json_end");
	    const size_t config_ico_size1 = (config_ico_end1 - config_ico_start1);
	    //httpd_resp_set_type(req, "image/x-icon");
	    httpd_resp_send(req, (const char *)config_ico_start1, config_ico_size1);
    return ESP_OK;
}
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    //httpd_resp_set_status(req, "307 Temporary Redirect");
    //httpd_resp_set_hdr(req, "Location", "/");
	    extern const unsigned char favicon_ico_start1[] asm("_binary_upload_script_css_start");
	    extern const unsigned char favicon_ico_end1[]   asm("_binary_upload_script_css_end");
	    const size_t favicon_ico_size1 = (favicon_ico_end1 - favicon_ico_start1);
	    //httpd_resp_set_type(req, "image/x-icon");
	    httpd_resp_send(req, (const char *)favicon_ico_start1, favicon_ico_size1);
        return ESP_OK;
}
static esp_err_t index_html_get_handler1(httpd_req_t *req)
{
    //httpd_resp_set_status(req, "307 Temporary Redirect");
    //httpd_resp_set_hdr(req, "Location", "/");
	    extern const unsigned char favicon_ico_start1[] asm("_binary_server_js_start");
	    extern const unsigned char favicon_ico_end1[]   asm("_binary_server_js_end");
	    const size_t favicon_ico_size1 = (favicon_ico_end1 - favicon_ico_start1);
	    //httpd_resp_set_type(req, "image/x-icon");
	    httpd_resp_send(req, (const char *)favicon_ico_start1, favicon_ico_size1);
        return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_script1_js_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_script1_js_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    //httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
static esp_err_t favicon_get_handler1(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_script_js_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_script_js_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    //httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
static esp_err_t example_get_handler1(httpd_req_t *req)
{
    extern const unsigned char example_ico_start[] asm("_binary_example_jpg_start");
    extern const unsigned char example_ico_end[]   asm("_binary_example_jpg_end");
    const size_t favicon_ico_size = (example_ico_end - example_ico_start);
    //httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)example_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_timer_handle_t periodic_timer;

/* Handler to redirect incoming GET request for /cardinallogo.png to /
 * This can be overridden by uploading file with same name */
//static esp_err_t logo_get_handler(httpd_req_t *req)
//{
//    /* Get handle to embedded file upload script */
////    extern const unsigned char logo_start[] asm("_binary_cardinallogo_png_start");
////    extern const unsigned char logo_end[]   asm("_binary_cardinallogo_png_end");
////    const size_t logo_size = (logo_end - logo_start);
////    httpd_resp_set_type(req, "image/png");
////    httpd_resp_send(req, (const char*) logo_start, logo_size);
//
//    return ESP_OK;
//}
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}


static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
//    char entrypath[FILE_PATH_MAX];
//    char entrysize[16];
//    const char *entrytype;
//
//    struct dirent *entry;
//    struct stat entry_stat;
//
//    DIR *dir = opendir(dirpath);
//    const size_t dirpath_len = strlen(dirpath);
//
//    /* Retrieve the base path of file storage to construct the full path */
//    strlcpy(entrypath, dirpath, sizeof(entrypath));
//    //printf("http_resp_dir_html : %s,%s", dirpath,entrypath);
//    if (!dir) {
//        //ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
//        /* Respond with 404 Not Found */
//        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
//        return ESP_FAIL;
//    }
    /* Send HTML file header */
//       httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
//
//   // httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head>");
//
//        /* Send CSS file link */
//        httpd_resp_sendstr_chunk(req, "<link rel=\"stylesheet\" type=\"text/css\" href=\"/upload_script.css\">");
//
//        /* Send JavaScript file links */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/script1.js\"></script>");
//        httpd_resp_sendstr_chunk(req, "<script src=\"/script.js\"></script>");
//        //httpd_resp_sendstr_chunk(req, "<script src=\"/script2.js\"></script>");
//
//        /* Send server-side JavaScript file link */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/server.js\"></script>");
//
//        /* Send configuration JSON file link */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/config.json\"></script>");
//        httpd_resp_sendstr_chunk(req, "<script src=\"/example.jpg\"></script>");
//
//        httpd_resp_sendstr_chunk(req, "</head><body>");



    /* Get handle to embedded file upload script */
//    extern const unsigned char upload_script_start[] asm("_binary_upload_script1_html_start");
//    extern const unsigned char upload_script_end[]   asm("_binary_upload_script1_html_end");
//    const size_t upload_script_size = (upload_script_end - upload_script_start);
//
////    extern const unsigned char upload_script_start1[] asm("_binary_upload_script_css_start");
////       extern const unsigned char upload_script_end1[]   asm("_binary_upload_script_css_end");
////       const size_t upload_script_size1 = (upload_script_end1 - upload_script_start1);
//
//       /* Add file upload form and script which on execution sends a POST request to /upload */
//
//
//    /* Add file upload form and script which on execution sends a POST request to /upload */
//    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);



       extern const unsigned char upload_script_start2[] asm("_binary_upload_script2_html_start");
       extern const unsigned char upload_script_end2[]   asm("_binary_upload_script2_html_end");
       const size_t upload_script_size2 = (upload_script_end2 - upload_script_start2);

   //    extern const unsigned char upload_script_start1[] asm("_binary_upload_script_css_start");
   //       extern const unsigned char upload_script_end1[]   asm("_binary_upload_script_css_end");
   //       const size_t upload_script_size1 = (upload_script_end1 - upload_script_start1);

          /* Add file upload form and script which on execution sends a POST request to /upload */


       /* Add file upload form and script which on execution sends a POST request to /upload */
       httpd_resp_send_chunk(req, (const char *)upload_script_start2, upload_script_size2);
    //httpd_resp_send_chunk(req, (const char *)upload_script_start1, upload_script_size1);

    /* Send file-list table definition and column labels */
//      httpd_resp_sendstr_chunk(req,
//        "<table class=\"fixed\" border=\"1\">"
//        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
//        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
//        "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
//       while ((entry = readdir(dir)) != NULL) {
//        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
//
//        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
//        if (stat(entrypath, &entry_stat) == -1) {
//            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
//            continue;
//        }
//        sprintf(entrysize, "%ld", entry_stat.st_size);
//        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
//
//        /* Send chunk of HTML file containing table entries with file name and size */
//        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
//        httpd_resp_sendstr_chunk(req, req->uri);
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        if (entry->d_type == DT_DIR) {
//            httpd_resp_sendstr_chunk(req, "/");
//        }
//        httpd_resp_sendstr_chunk(req, "\">");
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        httpd_resp_sendstr_chunk(req, "</a></td><td>");
//        httpd_resp_sendstr_chunk(req, entrytype);
//        httpd_resp_sendstr_chunk(req, "</td><td>");
//        httpd_resp_sendstr_chunk(req, entrysize);
//        httpd_resp_sendstr_chunk(req, "</td><td>");
//        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
//        httpd_resp_sendstr_chunk(req, req->uri);
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
//        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
//    }
    //closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t http_resp_dir_html1(httpd_req_t *req, const char *dirpath)
{
//      char entrypath[FILE_PATH_MAX];
//      char entrysize[16];
//      const char *entrytype;
//
//      struct dirent *entry;
//      struct stat entry_stat;
//
//      DIR *dir = opendir(dirpath);
//      const size_t dirpath_len = strlen(dirpath);
//
//    /* Retrieve the base path of file storage to construct the full path */
//       strlcpy(entrypath, dirpath, sizeof(entrypath));
//       //printf("http_resp_dir_html11 : %s,%s", dirpath,entrypath);
//
//    if (!dir) {
//        //ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
//        /* Respond with 404 Not Found */
//        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
//        return ESP_FAIL;
//    }
    /* Send HTML file header */
//       httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
//
//   // httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head>");
//
//        /* Send CSS file link */
//        httpd_resp_sendstr_chunk(req, "<link rel=\"stylesheet\" type=\"text/css\" href=\"/upload_script.css\">");
//
//        /* Send JavaScript file links */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/script1.js\"></script>");
//        httpd_resp_sendstr_chunk(req, "<script src=\"/script.js\"></script>");
//        //httpd_resp_sendstr_chunk(req, "<script src=\"/script2.js\"></script>");
//
//        /* Send server-side JavaScript file link */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/server.js\"></script>");
//
//        /* Send configuration JSON file link */
//        httpd_resp_sendstr_chunk(req, "<script src=\"/config.json\"></script>");
//        httpd_resp_sendstr_chunk(req, "<script src=\"/example.jpg\"></script>");
//
//        httpd_resp_sendstr_chunk(req, "</head><body>");



    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script1_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script1_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

//    extern const unsigned char upload_script_start1[] asm("_binary_upload_script_css_start");
//       extern const unsigned char upload_script_end1[]   asm("_binary_upload_script_css_end");
//       const size_t upload_script_size1 = (upload_script_end1 - upload_script_start1);

       /* Add file upload form and script which on execution sends a POST request to /upload */


    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);



    //extern const unsigned char upload_script_start2[] asm("_binary_upload_script2_html_start");
     //  extern const unsigned char upload_script_end2[]   asm("_binary_upload_script2_html_end");
      // const size_t upload_script_size2 = (upload_script_end2 - upload_script_start2);

   //    extern const unsigned char upload_script_start1[] asm("_binary_upload_script_css_start");
   //       extern const unsigned char upload_script_end1[]   asm("_binary_upload_script_css_end");
   //       const size_t upload_script_size1 = (upload_script_end1 - upload_script_start1);

          /* Add file upload form and script which on execution sends a POST request to /upload */


       /* Add file upload form and script which on execution sends a POST request to /upload */
      // httpd_resp_send_chunk(req, (const char *)upload_script_start2, upload_script_size2);
    //httpd_resp_send_chunk(req, (const char *)upload_script_start1, upload_script_size1);

    /* Send file-list table definition and column labels */
//    httpd_resp_sendstr_chunk(req,
//        "<table class=\"fixed\" border=\"1\">"
//        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
//        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
//        "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
//    while ((entry = readdir(dir)) != NULL) {
//        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
//
//        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
//        if (stat(entrypath, &entry_stat) == -1) {
//            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
//            continue;
//        }
//        sprintf(entrysize, "%ld", entry_stat.st_size);
//        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
//
//        /* Send chunk of HTML file containing table entries with file name and size */
//        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
//        httpd_resp_sendstr_chunk(req, req->uri);
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        if (entry->d_type == DT_DIR) {
//            httpd_resp_sendstr_chunk(req, "/");
//        }
//        httpd_resp_sendstr_chunk(req, "\">");
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        httpd_resp_sendstr_chunk(req, "</a></td><td>");
//        httpd_resp_sendstr_chunk(req, entrytype);
//        httpd_resp_sendstr_chunk(req, "</td><td>");
//        httpd_resp_sendstr_chunk(req, entrysize);
//        httpd_resp_sendstr_chunk(req, "</td><td>");
//        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
//        httpd_resp_sendstr_chunk(req, req->uri);
//        httpd_resp_sendstr_chunk(req, entry->d_name);
//        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
//        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
//    }
   // closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
//static esp_err_t index_html_get_handler(httpd_req_t *req)
//{
//    /* Get handle to embedded file upload script */
//    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
//    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
//    const size_t upload_script_size = (upload_script_end - upload_script_start);
//    httpd_resp_set_type(req, "text/html");
//    httpd_resp_send(req, (const char*) upload_script_start, upload_script_size);
//
//    return ESP_OK;
//}

//static esp_err_t products_get_handler(httpd_req_t *req)
//{
//	extern const unsigned char products_start[] asm("_binary_products_json_start");
//	extern const unsigned char products_end[]   asm("_binary_products_json_end");
//	const size_t products_size = (products_end - products_start);
//    httpd_resp_set_type(req, "text/json");
//    httpd_resp_send(req, (const char*) products_start, products_size);
//
//    return ESP_OK;
//}


//static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
//	uint8_t parameter_found = 0; // Flag to check if actor is found
//	char str[100] ={0};
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//	for (int i = 0; i < no_of_elements; i++) {
//		if (!strcmp(property, prop[i].str_name)) {
//
//			if (!strcmp(prop[i].access, "RW")) {
//
//			parameter_found = 1; // Set flag to indicate actor is found
//			switch (prop[i].type) {
//
//			case U_INT8:
//				*(uint8_t*) prop[i].name = atoi(value);
//				break;
//
//			case U_INT16:
//				*(uint16_t*) prop[i].name = atoi(value);
//				break;
//
//			case U_INT32:
//				*(uint32_t*) prop[i].name = atoi(value);
//				break;
//
//			case INT:
//				*(int*) prop[i].name = atoi(value);
//				break;
//			case FLOAT:
//				*(float*) prop[i].name = atof(value);
//				break;
//
//			case STRING:
//				strcpy((char*) prop[i].name, value);
//				break;
//
//			default:
//				break;
//			}
//		}
//		else
//		{
//			return 2;
//		}
//	  }
//	}
//	if(parameter_found)
//		return 1;
//	else
//		return 0;
//}//	set

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

			if(!(strcmp(prop[i].str_name, "CONN_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "WEB SERVER NOT CONNECTED");
					break;

				case 1:
					strcpy(val_a8, "WEB SERVER CONNECTED");
					break;

				default:
					break;
				}
			}
		}
	}
//	printf("E404! \r\n");
}//	get

//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx){
//
//	cJSON *out_JSON  = cJSON_CreateObject();
//	//no of elements
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//	for (int i = 0; i < no_of_elements; i++) {
//
//		switch (prop[i].type) {
//		case U_INT8:
//			sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
//			break;
//
//		case U_INT16:
//			sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
//			break;
//
//		case U_INT32:
//			sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
//			break;
//
//		case INT:
//			sprintf(val_a8, "%d", *(int*) prop[i].name);
//			break;
//
//		case FLOAT:
//			sprintf(val_a8, "%f", *(float*) prop[i].name);
//			break;
//
//		case STRING:
//			strcpy(val_a8, prop[i].name);
//			break;
//
//		default:
//			break;
//		}
//		if(!(strcmp(prop[i].str_name, "CONN_STATUS")))
//		{
//			switch (*(char*)prop[i].name)
//			{
//			case 0:
//				strcpy(val_a8, "WEB SERVER NOT CONNECTED");
//				break;
//
//			case 1:
//				strcpy(val_a8, "WEB SERVER CONNECTED");
//				break;
//
//			default:
//				break;
//			}
//		}
//		cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
//	}
//	if(s_Message_Rx->payload_p8!= NULL)
//	{
//		cJSON_free(s_Message_Rx->payload_p8);
//		s_Message_Rx->payload_p8 = NULL;
//	}
//	s_Message_Rx->payload_p8 = (uint8_t*)cJSON_PrintUnformatted(out_JSON);
//	console_send_responce_to_console_xface(s_Message_Rx);
//	cJSON_Delete(out_JSON);
//}	//	getAll

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[32] = {0};
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
			if(!(strcmp(prop[i].str_name, "CONN_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "WEB SERVER NOT CONNECTED");
					break;

				case 1:
					strcpy(val_a8, "WEB SERVER CONNECTED");
					break;

				default:
					break;
				}
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
		payLoadData[0] = '\0';
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the FILE_SERVER actor.");
	cJSON_AddStringToObject(responseObject, "SET(string FILE_NAME)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(FILE_NAME)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "START()", "Start the file server.");
	cJSON_AddStringToObject(responseObject, "SERVER_STOP()", "Stop the file server.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	payLoadData[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void monitor(void *pvParameters __attribute__((unused))) {
//	cJSON *in_JSON;
//	cJSON *out_JSON;
//	cJSON *name_JSON = NULL;
//	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
//	uint8_t *val_p8  = NULL;
//	char str[100] = {0};
//	char Rx_buffer_used = 0;
//	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
//		char Rx_buffer[8192];
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(fileserver_Rx_queue, (void*) (s_Message_Rx),portMAX_DELAY)) {         // Uart Tx queue Monitor
//			Rx_buffer_used = 0;
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			    if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			    {                 // Init

					if (FirstEntry == 0) {
						init(0,0);
					}
					else
					{
						Add_Response_msg("File Server actor is initialized",s_Message_Rx, payLoadData);
					}
				}
			    else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET"))
			    {            // Get Actor Properties
					Get_Property(s_Message_Rx);

				}
//			    else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
//				{
//					name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
//					if (name_JSON == NULL) {
//						sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//						Add_Response_msg(str,s_Message_Rx, payLoadData);
//					    }
//					else{
//					head_JSON = name_JSON->child;
//					cJSON *root_JSON  = cJSON_CreateObject();
//					cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/WEB_SERVER.json");
//				   // Loop through each key-value pair
//					do {
//						// Check if the value string is not NULL
//						if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
//						{
//							// Set the key-value pair
//							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
//							if(u8Result==1)
//							{
//							cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
//							}
//							else if(u8Result==2){
//								sprintf(str,"'%s' is a read only property", head_JSON->string);
//								 Add_Response_msg(str, s_Message_Rx, payLoadData);
//							}
//							else{
//							cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
//							}
//						} else {
//							// Handle the case where value string is NULL (e.g., log an error or take appropriate action)
//							sprintf(str, "Invalid parameter '%s'", head_JSON->string);
//							Add_Response_msg(str,s_Message_Rx, payLoadData);
//							// Handle the error as per your application's requirements
//						}
//						head_JSON = head_JSON->next;
//					} while (head_JSON != 0);
//
//					if(u8Result==1){
//					//  save parameters to JFS
//					payLoadData[0] = '\0';
//					cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
//					Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
//					cJSON_Delete(root_JSON);
//					console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
//					}
//					// Free the parsed JSON
//					cJSON_Delete(name_JSON);
//				}
//				}
//			    else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			    {
//					val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//					getAll(prop, (char*) val_p8,s_Message_Rx);
//					free(val_p8);
//				}
			    else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "START"))
			    {
					 Web_Server_operation = 1;
//					 Rx_buffer_used=1;
					 if(ServerHandle == NULL)
					 {
						// printf("\n START web server");
						 Start_server = xTaskCreatePinnedToCore(Start_Web_Server, (const char*) "Web Server", START_WEBSERVER_TASK_STACK_DEPTH, Rx_buffer, START_WEBSERVER_TASK_PRIORITY, &ServerHandle, 0);
					 }
					else
					 {
						// printf("\nTask for START command is already created");
//						Rx_buffer_used = 0;
						Web_Server_operation = 0;
						Add_Response_msg("Task for START command is already created.", s_Message_Rx, payLoadData);
					 }
					 /* Start the file server */
//					printf("\n Start file server");
//					if((WIFI_status == 2) || (WIFI_status == 3))
//					{
//						 ESP_ERROR_CHECK(start_file_server("nor:0:\\JFS\\" ));  //spiffs
//						 console_send_responce_to_console_xface(MSG_STR,THIS_ACTOR,"Server is started..");
//					}
//					else
//					{
//						get_from_other_actor("CONSOLE", "WIFI_STATUS");
////						get_from_other_actor("WIFI", "STATUS");
//						vTaskDelay(1000/ portTICK_PERIOD_MS);
//						if((WIFI_status == 2) || (WIFI_status == 3))
//						{
//							 ESP_ERROR_CHECK(start_file_server("nor:0:\\JFS\\" ));  //spiffs
//							 console_send_responce_to_console_xface(MSG_STR,THIS_ACTOR,"Server is started..");
//						}
//
//						else
//							console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Kindly connect to WIFI at first.");
//					}


				}
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SERVER_STOP"))
				{
					httpd_stop(server);
					Add_Response_msg("Server is stopped..",s_Message_Rx, payLoadData);
					vTaskDelete(ServerHandle);
					ServerHandle = NULL;
					//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Web server is stopped.");
				}
				 else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
				 {
					get_actor_properties(s_Message_Rx);
				  }
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "PING")){
					//if((WIFI_status == 2) || (WIFI_status == 3))
					   if(NET_status == E_NET_CONNECTED)
						{
							pingServer(s_Message_Rx);
						}
					else
						Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData);
						//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Kindly connect to WIFI at first.");

				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
					Analyse_Response(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
					help(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DEINIT"))
				{
					Add_Response_msg("WEB SERVER DEINIT method is received", s_Message_Rx, payLoadData);
					Deinit_Actor(s_Message_Rx);
				}
				else
				{
					//File server error message: invalid method
					Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
				}
				// Free s_MyMessage.payload_p8 here
//				if(s_Message_Rx->payload_size!=0)
//				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
//			if (Rx_buffer_used == 0)
//			{
//			  if(s_Message_Rx->payload_p8 !=  NULL)
//			  {
//				console_MessageRelease_xface((char*) s_Message_Rx->payload_p8);
//			  }
//				if(Rx_buffer != NULL)
//				{
//					free(Rx_buffer);
//					Rx_buffer = NULL;
//				}
//
//			}
		}
		//vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
//esp_err_t OTA_update_status_handler(httpd_req_t *req)
//{
//
//	char ledJSON[200];
//	char content[1000];
//
//	/* Truncate if content length larger than the buffer */
//	size_t recv_size = MIN(req->content_len, sizeof(content));
//
//	int ret = httpd_req_recv(req, content, recv_size);
//	if (ret <= 0) { /* 0 return value indicates connection closed */
//		/* Check if timeout occurred */
//		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//			/* In case of timeout one can choose to retry calling
//			 * httpd_req_recv(), but to keep it simple, here we
//			 * respond with an HTTP 408 (Request Timeout) error */
//			httpd_resp_send_408(req);
//		}
//		return ESP_FAIL;
//	}
//	//ESP_LOGI("OTA", "Status Requested");
//
//	//sprintf(ledJSON, "{\"status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", flash_status, __TIME__, __DATE__);
//	//strcpy(content,ledJSON);
//	httpd_resp_set_status(req, "200 OK");
//	httpd_resp_set_type(req, "text/json");
//	httpd_resp_send(req, (const char*) content, strlen(content));
//	//ESP_LOGI("OTA","%s", content);
//	vTaskDelay(400);
//
//	// This gets set when upload is complete
//	//if (flash_status == 1)
//	{
//		// We cannot directly call reboot here because we need the
//		// browser to get the ack back.
//	//	xEventGroupSetBits(reboot_event_group, REBOOT_BIT);
//		esp_restart();
//	}
//
//	return ESP_OK;
//}

esp_err_t OTA_update_post_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;
//	int64_t start, stop;
	char payLoadData_OTA[200] = {0}; //ota_buff[2048],
	char str[200] = {0}, count = 0;
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
//	char* buffer = heap_caps_calloc (100,sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char fm_version[100] = {0};
	int retry_count = 3, retry = 0;
	esp_err_t err;

//	start = esp_timer_get_time();
	do
	{
		/* Read the data for the request */
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
		{
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				//ESP_LOGI("OTA", "Socket Timeout");
				/* Retry receiving if timeout occurred */
				continue;
			}
			//ESP_LOGI("OTA", "OTA Other Error %d", recv_len);
			return ESP_FAIL;
		}

		//printf("OTA RX: %d of %d\r", content_received, content_length);

	    // Is this the first data we are receiving
		// If so, it will have the information in the header we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Lets find out where the actual data staers after the header info
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			//int body_part_sta = recv_len - body_part_len;
//			printf("OTA File Size: %d : Start Location:%d - End Location:%d\r\n", content_length, body_part_sta, body_part_len);
//			printf("OTA File Size: %d\r\n", content_length);
			esp_app_desc_t new_app_info;
			if (body_part_len > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
			{
				// check current version with downloading
				memcpy(&new_app_info, &body_start_p[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
//				sprintf(str,"New firmware version: %s\n",new_app_info.version);
//				Add_Response_msg(str,req->user_ctx);

                if(strncmp(new_app_info.version,"700",3) != 0)   // Check if new binary file is for Haven X- series
                {
//                    	printf("\n\n new_app_info.version = %s \n\n",new_app_info.version);
                	Add_Response_msg("Error! New firmware is not for Haven X- series",req->user_ctx, payLoadData_OTA);
                	return ESP_FAIL;
                }
			}

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				//printf("Error With OTA Begin, Cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{

			//	printf("Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);
			}

//            // Disable interrupts and events
//			{
////				Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "DEINIT");
//				cJSON *my_JSON = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
//				cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 0);
//				char* payload = cJSON_PrintUnformatted(my_JSON);
//				//printf("\n Record_payload = %s", Record_payload);
//				Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payload, strlen(payload), "SETSERVCONNFLAG");
//				cJSON_Delete(my_JSON); // Free the cJSON object
//				free(payload); // Free the memory allocated by cJSON_PrintUnformatted
//
//				cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
//				cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 0);
//				char* payload1 = cJSON_PrintUnformatted(my_JSON1);
//				//printf("\n Record_payload = %s", Record_payload);
//				Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payload1, strlen(payload1), "SETEVTTASKFG");
//				cJSON_Delete(my_JSON1); // Free the cJSON object
//				free(payload1); // Free the memory allocated by cJSON_PrintUnformatted
//
//				uint8_t blink_state_val = LED_BLINK_STATE_CHASE_MODE;
//				set_to_other_actor("LED", U_INT8, "BLINK_STATE",&blink_state_val);
//				Add_Response_msg("OTA is in progress. Please wait....",req->user_ctx);
//				vTaskDelay(3000/portTICK_PERIOD_MS);
//		  	  }

//        	cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
//        	cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);

			for(int i= 0; i<strlen(new_app_info.version); i++)
			{
			   if(new_app_info.version[i]=='_')
				   fm_version[i] = '.';
			   else
				   fm_version[i] = new_app_info.version[i];
			}
//        	cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", fm_version);
//        	char* payload = cJSON_PrintUnformatted(object_JSON);
//        	cJSON_Delete(object_JSON);
//        	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS",payload,strlen(payload),"WR_PARA");
//        	free(payload);

        	sprintf(str,"New firmware version: %s",fm_version);
        	Add_Response_msg(str,req->user_ctx, payLoadData_OTA);
//        	free(fm_version);

    		cJSON *responseObject = cJSON_CreateObject();
    		// Add stateName to the JSON object
    		cJSON_AddStringToObject(responseObject, "stateName", "Download");
    		// Add duration to the JSON object
    		cJSON_AddNumberToObject(responseObject, "duration", -1);
    		// Print the JSON object as a string
			payLoadData[0] = '\0';
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
    		// Send the JSON string to the other actor
    		Send_CMD_To_Other_Actor(LED, "LED", payLoadData, strlen(payLoadData), "SETSTATE");
    		// Free the allocated memory
    		cJSON_Delete(responseObject);

			// Lets write this first part of data out
			esp_ota_write(ota_handle, body_start_p, body_part_len);
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);

			content_received += recv_len;
            count++;
            if(count > 100)
            {
            	count = 0;
            	sprintf(str,"Firmware update progress:  %0.2f %%", ((float)((float)content_received/(float)content_length)*100));
            	Add_Response_msg(str,req->user_ctx, payLoadData_OTA);
            }

		}

	} while (recv_len > 0 && content_received < content_length);
//	stop = esp_timer_get_time();

	//printf("\n current time = %lld", esp_timer_get_time());
	//printf("time required to download the file is: %lld msec\n", ((stop-start)/1000));
	//Add_Response_msg(str,s_Message_Rx);
	//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, str);
//	Send_CMD_To_Other_Actor(LED,"LED", "\0", 0, "CHASE_STOP");
//	Send_CMD_To_Other_Actor(LED,"LED", "\0", 0, "BLINK_STOP");

	// End response
	//httpd_resp_send_chunk(req, NULL, 0);

//	if (esp_ota_end(ota_handle) == ESP_OK)
//	{
//		// Lets update the partition
//		if(esp_ota_set_boot_partition(update_partition) == ESP_OK)
//		{
//			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
//
//			// Webpage will request status when complete
//			// This is to let it know it was successful
//			//flash_status = 1;
//			cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
//        	cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
//        	cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", fm_version);
//        	char* payload = cJSON_PrintUnformatted(object_JSON);
//        	cJSON_Delete(object_JSON);
//        	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS",payload,strlen(payload),"WR_PARA");
//        	free(payload);
//           	free(fm_version);
//           	Add_Response_msg("Preparing to restart system...",req->user_ctx);
//           	vTaskDelay(1000/portTICK_PERIOD_MS);
//
//			esp_restart();
//			//ESP_LOGI("OTA", "Next boot partition subtype %d at offset 0x%x", boot_partition->subtype, boot_partition->address);
//			//ESP_LOGI("OTA", "Please Restart System...");
//		}
//		else
//		{
//			Add_Response_msg("Error in updating OTA partition.",req->user_ctx);
//			free(fm_version);
//	        Add_Response_msg("Preparing to restart system...",req->user_ctx);
//	       	vTaskDelay(1000/portTICK_PERIOD_MS);
//	        esp_restart();
//
//			return ESP_FAIL;
//		}
//	}
//	else
//    {
//		Add_Response_msg("Error in ending OTA operation.",req->user_ctx);
//		free(fm_version);
//
//        Add_Response_msg("Preparing to restart system...",req->user_ctx);
//       	vTaskDelay(1000/portTICK_PERIOD_MS);
//        esp_restart();
//
//		return ESP_FAIL;
//	}

	for (retry = 0; retry < retry_count; retry++) {
	        err = esp_ota_end(ota_handle);
	        if (err == ESP_OK) {
	            break;
	        } else if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
	            Add_Response_msg("Image validation failed, image is corrupted.", req->user_ctx, payLoadData_OTA);
	           // task_fatal_error();
	        } else {
	            sprintf(str, "esp_ota_end failed (%s). Retrying...", esp_err_to_name(err));
	            Add_Response_msg(str, req->user_ctx, payLoadData_OTA);
	        }
	        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay before retrying
	    }
    if (retry == retry_count) {
        Add_Response_msg("Failed to complete OTA after multiple attempts.", req->user_ctx, payLoadData_OTA);
//        task_fatal_error();
        Add_Response_msg("Preparing to restart system...",req->user_ctx, payLoadData_OTA);
        Restart_ESP_Xface(1);
//       	vTaskDelay(1000/portTICK_PERIOD_MS);
//        esp_restart();
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        sprintf(str, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        Add_Response_msg(str,req->user_ctx, payLoadData_OTA);
        Add_Response_msg("Preparing to restart system...",req->user_ctx, payLoadData_OTA);
        Restart_ESP_Xface(1);
//       	vTaskDelay(1000/portTICK_PERIOD_MS);
//        esp_restart();
//        task_fatal_error();
    }
   	cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
    cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
   	cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", fm_version);
	payLoadData[0] = '\0';
	cJSON_PrintPreallocated(object_JSON, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(object_JSON);
	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS",payLoadData,strlen(payLoadData),"WR_PARA");

    Add_Response_msg("Preparing to restart system...",req->user_ctx, payLoadData_OTA);
    Restart_ESP_Xface(1);
//	vTaskDelay(1000/portTICK_PERIOD_MS);
//    esp_restart();

	return ESP_OK;
}

static void pingServer(AMessage_st* s_Message_Rx)
{
	//char *TARGET_HOST = "www.espressif.com";
	char *TARGET_HOST = "www.google.com";
//	printf("target host is %s\r\n",TARGET_HOST);
	if (initialize_ping(10000, 2, TARGET_HOST) == ESP_OK) {
		Add_Response_msg("initialize_ping success",s_Message_Rx, payLoadData);
		//(MSG_STR, THIS_ACTOR, "initialize_ping success");
	} else {
		Add_Response_msg("initialize_ping fail",s_Message_Rx, payLoadData);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "initialize_ping fail");
	}
}//	pingServer

// Create a queue with storage in PSRAM
//static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer)
//{
//    QueueHandle_t xQueue = NULL;
//
//    // Allocate the queue storage area in PSRAM
//    *pucQueueStorage = (uint8_t *)heap_caps_malloc((uxQueueLength * uxItemSize), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//    if (*pucQueueStorage == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("\n Failed to allocate queue storage in PSRAM \n");
//#endif
//        return NULL;
//    }
//    memset(*pucQueueStorage,0,(uxQueueLength * uxItemSize));
//
//    // Allocate the queue structure itself in PSRAM
//    *pxQueueBuffer = (StaticQueue_t *)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//    if (*pxQueueBuffer == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("Failed to allocate queue structure in PSRAM\n");
//#endif
//        heap_caps_free(*pucQueueStorage);
//		*pucQueueStorage = NULL;
//        return NULL;
//    }
//    memset(*pxQueueBuffer,0,sizeof(StaticQueue_t));
//    // Create the queue with custom storage
//    xQueue = xQueueCreateStatic(uxQueueLength, uxItemSize, *pucQueueStorage, *pxQueueBuffer);
//
//    if (xQueue == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("Failed to create queue in PSRAM \n");
//#endif
//        heap_caps_free(*pucQueueStorage);
//        heap_caps_free(*pxQueueBuffer);
//		*pucQueueStorage = NULL;
//        *pxQueueBuffer = NULL;
//    }
//    return xQueue;
//}
static void init(void *a, void *b){

	// Calculate the total size of memory required for the stack array
	//size_t stack_size = WEBSERVER_TASK_STACK_DEPTH * sizeof(StackType_t);
	//char JFS_dir[] = "{\"dirname\":\"JFS\"}";
	if (FirstEntry == 0)
	{
		FirstEntry = 1;
		s_Para.connectStatus_u8 = 0; // web server not connected.
		if(fileserver_Rx_queue == NULL)
		{
//			fileserver_Rx_queue = xQueueCreateInPSRAM(OBJ_QUE_COUNT, sizeof(AMessage_st), &Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
			fileserver_Rx_queue = xQueueCreateStatic(OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

			if(fileserver_Rx_queue == NULL)
			{
	#ifdef ENABLE_PRINT_MSG
				printf("Web server RX Queue is not created.");
	#endif
			}
		}

//		fileserver_Rx_queue = xQueueCreate(OBJ_QUE_COUNT, sizeof(AMessage_st));    //create a uart_Rx_queue queue.
//		if(fileserver_Rx_queue == NULL)
//		{
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Fileserver_Rx_queue is not created");
//		}

		// Create proc_uart Tx queue
//		fileserver_Tx_queue = xQueueCreate(OBJ_QUE_COUNT, sizeof(s_Message_Tx));    //create a uart_Tx_queue queue.
//		if(fileserver_Tx_queue == NULL)
//		{
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Fileserver_Tx_queue is not created");
//		}

//		       xTaskStack = (StackType_t *)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//				if (xTaskStack == NULL) {
//				    // Memory allocation failed
//#ifdef ENABLE_PRINT_MSG
//				    printf( "Failed to allocate memory for the task stack");
//#endif
//				    // Handle error
//				    return;
//				}

				file_serverHandle = xTaskCreateStaticPinnedToCore(
					    monitor,                 // Task function
						"File Server Monitor",            // Task name
						WEBSERVER_TASK_STACK_DEPTH,        // Stack size in words
					    NULL,                    // Task parameters (not used here)
						WEBSERVER_TASK_PRIORITY,                       // Task priority
					    xTaskStack,              // Pointer to task stack (allocated in PSRAM)
					    &xWEBSERVERTaskBuffer,             // Pointer to task control block
						0
					);
				if (file_serverHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
					    printf("Failed to create task\n");
#endif
					    // Handle error
					}

		//xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
		//file_serverMonitor = xTaskCreate(monitor, (const char*) "File Server Monitor", 6*1024, NULL, 2, &file_serverHandle);

		ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args_new, &periodic_timer));  // create periodic timer
//		strcpy(out, "{\"filename\":\"A:/System/Server.json\"}");
//		Add_response_to_Tx_Queue("FILE_SYSTEM", out, strlen(out), "READ");

		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/WEB_SERVER.json");
		payLoadData[0] = '\0';
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		cJSON_Delete(responseObject);

		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
//		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "File Server actor is initialized");
	}
}

//static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
//{
//    if (IS_FILE_EXT(filename, ".pdf")) {
//        return httpd_resp_set_type(req, "application/pdf");
//    } else if (IS_FILE_EXT(filename, ".html")) {
//        return httpd_resp_set_type(req, "text/html");
//    } else if (IS_FILE_EXT(filename, ".jpeg")) {
//        return httpd_resp_set_type(req, "image/jpeg");
//    } else if (IS_FILE_EXT(filename, ".ico")) {
//        return httpd_resp_set_type(req, "image/x-icon");
//    }
//    /* This is a limited set only */
//    /* For any other type always set as plain text */
//    return httpd_resp_set_type(req, "text/plain");
//}
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
//    if (!filename) {
//       // ESP_LOGE(TAG, "Filename is too long");
//        /* Respond with 500 Internal Server Error */
//        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
//        return ESP_FAIL;
//    }



    //printf( "download file path  : %s,%s\r\n", filepath,filename);
    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
    	// printf( "file path ny anita : %s,%s", filepath,filename);
        return http_resp_dir_html(req, filepath);
    }

//    if (strcmp(filename,"/updates")==0)
    if (strcasecmp(filename,"/updates")==0)
    		{
    	// printf( "file path1111");
    	   return http_resp_dir_html1(req, "");
    		}

    if (stat(filepath, &file_stat) == -1) {

    	           /* If file not present on SPIFFS check if URI
    	            * corresponds to one of the hardcoded paths */
//    	           if (strcmp(filename, "/upload_script.css") == 0) {
    	           if (strcasecmp(filename, "/upload_script.css") == 0) {
    	               return index_html_get_handler(req);
//    	           } else if (strcmp(filename, "/script1.js") == 0) {
    	           } else if (strcasecmp(filename, "/script1.js") == 0) {
    	               return favicon_get_handler(req);
    	           }
//    	           else if (strcmp(filename, "/config.json") == 0) {
    	           else if (strcasecmp(filename, "/config.json") == 0) {
    	               	               return config_html_get_handler(req);
    	               	           }
//    	           else if (strcmp(filename, "/server.js") == 0) {
    	           else if (strcasecmp(filename, "/server.js") == 0) {
    	               	               return index_html_get_handler1(req);
    	               	           }
//    	           else if (strcmp(filename, "/script.js") == 0) {
    	           else if (strcasecmp(filename, "/script.js") == 0) {
    	              	               	               return favicon_get_handler1(req);
    	              	               	           }


//    	           else if (strcmp(filename, "/example.jpg") == 0) {
    	           else if (strcasecmp(filename, "/example.jpg") == 0) {
    	               	              	               	               return example_get_handler1(req);
    	               	              	               	           }

        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
//        if (strcmp(filename, "/index.html") == 0) {
//            return index_html_get_handler(req);
//        } else if (strcmp(filename, "/favicon.ico") == 0) {
//            return favicon_get_handler(req);
//        }

        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

//    fd = fopen(filepath, "r");
//    if (!fd) {
//        //ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
//        /* Respond with 500 Internal Server Error */
//        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
//        return ESP_FAIL;
//    }
//
//   // ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
//    set_content_type_from_file(req, filename);
//
//    /* Retrieve the pointer to scratch buffer for temporary storage */
//    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
//    size_t chunksize;
//    do {
//        /* Read file in chunks into the scratch buffer */
//        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
//
//        if (chunksize > 0) {
//            /* Send the buffer contents as HTTP response chunk */
//            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
//                fclose(fd);
//                ///ESP_LOGE(TAG, "File sending failed!");
//                /* Abort sending file */
//                httpd_resp_sendstr_chunk(req, NULL);
//                /* Respond with 500 Internal Server Error */
//                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
//               return ESP_FAIL;
//           }
//        }
//
//        /* Keep looping till the whole file is sent */
//    } while (chunksize != 0);
//
//    /* Close file after sending complete */
//    fclose(fd);
   // ESP_LOGI(TAG, "File sending complete");
//
//    /* Respond with an empty chunk to signal HTTP response completion */
//#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
//    httpd_resp_set_hdr(req, "Connection", "close");
//#endif
//    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

//
//static esp_err_t updates_get_handler(httpd_req_t *req)
//{
//    char filepath[FILE_PATH_MAX];
//    FILE *fd = NULL;
//    struct stat file_stat;
//
//    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
//                                             req->uri, sizeof(filepath));
////                   if (!filename)
////                   {
////                   //ESP_LOGE(TAG, "Filename is too long");
////        /* Respond with 500 Internal Server Error */
////                   httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
////                    return ESP_FAIL;
////                   }
//
//
//    printf( "updates_get_handler  : %s", filepath);
//
//    /* If name has trailing '/', respond with directory contents */
//                  if (filename[strlen(filename) - 1] == '/')
//                  {
//                   return http_resp_dir_html1(req, filepath);
//                  }
//
//                   if (stat(filepath, &file_stat) == -1) {
//
//    	           /* If file not present on SPIFFS check if URI
//    	            * corresponds to one of the hardcoded paths */
//    	           if (strcmp(filename, "/upload_script.css") == 0) {
//    	               return index_html_get_handler(req);
//    	           } else if (strcmp(filename, "/script1.js") == 0) {
//    	               return favicon_get_handler(req);
//    	           }
//    	           else if (strcmp(filename, "/config.json") == 0) {
//    	               	               return config_html_get_handler(req);
//    	               	           }
//    	           else if (strcmp(filename, "/server.js") == 0) {
//    	               	               return index_html_get_handler1(req);
//    	               	           }
//    	           else if (strcmp(filename, "/script.js") == 0)
//
//    	           {
//
//    	            return favicon_get_handler1(req);
//    	           }
//    	           else if (strcmp(filename, "/example.jpg") == 0)
//    	           {
//    	             return example_get_handler1(req);
//    	          }
//
//        /* If file not present on SPIFFS check if URI
//         * corresponds to one of the hardcoded paths */
////        if (strcmp(filename, "/index.html") == 0) {
////            return index_html_get_handler(req);
////        } else if (strcmp(filename, "/favicon.ico") == 0) {
////            return favicon_get_handler(req);
////        }
//       //printf( "Failed to stat file1111111 : %s", filepath);
//        /* Respond with 404 Not Found */
//        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
//        return ESP_FAIL;
//    }
//
//    fd = fopen(filepath, "r");
//    if (!fd) {
//       // ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
//        /* Respond with 500 Internal Server Error */
//        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
//        return ESP_FAIL;
//    }
//
//          // ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
//           set_content_type_from_file(req, filename);
//
//    /* Retrieve the pointer to scratch buffer for temporary storage */
//       char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
//       size_t chunksize;
//        do {
//        /* Read file in chunks into the scratch buffer */
//        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
//
//            if (chunksize > 0)
//            {
//            /* Send the buffer contents as HTTP response chunk */
//                 if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
//                fclose(fd);
//                //ESP_LOGE(TAG, "File sending failed!");
//                /* Abort sending file */
//                httpd_resp_sendstr_chunk(req, NULL);
//                /* Respond with 500 Internal Server Error */
//                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
//               return ESP_FAIL;
//           }
//          }
//
//        /* Keep looping till the whole file is sent */
//      } while (chunksize != 0);
//
//    /* Close file after sending complete */
//    fclose(fd);
//   // ESP_LOGI(TAG, "File sending complete");
//
//    /* Respond with an empty chunk to signal HTTP response completion */
//#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
//    httpd_resp_set_hdr(req, "Connection", "close");
//#endif
//    httpd_resp_send_chunk(req, NULL, 0);
//    return ESP_OK;
//}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        //ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
       // ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > 200*1024) {
       // ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
    	httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
    	                            "File size must be less than 200 kb "
    	                           );
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        //ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    //ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        //ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            //ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            //ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    //ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}


/* Function to start the file server */
esp_err_t start_file_server(const char *base_path, AMessage_st* s_Message_Rx)
{
	static struct file_server_data *server_data = NULL;

    if (server_data) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = heap_caps_calloc(1, sizeof(struct file_server_data),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!server_data) {
    	Add_Response_msg("Failed to allocate memory for server data",s_Message_Rx, payLoadData);
       // console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
 //   strlcpy(server_data->base_path, base_path, sizeof(server_data->base_path));

//    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
     config.stack_size=1024 * 5;  //10240  Note:ESP FOTA working for 3K and not working for 2K stack size.
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    //ESP_LOGI(aTag, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
    	Add_Response_msg("Failed to start file server!",s_Message_Rx, payLoadData);
       // console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Failed to start file server!");
        return ESP_FAIL;
    }

    s_Para.connectStatus_u8 = 1; // web server is connected.
    httpd_uri_t file_download = {
           .uri       = "/*",  // Match all URIs of type /path/to/file
           .method    = HTTP_GET,
           .handler   = download_get_handler,
           .user_ctx  = server_data    // Pass server data as context
       };
       httpd_register_uri_handler(server, &file_download);

       /* URI handler for uploading files to server */
       httpd_uri_t file_upload = {
           .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
           .method    = HTTP_POST,
           .handler   = upload_post_handler,
           .user_ctx  = server_data    // Pass server data as context
       };
       httpd_register_uri_handler(server, &file_upload);

       /* URI handler for deleting files from server */

      // httpd_register_uri_handler(server, &file_delete);
//       httpd_uri_t file_updates = {
//               .uri       = "/updates",   // Match all URIs of type /delete/path/to/file
//               .method    = HTTP_POST,
//               .handler   = updates_get_handler,
//               .user_ctx  = server_data    // Pass server data as context
//
//
//
//           };
//           httpd_register_uri_handler(server, &file_updates);


       httpd_uri_t OTA_update = {
       					.uri = "/update",
       					.method = HTTP_POST,
       					.handler = OTA_update_post_handler,
       					.user_ctx = s_Message_Rx //NULL
       				};
       httpd_register_uri_handler(server, &OTA_update);



//       httpd_uri_t OTA_status = {
//       					.uri = "/status",
//       					.method = HTTP_POST,
//       					.handler = OTA_update_status_handler,
//       					.user_ctx = NULL
//       				};
//       				httpd_register_uri_handler(server, &OTA_status);


//     				 httpd_uri_t logo_png = {
//     				     					.uri = "/cardinallogo.png",
//     				     					.method = HTTP_GET,
//     				     					.handler = logo_get_handler,
//     				     					.user_ctx = NULL
//     				     				};
//     				     				httpd_register_uri_handler(server, &logo_png);

        free(server_data);
		return ESP_OK;
}

void WEB_SERVER_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;

	if (FirstEntry == false)
	{
//		if(xTaskStack != NULL)
//		{
//			heap_caps_free(xTaskStack);  //free(xTaskStack);- can be used, both have same effect
//			xTaskStack = NULL;
//		}
//		memset(&xWEBSERVERTaskBuffer, 0, sizeof(xWEBSERVERTaskBuffer));
		init(0,0);
	}
	uint8_t state = xQueueSend(fileserver_Rx_queue, s_Message, QUE_DELAY);
	if (state != pdTRUE)
	{
		free(s_Message->payload_p8);
		s_Message->payload_p8 = NULL;
		if (state == errQUEUE_FULL)
		{
//#ifdef ENABLE_PRINT_MSG
			printf("<WEBSERVER.ERROR(WEBSERVER RX Queue is full)<CR>\n");
//#endif
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "HTTP RX Queue is full.");
		}
		else
		{
//#ifdef ENABLE_PRINT_MSG
			printf("<WEBSERVER.ERROR(WEBSERVER RX Queue send unsuccessful)<CR>");
//#endif
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "HTTP RX Queue send unsuccessful.");
		}
	}
}//	File_server_ConsolWriteToActor

static void Get_Property(AMessage_st* s_Message_Rx)
{
//	uint8_t *val_p8  = NULL;
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	char val_p8[256] = {0};

	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	out_JSON 	= cJSON_CreateObject();
	head_JSON 	= in_JSON->child;

	//loop
	do {
		memset(val_p8, 0, sizeof (val_p8));
		get(head_JSON->string, val_p8);
		cJSON_AddStringToObject(out_JSON, head_JSON->string, (char*) val_p8);
		head_JSON = head_JSON->next;
	} while (head_JSON != NULL);

	payLoadData[0] = '\0';
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);


	cJSON_Delete(out_JSON);
	cJSON_Delete(head_JSON);
	cJSON_Delete(in_JSON);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char temp[10] = {0};
//	char keyValue[50]={0};
	char str[200]={0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
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
				payLoadData[0] = '\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
	}
//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
//	{
//		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//		if (in_JSON == NULL) {
//			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//			Add_Response_msg(str,s_Message_Rx, payLoadData);
//			  //  return 1;
//			return;
//		}
//		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
//		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
//		{
//			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
//			if (root != NULL)
//			{
//				// Iterate over the keys
//				cJSON *currentItem = root->child;
//				if(currentItem->valuestring != NULL)
//				{
////					if(!strcmp(currentItem->valuestring, "System/WEB_SERVER.json"))
//					if(!strcasecmp(currentItem->valuestring, "System/WEB_SERVER.json"))
//					{
//						currentItem = currentItem->next;
//						while (currentItem != NULL)
//						{
//							if (cJSON_IsString(currentItem))   // Check the type of the value
//							{
//								set(currentItem->string, currentItem->valuestring,s_Message_Rx);
//							}
//							else if (cJSON_IsNumber(currentItem))
//							{
//								sprintf(keyValue, "%d", currentItem->valueint);
//								set(currentItem->string, keyValue,s_Message_Rx);
//							}
//							currentItem = currentItem->next;    // Move to the next key-value pair
//						}
//					}
//				}
//			}
//		}
//		cJSON_Delete(in_JSON);
//		return;
//	}
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
				name_JSON 		= cJSON_GetObjectItem(responseKey, "WIFI_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					strcpy((char*)temp,name_JSON->valuestring);
					WIFI_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
				}
			}
			cJSON_Delete(in_JSON);
			return;
		 }
	return;


//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
//	{
//		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//		if(strcmp(in_JSON->child->string,"WIFI_STATUS")==0)
//		{
//			name_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_STATUS");
//			strcpy(value,name_JSON->valuestring);
//			WIFI_status = (uint8_t) (value[0]-0x30);
//		}
//
//		else if(strcmp(in_JSON->child->string,"JMSG")==0)
//		{
//			cJSON *jmsg_array = cJSON_GetObjectItem(in_JSON, "JMSG");
//			if (jmsg_array != NULL && cJSON_IsArray(jmsg_array))
//			{
//				//printf("Items in the array:\n");
//				cJSON *json_item;
//				cJSON_ArrayForEach(json_item, jmsg_array)
//				{
//					if (json_item->type == cJSON_Object)
//					{
//						// Process each JSON object in the array
//						process_json_object(json_item);
//					}
////					else
////						console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"Error: Unexpected item type in the array.");
//				}
//			}
////			else
////				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"Error: 'JMSG' is not an array or doesn't exist in the JSON.");
//		 }
//		cJSON_Delete(in_JSON);
//		return;
//	}
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
//	printf("\n COP_add_response_to_COP_Tx_Queue");
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
} //	COP_add_response_to_COP_Tx_Queue

static void Start_Web_Server(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root = NULL;
	cJSON *name = NULL;
	uint16_t u16StopMinutes=0;
	char str[100] ={0}, payLoadData_Web_Ser[200] = {0};
	int ret = -1;
	//printf("\n Start_Web_Server \n");
	//get_from_other_actor("CONSOLE", "WIFI_STATUS");
//	get_from_other_actor("WIFI", "STATUS");
	root = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_Web_Ser);
		  //  return 1;
		return;
	}
	else{
		name = cJSON_GetObjectItem(root, "Minutes");
		u16StopMinutes = name->valueint;
		cJSON_Delete(root);
		//printf("u16StopMinutes:%d\n",u16StopMinutes);

	}

	cJSON *my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "NET_STATUS", "");
	payLoadData_Web_Ser[0] = '\0';
	cJSON_PrintPreallocated(my_JSON, payLoadData_Web_Ser, sizeof(payLoadData_Web_Ser), false);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE",payLoadData_Web_Ser,strlen(payLoadData_Web_Ser),"GET");
	cJSON_Delete(my_JSON);

	cJSON *my_JSON1  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON1, "WIFI_STATUS", "");
	payLoadData_Web_Ser[0] = '\0';
	cJSON_PrintPreallocated(my_JSON1, payLoadData_Web_Ser, sizeof(payLoadData_Web_Ser), false);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE",payLoadData_Web_Ser,strlen(payLoadData_Web_Ser),"GET");
	cJSON_Delete(my_JSON1);
	int count = 0;
//	if((WIFI_status != 2) || (WIFI_status != 3))
//		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Kindly connect to WIFI at first.");
	while(1)
	{
//		if((WIFI_status == 2) || (WIFI_status == 3))
		if((NET_status == E_NET_CONNECTED) || (WIFI_status == 4))
			{
			  if(Web_Server_operation == 1)
			  {
				  display_msg = 1;
				  Web_Server_operation = 0;
				 // printf("\n WIFI is connected in web server");
				  ret = start_file_server("nor:0:\\JFS\\" ,s_Message_Rx);  //spiffs
				  if(ret == ESP_OK)
					  Add_Response_msg("Server is started..",s_Message_Rx, payLoadData_Web_Ser);
				  else
					  Add_Response_msg("Error! Server is failed to start..",s_Message_Rx, payLoadData_Web_Ser);

				  ret = esp_timer_start_periodic(periodic_timer, (u16StopMinutes*60000000)); // start the timer with the period of given minutes
				  if(ret != ESP_OK)
				 		Add_Response_msg("Error in start periodic timer.",s_Message_Rx, payLoadData_Web_Ser);
			  }
			}
		else
		{
			count++;
			if((count > 100) && (display_msg == 0))
			{
				display_msg = 1;
				Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_Web_Ser);
				ServerHandle = NULL;
				vTaskDelete(ServerHandle);
			}

		}
		if(u8stopWebServer ==1)
		{
			u8stopWebServer =0;
			esp_err_t ret = httpd_stop(server);
			if(ret == ESP_OK)
				Add_Response_msg("Server is stopped..",s_Message_Rx, payLoadData_Web_Ser);
			else
				Add_Response_msg("Error! Could not stop the web server.",s_Message_Rx, payLoadData_Web_Ser);
			s_Para.connectStatus_u8 = 0; // web server not connected.
			ServerHandle = NULL;
			vTaskDelete(ServerHandle);
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


/*
 * This callback is used to trigger 1ms periodic timer at start.
 * at every 1ms this callback checks for perfect second.
 * once perfect second is identified same timer is stopped and restarted to identify perfect 10 seconds
 * on perfect 10 seconds pulse is generated and timer is kept running with 10seconds callback
 */
static void periodic_timer_callback(void* arg)
{
	u8stopWebServer =1;
	esp_timer_stop(periodic_timer);
	//printf("Enter in periodic_timer_callback:%lld\n",esp_timer_get_time());
}

static void Deinit_Actor(AMessage_st* s_Message_Rx)
{
	if(FirstEntry)
	{
		FirstEntry = false;
//	    if (Monitor_pucQueueStorage != NULL) {
//	    	heap_caps_free(Monitor_pucQueueStorage);
//	        Monitor_pucQueueStorage = NULL;
//	    }

//	    if (Monitor_pxQueueBuffer != NULL) {
//	    	heap_caps_free(Monitor_pxQueueBuffer);
//	        Monitor_pxQueueBuffer = NULL;
//	    }

		if(fileserver_Rx_queue != NULL)     // delete Rx Queue
		{
			vQueueDelete(fileserver_Rx_queue);
			fileserver_Rx_queue = NULL;
		}

		if(file_serverHandle != NULL)
		{
			vTaskDelete(file_serverHandle);
			file_serverHandle = NULL;
		}
	}
	else
		Add_Response_msg("Web server actor is already de-initialized.",s_Message_Rx, payLoadData);
}

//static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {
//
//	uint8_t out_val[128]  	= {0}; // (uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	cJSON   *my_JSON  	= cJSON_CreateObject();
//	switch(data_type)
//	{
//		case U_INT8  :	sprintf((char*)out_val,	"%d",	*(uint8_t *) 	value);	break;
//		case U_INT16 :	sprintf((char*)out_val,	"%d",	*(uint16_t *) 	value);	break;
//		case INT	 :	sprintf((char*)out_val,	"%d", 	*(int *) 		value);	break;
////		case FLOAT   :  sprintf((char*)out_val,	"%f", 	(float) 	value); break;
//		case STRING  :	sprintf((char*)out_val,	"%s",	(char*) 	value);	break;
//		default 	 : 	break;
//	}
//	cJSON_AddStringToObject(my_JSON, parameter, (char*)out_val);
//	payLoadData[0] = '\0';
//	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
//	uint8_t *newpointer = (uint8_t*) heap_caps_calloc((strlen((char*) payLoadData) + 1), sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (newpointer == NULL)
//	{
//		printf("Memory allocation failed\n");
//		return;
//	}
//	s_Message_Tx.payload_p8 = newpointer;
//	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payLoadData);
//	s_Message_Tx.payload_size = strlen((char*)payLoadData);
//	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
//	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
//	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
//	console_ActorWriteToConsole_xface( &s_Message_Tx);
//	cJSON_Delete(my_JSON);
//}//	set_to_other_actor

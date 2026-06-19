/*
 * EVENT_Actor.c
 *
 *  Created on: 02-May-2024
 *      Author: Priyanka Patil
 */


#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "EVENT_Actor.h"
#include "pcf8563.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/*-----------------------------------------------------------*/

#define OBJ_QUE_COUNT          	200
#define EPOSCH_TO_30_YEAR    	946684800
#define TOTAL_DAY_MINUTES		(uint16_t)( (uint16_t)24 * (uint16_t)60 )
#define MAX_EVENT_ORDER  		400
#define TRUE					1
#define FALSE					0
#define BASE_EPOC_SEC 			(uint64_t)1735689600000
/*-----------------------------------------------------------*/
static const char * THIS_ACTOR 	= "EVENT_ACTOR";
static const char 			THIS_ACTOR_ID 	= 	EVENT_ACTOR;

static int FirsteventEntry = 0;
static AMessage_st  s_Message_Tx;  //s_Message_Rx,
PSRAM_ATTR_BSS static char payLoadDataEvtChk[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadDataUpdate_sch[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadDataEvtExe[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Sch_Tab_data_buffer[3000];       // Buffer for SQL query strings
PSRAM_ATTR_BSS static char Sch_Tab_buffer[3000];    // Buffer for received messages
PSRAM_ATTR_BSS static char Evt_Exe_data_buffer[COMMAND_LEN];
PSRAM_ATTR_BSS static char Eve_Exe_buffer[COMMAND_LEN];
PSRAM_ATTR_BSS static char ExecuteMethodstr[COMMAND_LEN+100];
PSRAM_ATTR_BSS static  char line[COMMAND_LEN];
PSRAM_ATTR_BSS static char line_2[COMMAND_LEN];
static uint32_t Global_EventID = 0;
static char Global_EventString[50] = {0};
static volatile uint64_t startEPOC1 = 0;
static uint64_t endEPOC1 = 0;
typedef enum
{
  RESET = 0,
  SET = !RESET
} FlagStatus;

typedef enum
{
	ALL_YEAR = 1,
	DATE_RANGE,
	HOLIDAY_RANGE
} DayType;

typedef enum
{
	Local = 1,
	SUNSET,
	SUNRISE
} TimeType;

typedef struct
{
	uint16_t locationid;
	uint16_t ownerid;
	uint8_t capabilityLevel;
	double latitude;
	double longitude;
	int16_t timezone;//rawoffset in minutes
	uint8_t d_s_t;

}Struct_LOCATION_TABLE;

typedef struct
{
	uint32_t	EventId;
	uint16_t	ScriptID;
	uint8_t 	Date_Range_Type;
	uint8_t 	Start_month;
	int8_t 		Start_day;
	uint8_t 	End_month;
	int8_t 		End_day;
	uint8_t 	WeekDays;
	uint8_t 	Time_type;
	uint8_t     HolidayStartIndex;
	int8_t     	HolidayStartOffset;
	uint8_t     HolidayEndIndex;
	int8_t     	HolidayEndOffset;
	int8_t      OffsetMinutes;
	uint8_t 	Hour;
	int16_t 	Minute;
	uint8_t   	SortOrder;
	uint8_t   	OverrideSport;
	uint32_t    RampTime;	//RampTimeScene
	uint32_t    Duration;		//DwellTimeScene;

}Struct_SCHEDULE_TABLE;

PSRAM_ATTR_BSS static struct EVENT_parameter
{
	uint8_t 	sr_time [32 ];
	uint8_t 	ss_time [32 ];
	int16_t   	gmt_value;
	uint8_t     dst_value;
	double Latitude;
	double Longitude;
	uint8_t  Reboot_Flag_u8;
	uint8_t  Reboot_HH_u8;
	uint8_t  Reboot_MM_u8;
    uint64_t defer_on_u64;
    uint64_t defer_off_u64;
} EVENT_Para;

/**** Timeout typedef ------------------------------------------------------------*/
typedef volatile struct
{
	uint32_t	ulTwinWhile_Timeout;		// Twin Timeout
	uint32_t	ulWaitAll_Timeout;
	uint32_t	ulSendSNSWhile_Timeout;
	uint32_t	ulAzureWhile_Timeout;
	uint32_t	dowork_ulAzureWhile_Timeout;
	uint32_t	ul_TX_ReadyWhile_Timeout;
	uint32_t	MinuteCnt;
	uint64_t	ulTwinWhileTimerVal;
	uint64_t	ulWaitAllTimerVal;
	uint64_t	ulSendSNSWhileTimerVal;
	uint64_t	ulAzureWhileTimerVal;
	uint64_t	dowork_ulAzureWhileTimerVal;
	uint64_t	ul_TX_ReadyWhileTimerVal;
	uint64_t 	ul5msecTimerVal;			// 5 msec Task
	uint64_t 	ul10msecTimerVal;			// 10 msec Task
	uint64_t  ul100msecTimerVal;		// 100 Second Task
	uint64_t  ul500msecTimerVal;		// 500 Second Task
	uint64_t	ul_1SecTimerVal;			// 1 Second Task
	uint64_t	ul_5SecTimerVal;			// 5 Second Task
	uint64_t	ul_10SecTimerVal;			// 10 Second Task
	uint64_t	ul_1_MinTimerVal;					// 1 Minute Task
	uint64_t  ul_RF_CheckTimerVal;			// 2 Second Task
	uint64_t  ul_NightModeTimerVal;			// 3 Second
	uint64_t  ul_3Sec_FW_Feed;					// 3 Seconds
	uint64_t  ul_RF_HW_ERRTimerVal;			// 3 Second Task
#ifdef HARDWARE_L_SERIES_C
	uint64_t  ul_100mSecBusyTimerVal;			// 3 Second Task
#endif
	uint64_t	ul_MonitorMinVal;					// 1 Minute Task
	uint64_t  ulEventTaskTimerVal;		// 500 Second Task
	uint64_t  ulEEPROM_WriteTimerVal;		// EEPROM Write Task
}TIMEOUT_FLAGS;

typedef struct
{
	uint8_t SR_HR;
	uint8_t SR_MIN;
	uint8_t SS_HR;
	uint8_t SS_MIN;
}Struc_SS_SR;

Struc_SS_SR Str_SS_SR;
TIMEOUT_FLAGS TFlag;
Struct_LOCATION_TABLE location_table;
__attribute__((section(".ext_ram.data"))) Struct_SCHEDULE_TABLE Schedule_table[MAX_EVENT_ORDER];
//__attribute__((section(".ext_ram.data"))) Struct_ACTION_TABLE Action_table[MAX_EVENT_ORDER];
date_time_t UserDateTime_1;
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char data_buffer[MAX_JSON_PAYLOAD_BYTES];
static char Event_Execution_Flag = Enable;
static uint8_t	dst_apply_flag=0;
/*-----------------------------------------------------------*/

/*----------------------------EVENT Actor Functions-------------------------------*/
static void init(void *a, void *b);
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);					//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Set_Event_Task__Flag(AMessage_st* s_Message_Rx);
static void CalculateSunriseTime(AMessage_st* s_Message_Rx,  cJSON * root);
static void CalculateSunsetTime(AMessage_st* s_Message_Rx,  cJSON * root);
static void get_RTC_time(AMessage_st* s_Message_Rx);
static void ExecuteSceneCommand(AMessage_st* s_Message_Rx);
static void ExecuteActionCommand(AMessage_st* s_Message_Rx);
static void ExecuteEventIDCommand(AMessage_st* s_Message_Rx);
static void TimeWarpCommand(AMessage_st* s_Message_Rx);
static void Tasks_EventCheck(void *pvParameters __attribute__((unused)));
static int yisleap(int year);
static int get_yday(int day, int mon, int year);
static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch);
static uint32_t Stop_EpochDay(uint8_t EventOrder);
static uint32_t Start_EpochDay( uint8_t EventOrder);
static unsigned long date_time_to_epoch_days(date_time_t* date_time);
static void Tasks_ExecuteMethodForEvent(void *pvParameters __attribute__((unused)));
static void Update_Location_Table(void);
static void Update_Schedule_Table(void *pvParameters __attribute__((unused)));
static int sunrise_function(double latitude,double longitude,int year,int month,int day,uint8_t *sunrise_time);
static int sunset_function(double latitude,double longitude,int year,int month,int day,uint8_t*sunset_time);
static uint64_t sunset_function_seconds(double latitude,double longitude,int year,int month,int day);
static uint64_t sunrise_function_seconds(double latitude,double longitude,int year,int month,int day);
static double calcSunsetUTC(double JD, double latitude, double longitude);
static double calcSunriseUTC(double JD, double latitude, double longitude);
static double calcSunEqOfCenter(double t);
static double calcJDFromJulianCent(double t);
static double calcJD(int year,int month,int day);
static double calcHourAngleSunset(double lat, double solarDec);
static double calcHourAngleSunrise(double lat, double solarDec);
static double calcSunDeclination(double t);
static double calcSunApparentLong(double t);
static double calcSunTrueLong(double t);
static double calcTimeJulianCent(double jd);
static double calcEquationOfTime(double t);
static double calcEquationOfTime(double t);
static double calcGeomMeanAnomalySun(double t);
static double calcEccentricityEarthOrbit(double t);
static double calcObliquityCorrection(double t);
static double calcGeomMeanLongSun(double t);
static double calcMeanObliquityOfEcliptic(double t);
static double radToDeg(double angleRad);
static double  degToRad(double angleDeg);

static SemaphoreHandle_t EventActorMutex = NULL;

static int execute_com = 0;
/*-----------------------------------------------------------*/

PSRAM_ATTR static struct property prop[] = // Actor Property
{
	{ &EVENT_Para.sr_time,    		"SR_TIME",            STRING, 	"R",  	"SUNRISE Time" },
	{ &EVENT_Para.ss_time,    		"SS_TIME",            STRING, 	"R",  	"SUNSET Time" },
	{ &EVENT_Para.dst_value,    	"DST",                U_INT8,  	"RW",  	"Daylight Saving Time." },
    { &EVENT_Para.gmt_value,    	"GMT",                INT16, 	"RW",  	"Greenwich Mean Time" },
	{ &EVENT_Para.Latitude,    		"LATITUDE",           DOUBLE,  	"RW",  	"Latitude." },
	{ &EVENT_Para.Longitude,    	"LONGITUDE",          DOUBLE,  	"RW",  	"Longitude." },
	{ &EVENT_Para.Reboot_Flag_u8, 	"REBOOT_FLAG", 		  U_INT8, 	"RW", 	"Reboot Flag" },
	{ &EVENT_Para.Reboot_HH_u8, 	"REBOOT_HH", 		  U_INT8, 	"RW", 	"Reboot time in Hour 0-23" },
	{ &EVENT_Para.Reboot_MM_u8, 	"REBOOT_MM", 		  U_INT8, 	"RW", 	"Reboot time in Minute 0-59" },
	{ &EVENT_Para.defer_on_u64, 	"DEFER_ON", 		  U_INT64, 	"RW", 	"Defer ON time" },
	{ &EVENT_Para.defer_off_u64, 	"DEFER_OFF", 		  U_INT64, 	"RW", 	"Defer OFF time" }
};


BaseType_t EventCheck, EventExecute;
static TaskHandle_t  EVENT_Handle = NULL, EventCheck_Handle=NULL,ExecuteMethodForEventHandle =NULL,UpdateSchdule_Handle=NULL; // UpdateAction_Handle=NULL;
static QueueHandle_t EVENT_Rx_Queue, Schedule_Record_Que =NULL, Action_Record_Que =NULL, Script_Execute_Que =NULL, Script_Execute_Que2 =NULL;   //msg_Tx_Queue EventCheck_Que =NULL,,EventID_Execute_Que =NULL
static StaticTask_t xEVENTTaskBuffer, xEventCheckTaskBuffer,xEventExecuteTaskBuffer,xUpdateSchduleTaskBuffer;  //// Declare a static task control block; ,xUpdateActionTaskBuffer //// Declare a static task control block

PSRAM_ATTR_BSS static StackType_t xScheduleTaskStack[UPDATE_SCH_TABLE_TASK_STACK_DEPTH], xTaskStack2 [EVENT_EXECUTE_TASK_STACK_DEPTH], xSceneTaskStack [EVENT_EXECUTE_TASK_STACK_DEPTH], xTaskStack [EVENT_TASK_STACK_DEPTH], xTaskStack1 [EVENT_CHECK_TASK_STACK_DEPTH];
 static StaticQueue_t  Monitor_pxQueueBuffer ,  Schedule_pxQueueBuffer,  Script_Execute_pxQueueBuffer,Script_Execute_pxQueueBuffer2;   //*EvtCheck_pxQueueBuffer = NULL,*Action_pxQueueBuffer = NULL,
/*-----------------------------------------------------------*/
/* US DST: 2nd Sunday in March 02:00:00 UTC (e.g. 1520733600 = 2018-03-11 02:00 UTC). */
const unsigned int Dst_Start_date[]={
	1520733600-EPOSCH_TO_30_YEAR,1552183200-EPOSCH_TO_30_YEAR,1583632800-EPOSCH_TO_30_YEAR,1615687200-EPOSCH_TO_30_YEAR,
	1647136800-EPOSCH_TO_30_YEAR,1678586400-EPOSCH_TO_30_YEAR,1710036000-EPOSCH_TO_30_YEAR,1741485600-EPOSCH_TO_30_YEAR,
	1772935200-EPOSCH_TO_30_YEAR,1804989600-EPOSCH_TO_30_YEAR,1836439200-EPOSCH_TO_30_YEAR,1867888800-EPOSCH_TO_30_YEAR,
	1899338400-EPOSCH_TO_30_YEAR,1930788000-EPOSCH_TO_30_YEAR,1962842400-EPOSCH_TO_30_YEAR,1994292000-EPOSCH_TO_30_YEAR,
	2025741600-EPOSCH_TO_30_YEAR,2057191200-EPOSCH_TO_30_YEAR,2088640800-EPOSCH_TO_30_YEAR,2120090400-EPOSCH_TO_30_YEAR,
	2152144800-EPOSCH_TO_30_YEAR,2183594400-EPOSCH_TO_30_YEAR,2215044000-EPOSCH_TO_30_YEAR,2246493600-EPOSCH_TO_30_YEAR,
	2277943200-EPOSCH_TO_30_YEAR,2309392800-EPOSCH_TO_30_YEAR,2341447200-EPOSCH_TO_30_YEAR,2372896800-EPOSCH_TO_30_YEAR,
	2404346400-EPOSCH_TO_30_YEAR,2435796000-EPOSCH_TO_30_YEAR,2467245600-EPOSCH_TO_30_YEAR,2499300000-EPOSCH_TO_30_YEAR,
	2530749600-EPOSCH_TO_30_YEAR};
/* DST end: 1st Sunday in November 01:00:00 UTC (one hour before 02:00 UTC for proper exit from DST). */
const unsigned int Dst_End_date[]={
	1541293200-EPOSCH_TO_30_YEAR,1572742800-EPOSCH_TO_30_YEAR,1604192400-EPOSCH_TO_30_YEAR,1636246800-EPOSCH_TO_30_YEAR,
	1667696400-EPOSCH_TO_30_YEAR,1699146000-EPOSCH_TO_30_YEAR,1730595600-EPOSCH_TO_30_YEAR,1762045200-EPOSCH_TO_30_YEAR,
	1793494800-EPOSCH_TO_30_YEAR,1825549200-EPOSCH_TO_30_YEAR,1856998800-EPOSCH_TO_30_YEAR,1888448400-EPOSCH_TO_30_YEAR,
	1919898000-EPOSCH_TO_30_YEAR,1951347600-EPOSCH_TO_30_YEAR,1983402000-EPOSCH_TO_30_YEAR,2014851600-EPOSCH_TO_30_YEAR,
	2046301200-EPOSCH_TO_30_YEAR,2077750800-EPOSCH_TO_30_YEAR,2109200400-EPOSCH_TO_30_YEAR,2140650000-EPOSCH_TO_30_YEAR,
	2172704400-EPOSCH_TO_30_YEAR,2204154000-EPOSCH_TO_30_YEAR,2235603600-EPOSCH_TO_30_YEAR,2267053200-EPOSCH_TO_30_YEAR,
	2298502800-EPOSCH_TO_30_YEAR,2329952400-EPOSCH_TO_30_YEAR,2362006800-EPOSCH_TO_30_YEAR,2393456400-EPOSCH_TO_30_YEAR,
	2424906000-EPOSCH_TO_30_YEAR,2456355600-EPOSCH_TO_30_YEAR,2487805200-EPOSCH_TO_30_YEAR,2519859600-EPOSCH_TO_30_YEAR,
	2551309200-EPOSCH_TO_30_YEAR};

#define DST_TABLE_ENTRIES  (sizeof(Dst_Start_date) / sizeof(Dst_Start_date[0]))

PSRAM_ATTR_BSS static uint8_t  Monitor_pucQueueStorage [OBJ_QUE_COUNT * sizeof(AMessage_st)], Schedule_pucQueueStorage [100 * 1000], Script_Execute_pucQueueStorage[ScriptExecute_Ack_QUE_COUNT * sizeof(AMessage_st)], Script_Execute_pucQueueStorage2[ScriptExecute_Ack_QUE_COUNT * ExecuteScript_QUEUE_ITEMSIZE];  //*EvtCheck_pucQueueStorage = NULL, *Action_pucQueueStorage = NULL,

const uint64_t Holiday_table[15][21]=
{
//										2020,				2021,			  2022,			2023,			2024,				2025,				2026,				2027,			2028,			 2029,			2030,			2031,				2032,				2033,			2034,				2035,			2036,			2037,			2038,				2039,				2040
/*1st JAN*/					/*1/1*/{1577836800,1609459200,1640995200,1672531200,1704067200,1735689600,1767225600,1798761600,1830297600,1861920000,1893456000,1924992000,1956528000,1988150400,2019686400,2051222400,2082758400,2114380800,2145916800,2177452800,2208988800},
/*MLK JR DAY*/			/*The 3rd Monday in January*/	{1579478400,1610928000,1642377600,1673827200,1705276800,1737331200,1768780800,1800230400,1831680000,1863129600,1895184000,1926633600,1958083200,1989532800,2020982400,2052432000,2084486400,2115936000,2147385600,2178835200,2210284800},
/*President's DAY*/	/*The 3rd Monday in February*/{1581897600,1613347200,1645401600,1676851200,1708300800,1739750400,1771200000,1802649600,1834704000,1866153600,1897603200,1929052800,1960502400,1992556800,2024006400,2055456000,2086905600,2118355200,2149804800,2181859200,2213308800},
/*ST.Patric's DAY*/ /*Always March 17*/ {1584403200,1615939200,1647475200,1679011200,1710633600,1742169600,1773705600,1805241600,1836864000,1868400000,1899936000,1931472000,1963094400,1994630400,2026166400,2057702400,2089324800,2120860800,2152396800,2183932800,2215555200},
/*Easter Sunday*/		/*complex calculation*/{1586649600,1617494400,1650153600,1680998400,1711843200,1745107200,1775347200,1806192000,1839456000,1869696000,1902960000,1933804800,1964044800,1997308800,2028153600,2058393600,2091657600,2122502400,2155766400,2186006400,2216851200},
/*Mother's Day*/		/* 2nd Sunday of May*/{1589068800,1620518400,1651968000,1684022400,1715472000,1746921600,1778371200,1809820800,1841875200,1873324800,1904774400,1936224000,1967673600,1999123200,2031177600,2062627200,2094076800,2125526400,2156976000,2188425600,2220480000},
/*Memorial Day*/  	/*The last Monday in May*/{1590364800,1622419200,1653868800,1685318400,1716768000,1748217600,1779667200,1811721600,1843171200,1874620800,1906070400,1937520000,1969574400,2001024000,2032473600,2063923200,2095372800,2126822400,2158876800,2190326400,2221776000},
/*Father's Day*/		/* 3rd Sunday of June*/{1592697600,1624147200,1655596800,1687046400,1718496000,1749945600,1782000000,1813449600,1844899200,1876348800,1907798400,1939248000,1971302400,2002752000,2034201600,2065651200,2097100800,2129155200,2160604800,2192054400,2223504000},
/*Independence Day*//*7/4*/{1593820800,1625356800,1656892800,1688428800,1720051200,1751587200,1783123200,1814659200,1846281600,1877817600,1909353600,1940889600,1972512000,2004048000,2035584000,2067120000,2098742400,2130278400,2161814400,2193350400,2224972800},
/*Labor Day*/  			/*The 1st Monday in September*/{1599436800,1630886400,1662336000,1693785600,1725235200,1756684800,1788739200,1820188800,1851638400,1883088000,1914537600,1945987200,1978041600,2009491200,2040940800,2072390400,2103840000,2135894400,2167344000,2198793600,2230243200},
/*Columbus Day*/  	/*The 2nd Monday in October*/{1602460800,1633910400,1665360000,1696809600,1728864000,1760313600,1791763200,1823212800,1854662400,1886112000,1918166400,1949616000,1981065600,2012515200,2043964800,2075414400,2107468800,2138918400,2170368000,2201817600,2233267200},
/*Pink Ribbon Day*/ /*10/24*/	{1603497600,1635033600,1666569600,1698105600,1729728000,1761264000,1792800000,1824336000,1855958400,1887494400,1919030400,1950566400,1982188800,2013724800,2045260800,2076796800,2108419200,2139955200,2171491200,2203027200,2234649600},
///*Pink Ribbon Day*/ /*06/23*/{1592870400,1624406400,1655942400,1687478400,1719100800,1750636800,1782172800,1813708800,1845331200,1876867200,1908403200,1939939200,1971561600,2003097600,2034633600,2066169600,2097792000,2129328000,2160864000,2192400000,2224022400},
/*Veteran's Day*/		/*11/11*/{1605052800,1636588800,1668124800,1699660800,1731283200,1762819200,1794355200,1825891200,1857513600,1889049600,1920585600,1952121600,1983744000,2015280000,2046816000,2078352000,2109974400,2141510400,2173046400,2204582400,2236204800},
/*Thanksgiving Day*//*The 4th Thursday in November*/{1606348800,1637798400,1669248000,1700697600,1732752000,1764201600,1795651200,1827100800,1858550400,1890000000,1922054400,1953504000,1984953600,2016403200,2047852800,2079302400,2111356800,2142806400,2174256000,2205705600,2237155200},
/*Christmas Day*/		/*12/25*/{1608854400,1640390400,1671926400,1703462400,1735084800,1766620800,1798156800,1829692800,1861315200,1892851200,1924387200,1955923200,1987545600,2019081600,2050617600,2082153600,2113776000,2145312000,2176848000,2208384000,2240006400}
};

static const unsigned short days[4][12] =
{
   {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
   { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
   { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
   {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};
//-------------------------- Common Actor Methods ------------------------------//

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx)
{
	uint8_t parameter_found = 0; // Flag to check if actor is found
	int no_of_elements = sizeof(prop) / sizeof(prop[0]);
	for (int i = 0; i < no_of_elements; i++)
	{
		if (!strcmp(property, prop[i].str_name))
		{
			if (!strcmp(prop[i].access, "RW")) {
			parameter_found = 1; // Set flag to indicate actor is found
			switch (prop[i].type)
			{
				case U_INT8:
					{
						if(!strcmp(property, "REBOOT_FLAG"))
						{
							if(( atoi(value) == 0) || ( atoi(value) == 1))
							{
								*(uint8_t*) prop[i].name = atoi(value);
							}
						}
						else if(!strcmp(property, "REBOOT_HH"))
						{
							if(( atoi(value) >= 0) && ( atoi(value) <= 23))
							{
								*(uint8_t*) prop[i].name = atoi(value);
							}
						}
						else if(!strcmp(property, "REBOOT_MM"))
						{
							if(( atoi(value) >= 0) && ( atoi(value) <= 59))
							{
								*(uint8_t*) prop[i].name = atoi(value);
							}
						}
						else
						{
							*(uint8_t*) prop[i].name = atoi(value);
						}
					}
					break;

				case U_INT16:
					*(uint16_t*) prop[i].name = atoi(value);
					break;

				case U_INT32:
					*(uint32_t*) prop[i].name = atoi(value);
					break;

				case U_INT64:
					*(uint64_t*) prop[i].name = strtoll(value, NULL, 10);
					break;

				case INT:
					*(int*) prop[i].name = atoi(value);
					break;

				case INT16:
					*(int16_t*) prop[i].name = atoi(value);
					break;

				case FLOAT:
					*(float*) prop[i].name = atof(value);
					break;

				case DOUBLE:
					*(double*) prop[i].name = atof(value);
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

static void get(char *str_prop, char *val_a8)
{
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(prop[0]);
//	uint32_t duty = 0;
	for (int i = 0; i < no_of_elements; i++)
	{
		if (!strcmp(str_prop, prop[i].str_name))
		{
			switch (prop[i].type)
			{
				case U_INT8:
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
					break;

				case INT:
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case INT16:
					sprintf(val_a8, "%d", *(int16_t*) prop[i].name);
					break;

				case FLOAT:
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(val_a8, prop[i].name);
					break;

				case DOUBLE:
					sprintf(val_a8, "%lf", *(double*) prop[i].name);
					break;

				default:
					break;
			}
		}
	}
}//	get

static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx)
{
	cJSON *out_JSON  = cJSON_CreateObject();
	cJSON_AddStringToObject(out_JSON, "FILE_NAME", "A:/System/EVENT_ACTOR.json");

	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++)
	{
		switch (prop[i].type)
		{
			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
				break;

			case U_INT64:
				sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
				break;

			case INT:
				sprintf(val_a8, "%d", *(int*) prop[i].name);
				break;

			case INT16:
				sprintf(val_a8, "%d", *(int16_t*) prop[i].name);
				break;

			case FLOAT:
				sprintf(val_a8, "%f", *(float*) prop[i].name);
				break;

			case DOUBLE:
				sprintf(val_a8, "%lf", *(double*) prop[i].name);
				break;

			case STRING:
				strcpy(val_a8, prop[i].name);
				break;

			default:
				break;
		}
		if ((strcmp(prop[i].str_name, "GMT")) && (strcmp(prop[i].str_name, "DST")) && (strcmp(prop[i].str_name, "LATITUDE")) && (strcmp(prop[i].str_name, "LONGITUDE"))  && (strcmp(prop[i].str_name, "SS_TIME")) && (strcmp(prop[i].str_name, "SR_TIME")))
		{
			cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
		}
	}
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
	cJSON_Delete(out_JSON);
}

static void init(void *a, void *b)
{
	if (FirsteventEntry == 0)
	{
		FirsteventEntry = 1;
		/*
		 * Write the first time/one time Init Routines here
		 */
		/**********************************
		 * Create queues here
		 */
		    EventActorMutex = xSemaphoreCreateMutex();
		    if (EventActorMutex == NULL)
		    {
		        printf(" Failed to create mutex for event actor \n");
		    }
		    EVENT_Rx_Queue = xQueueCreateStatic(OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
			if (EVENT_Rx_Queue == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("EVENT RX Queue is not created. \n");
#endif
			}
			EVENT_Handle = xTaskCreateStaticPinnedToCore(
							monitor,                 // Task function
							"Event Monitor",            // Task name
							EVENT_TASK_STACK_DEPTH,        // Stack size in words
							NULL,                    // Task parameters (not used here)
							EVENT_TASK_PRIORITY,                       // Task priority
							xTaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xEVENTTaskBuffer,             // Pointer to task control block
							0
			);

			if (EVENT_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				    printf("Failed to create task\n");
#endif
				    // Handle error
				}
			strcpy((char*)&EVENT_Para.sr_time,"\0");
			strcpy((char*)&EVENT_Para.ss_time,"\0");
			EVENT_Para.gmt_value =  -300; //330;
			EVENT_Para.dst_value =0;
			EVENT_Para.Latitude = 39.048696; //18.642853;
			EVENT_Para.Longitude =  -84.634182; //73.850572;
			EVENT_Para.Reboot_Flag_u8 = 1;
			EVENT_Para.Reboot_HH_u8 = 3;	//15;	//12;
			EVENT_Para.Reboot_MM_u8 = 00;
			EVENT_Para.defer_on_u64 = 0;
			EVENT_Para.defer_off_u64 = 0;

		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor

//		cJSON *responseObject11 = cJSON_CreateObject();
//		cJSON_AddStringToObject(responseObject11, "FILE_NAME","A:/System/EVENT_ACTOR.json");
//		cJSON_AddStringToObject(responseObject11, "REBOOT_HH", "3");
//		cJSON_AddStringToObject(responseObject11, "REBOOT_MM", "0");
//		memset(payLoadData,0,sizeof(payLoadData));//\0';
//		cJSON_PrintPreallocated(responseObject11, payLoadData, sizeof(payLoadData), false);
//		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
//		cJSON_Delete(responseObject11);

		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/EVENT_ACTOR.json");
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		cJSON_Delete(responseObject);
		cJSON *responseObject1 = cJSON_CreateObject();
		cJSON_AddNumberToObject(responseObject1, "GMT_VAL",EVENT_Para.gmt_value);
		cJSON_AddNumberToObject(responseObject1, "DST_VAL",dst_apply_flag);
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(responseObject1, payLoadData, sizeof(payLoadData), false);
		Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData, strlen(payLoadData), "GMT_DST");
		cJSON_Delete(responseObject1);
		memset(Schedule_table, 0, sizeof(Schedule_table));
	}
}

static void monitor(void *pvParameters __attribute__((unused)))
{
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;

	while (1)
	{
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(EVENT_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
//			printf("\n EVENT_ACTOR msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("EVENT_ACTOR DT = %s\n\n",s_Message_Rx->payload_p8);
//			}
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties
				if (FirsteventEntry == 0)
					init(0, 0);
				else if((FirsteventEntry==1)&&(EventCheck_Handle==NULL))
				{
					Add_Response_msg(" EVENT_ACTOR Actor initialization is done.", s_Message_Rx, payLoadData);
					EventCheck_Handle = xTaskCreateStaticPinnedToCore(
									Tasks_EventCheck,                 // Task function
									"EVENT_CHECK",            // Task name
									EVENT_CHECK_TASK_STACK_DEPTH,        // Stack size in words
									s_Message_Rx,                    // Task parameters (not used here)
									EVENT_CHECK_TASK_PRIORITY,                       // Task priority
									xTaskStack1,              // Pointer to task stack (allocated in PSRAM)
									&xEventCheckTaskBuffer,             // Pointer to task control block
									1	//0
					);

				if (EventCheck_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
						printf("Failed to create task\n");
#endif
						// Handle error
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				u8Result =0;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL)
				{
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				}
				else{
				head_JSON = name_JSON->child;
				cJSON *root_JSON  = cJSON_CreateObject();
				cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/EVENT_ACTOR.json");
			   // Loop through each key-value pair
				do {
					// Check if the value string is not NULL
					if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
					{
						// Set the key-value pair
						u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
						if(u8Result==1)
						{
						cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
						}
						else if(u8Result==2){
							sprintf(str,"'%s' is a read only property", head_JSON->string);
							 Add_Response_msg(str, s_Message_Rx, payLoadData);
						}
						else{
						cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
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
				//  save parameters to JFS
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
				cJSON_Delete(root_JSON);
				console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
				// Free the parsed JSON
				cJSON_Delete(name_JSON);
			}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			{
//				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//				getAll((char*)prop, (char*) val_p8,s_Message_Rx);
//				free(val_p8);
//			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			{
				get_actor_properties(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UPDATE_LOC"))
			{
				if(Event_Execution_Flag == Enable)
				{
					Update_Location_Table();
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET_RTC_TIME")) {
				get_RTC_time(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UPDATE_SCH_TABLE"))
			{
				if(Event_Execution_Flag == Enable)
				{
					if(Schedule_Record_Que == NULL)
					{
						Schedule_Record_Que = xQueueCreateStatic(100, 1000, Schedule_pucQueueStorage, &Schedule_pxQueueBuffer);
					}
					if (Schedule_Record_Que == NULL){
	#ifdef ENABLE_PRINT_MSG
							printf("ERROR(Schedule_Record_Que is not created)\n");
	#endif
						}
					if(UpdateSchdule_Handle == NULL)
					{
						UpdateSchdule_Handle = xTaskCreateStaticPinnedToCore(
										Update_Schedule_Table,                 // Task function
										"UPDATE_SCHEDULE_TABLE",            // Task name
										UPDATE_SCH_TABLE_TASK_STACK_DEPTH,        // Stack size in words
										s_Message_Rx,                    // Task parameters (not used here)
										UPDATE_SCH_TABLE_PRIORITY,                       // Task priority
										xScheduleTaskStack,              // Pointer to task stack (allocated in PSRAM)
										&xUpdateSchduleTaskBuffer,             // Pointer to task control block
										0
						);
						if (UpdateSchdule_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
								printf("Failed to create task\n");
#endif
								// Handle error
							}
					}
					else
					{
						Add_Response_msg("UPDATE_SCHEDULE_TABLE task already created.", s_Message_Rx, payLoadData);
					}
				}

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SETEVTTASKFG"))
			{
				Set_Event_Task__Flag(s_Message_Rx);

			}

			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "EXECUTESCENE"))
			{
				ExecuteSceneCommand(s_Message_Rx);

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "EXECUTEACTION"))
			{
				ExecuteActionCommand(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "EXECUTEEVENTID"))
			{
				ExecuteEventIDCommand(s_Message_Rx);

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "TIMEWARP"))
			{
				TimeWarpCommand(s_Message_Rx);

			}
			else
			{
				//ACTOR_SYSTEM error message: invalid method
				  Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}

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

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
					break;

				case INT:
					strcpy(typeString, "INT");
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case INT16:
					strcpy(typeString, "INT16");
					sprintf(val_a8, "%d", *(int16_t*) prop[i].name);
					break;

				case FLOAT:
					strcpy(typeString, "FLOAT");
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case DOUBLE:
					strcpy(typeString, "DOUBLE");
					sprintf(val_a8, "%lf", *(double*) prop[i].name);
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
		cJSON_Delete(jsonObject);
		{
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the  EVENT actor.");
	cJSON_AddStringToObject(responseObject, "SET(U_INT8 DST)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table. Pass the parameter in prop and its value is return in val_a8");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "UPDATE_LOC()", "Update Location Table.");
	cJSON_AddStringToObject(responseObject, "UPDATE_SCH_TABLE()", "Update Schedule Table.");
	cJSON_AddStringToObject(responseObject, "UPDATE_ACT_TABLE()", "Update Action Table.");
	cJSON_AddStringToObject(responseObject, "GET_RTC_TIME()", "Get RTC Time.");
	cJSON_AddStringToObject(responseObject, "EXECUTEACTION(U_INT32 ActionIndex)", "Execute Action");
	cJSON_AddStringToObject(responseObject, "EXECUTESCENE(U_INT8 scriptId, U_INT32 rampTime, U_INT32 Duration)", "Execute Script, rampTime and Duration is in second ");
	cJSON_AddStringToObject(responseObject, "EXECUTEEVENTID(U_INT16 eventId)", "Execute EventID");
	cJSON_AddStringToObject(responseObject, "TIMEWARP(U_INT64 startEPOC, U_INT64 endEPOC)", "Timewarp for startEPOC - endEPOC");
	cJSON_AddStringToObject(responseObject, "DEFER(string COMMAND, U_INT64 ENDEPOC)", "Get the Defer epoc value for ON/OFF command, COMMAND = (ON or OFF), ENDEPOC = End epoc time for defer.");

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
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
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

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  		= (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
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
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
    cJSON *in_JSON 		= NULL;
	cJSON *name_JSON = NULL;
	char keyValue[100] = {0};
//	uint8_t temp[10];
    char str[200]={0};
//    char *filedata;

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n EVENT s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
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

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SQL")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"DB_EXECUTE")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsObject(responseKey))
			{
				cJSON *D_Type = cJSON_GetObjectItemCaseSensitive(responseKey, "DateType");
				if(D_Type != NULL)
				{
					if(strcmp(D_Type->string,"DateType")==0){
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(Schedule_Record_Que != NULL)
							xQueueSend(Schedule_Record_Que, payLoadData, QUE_DELAY);
					}

				}

				cJSON *method = cJSON_GetObjectItemCaseSensitive(responseKey, "Command");
				if(method != NULL)
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Script_Execute_Que2 != NULL)
						xQueueSend(Script_Execute_Que2, payLoadData, QUE_DELAY);
				}

				cJSON *count = cJSON_GetObjectItemCaseSensitive(responseKey, "COUNT()");
				if(count != NULL)
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Script_Execute_Que2 != NULL)
						xQueueSend(Script_Execute_Que2, payLoadData, QUE_DELAY);
				}

				cJSON *Sch_count = cJSON_GetObjectItemCaseSensitive(responseKey, "SCH_MIN_ID");
				if(Sch_count != NULL)
				{
				if(strcmp(Sch_count->string, "SCH_MIN_ID")==0){
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Schedule_Record_Que != NULL)
						xQueueSend(Schedule_Record_Que, payLoadData, QUE_DELAY);
					}
				}

				cJSON *Act_count = cJSON_GetObjectItemCaseSensitive(responseKey, "ACT_MIN_ID");
				if(Act_count != NULL)
				{
					if(strcmp(Act_count->string, "ACT_MIN_ID")==0){
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Action_Record_Que != NULL)
						xQueueSend(Action_Record_Que, payLoadData, QUE_DELAY);
				}
				}

				cJSON *Resp = cJSON_GetObjectItemCaseSensitive(responseKey, "RESP");
				if((Resp != NULL) && (cJSON_IsString(Resp)))
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Schedule_Record_Que != NULL)
						xQueueSend(Schedule_Record_Que, payLoadData, QUE_DELAY);
					if(Action_Record_Que != NULL)
						xQueueSend(Action_Record_Que, payLoadData, QUE_DELAY);
				}

				cJSON *locTab = cJSON_GetObjectItemCaseSensitive(responseKey, "LocationId");

				if(locTab != NULL)
				{
					cJSON *Latitude = cJSON_GetObjectItemCaseSensitive(responseKey, "Latitude");
					if((Latitude != NULL)&&(cJSON_IsString(Latitude)))
					{
						 // Convert strings to double
						EVENT_Para.Latitude = strtod(Latitude->valuestring, NULL);
					}
					cJSON *Longitude = cJSON_GetObjectItemCaseSensitive(responseKey, "Longitude");
					if((Longitude != NULL)&&(cJSON_IsString(Longitude)))
					{
						 // Convert strings to double
						EVENT_Para.Longitude = strtod(Longitude->valuestring, NULL);
					}
					uint8_t flg_gmt_dst =0;
					cJSON *GMT = cJSON_GetObjectItemCaseSensitive(responseKey, "RawOffset");
					if((GMT != NULL)&&(cJSON_IsString(GMT)))
					{
						int16_t gmt_v_loc = EVENT_Para.gmt_value;
						EVENT_Para.gmt_value = atoi(GMT->valuestring);
						if(gmt_v_loc != EVENT_Para.gmt_value)
						{
							flg_gmt_dst = 1;
						}
					}
					cJSON *DST = cJSON_GetObjectItemCaseSensitive(responseKey, "IsDstValue");
					if((DST != NULL)&&(cJSON_IsString(DST)))
					{
						EVENT_Para.dst_value = atoi(DST->valuestring);
					}
					if(flg_gmt_dst == 1)
					{
						flg_gmt_dst = 0;

						cJSON *responseObject1 = cJSON_CreateObject();
						cJSON_AddNumberToObject(responseObject1, "GMT_VAL",EVENT_Para.gmt_value);
						cJSON_AddNumberToObject(responseObject1, "DST_VAL",dst_apply_flag);
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(responseObject1, payLoadData, sizeof(payLoadData), false);
						Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData, strlen(payLoadData), "GMT_DST");
						Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", payLoadData, strlen(payLoadData), "LIGHT_GMT_DST");
						Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "FILE_SYSTEM_GMT_DST");
						Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "GMT_DST_SYSTEM");

						cJSON_Delete(responseObject1);
					}
				}
		  }
		}
		cJSON_Delete(in_JSON);
		return;
	}

	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				// Iterate over the keys
				cJSON *currentItem = root->child;
				if(currentItem->valuestring != NULL)
				{
					if(!strcasecmp(currentItem->valuestring, "System/EVENT_ACTOR.json"))
					{
						currentItem = currentItem->next;
						while (currentItem != NULL)
						{
							if (cJSON_IsString(currentItem))   // Check the type of the value
							{
								set(currentItem->string, currentItem->valuestring,s_Message_Rx);
							}
							else if (cJSON_IsNumber(currentItem))
							{
								sprintf(keyValue, "%d", currentItem->valueint);
								set(currentItem->string, keyValue,s_Message_Rx);
							}
							currentItem = currentItem->next;    // Move to the next key-value pair
						}
					}
					else
					{
						char val[100];
						memset(val,0, sizeof(val));
						getAll(NULL,val,NULL);
					}
				}
			}
		}
		else if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_FILE_SIZE")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				name_JSON 		= cJSON_GetObjectItem(root, "FILE_NAME");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					if(strcmp(name_JSON->valuestring,"Database/Schedule.db")==0)
					{
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(Schedule_Record_Que != NULL)
							xQueueSend(Schedule_Record_Que, payLoadData, QUE_DELAY);
					}
					if(strcmp(name_JSON->valuestring,"Database/Action.db")==0)
					{
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(Action_Record_Que != NULL)
							xQueueSend(Action_Record_Que, payLoadData, QUE_DELAY);
					}
				}
			}
	    }
		cJSON_Delete(in_JSON);
		return;
	}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICEID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Script_Execute_Que2 != NULL)
						xQueueSend(Script_Execute_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

void EVENT_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirsteventEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(EVENT_Rx_Queue, s_Message, QUE_DELAY);

	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<EVENT.ERROR(EVENT RX Queue is full)\n");
		}
		else
		{
			printf("<EVENT.ERROR(EVENT RX Queue send unsuccessful)\n");
		}
	}

}//	LED_ConsolWriteToActor




static int yisleap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int get_yday(int day, int mon, int year)
{
    static const int days[2][13] = {
        {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
    int leap = yisleap(year);

    return days[leap][mon%13] + day;
}

/*****************************************************************************
 *  Function definitions
 *****************************************************************************/
uint32_t uwTick;
static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch)
{
   date_time->second = epoch%60; epoch /= 60;
   date_time->minute = epoch%60; epoch /= 60;
   date_time->hour   = epoch%24; epoch /= 24;
   unsigned long years = epoch/(365*4+1)*4; epoch %= 365*4+1;
   unsigned long year;
   for (year=3; year>0; year--)
   {
       if (epoch >= days[year][0])
           break;
   }
   unsigned long month;
   for (month=11; month>0; month--)
   {
       if (epoch >= days[year][month])
           break;
   }
   date_time->year  = years+year;
   date_time->month = month+1;
   date_time->date   = epoch-days[year][month]+1;
}

static unsigned long date_time_to_epoch_days(date_time_t* date_time)
{
  unsigned int date    = date_time->date-1;   // 0-30
  unsigned int month  = date_time->month-1; // 0-11
  unsigned int year   = date_time->year;    // 0-99
  return (((year/4*(365*4+1)+days[year%4][month]+date)));
}


/********************************************************************************
  * @brief  uint32_t Start_EpochDay(uint8_t *Target_Light, uint8_t *EventOrder)
  * @note   Returns the total days equivalent with Start Time
  * @param  uint8_t Target_Light, uint8_t EventOrder
  * @return uint32_t TotalStart Days
	* @author	BDB
	* @retval None
  ******************************************************************************/
static uint32_t Start_EpochDay( uint8_t EventOrder)
{
	uint8_t 	Start_month = Schedule_table[EventOrder].Start_month;
	int8_t 		Start_day = Schedule_table[EventOrder].Start_day;
	uint8_t 	End_month =Schedule_table[EventOrder].End_month;
	int8_t 		End_day =Schedule_table[EventOrder].End_day;
	date_time_t       sdate_tim;
	uint64_t Holiday_epoch=0,Holiday_offset_sec=0;
	date_time_t 	StartDayTime;
	uint32_t			TotalDays	=	0;
	uint32_t stopdays_1=0,startdays_1=0,currentdays_1=0;
	uint8_t stopdd,stopmm,currentyy,currentdd,currentmm,startdd,startmm;
		// Don't calculate Start Day and Stop Day if we haven't received any Event.
		currentdd=UserDateTime_1.date;
	currentmm=UserDateTime_1.month;
	currentyy = UserDateTime_1.year;
				//calculate Event Start total number of Days
	          if(Schedule_table[EventOrder].Date_Range_Type == HOLIDAY_RANGE)
				{
					//printf("\n\n Schedule_table[schedule_record].Date_Range_Type:%d,timeinfo.tm_year:%d \n",Schedule_table[schedule_record].Date_Range_Type,((timeinfo.tm_year +1900)-2020));
					Holiday_epoch=Holiday_table[Schedule_table[EventOrder].HolidayStartIndex-1][(UserDateTime_1.year-20)];
					//printf("Holiday_epoch:%lld \n",Holiday_epoch);
					Holiday_offset_sec = Schedule_table[EventOrder].HolidayStartOffset*24*60*60;
					//printf("Holiday_offset_sec:%lld \n",Holiday_offset_sec);
					Holiday_offset_sec =Holiday_epoch + Holiday_offset_sec;
					//printf("Holiday_offset_sec1:%lld \n",Holiday_offset_sec);
					epoch_to_date_time(&sdate_tim,Holiday_offset_sec-EPOSCH_TO_30_YEAR);
					//printf("Start Holiday date:%02d/%02d, ",sdate_tim.date,sdate_tim.month);
					Start_day = sdate_tim.date;
					Start_month = sdate_tim.month;

					Holiday_epoch=Holiday_table[Schedule_table[EventOrder].HolidayEndIndex-1][(UserDateTime_1.year-20)];
					Holiday_offset_sec = Schedule_table[EventOrder].HolidayEndOffset*24*60*60;
					Holiday_offset_sec =Holiday_epoch + Holiday_offset_sec;
					epoch_to_date_time(&sdate_tim,Holiday_offset_sec-EPOSCH_TO_30_YEAR);
					//printf("End Holiday date:%02d/%02d\n",sdate_tim.date,sdate_tim.month);
					End_day = sdate_tim.date;
					End_month = sdate_tim.month;

				}

				StartDayTime.year		=	UserDateTime_1.year;
				StartDayTime.date		=	Start_day; //Schedule_table[EventOrder].Start_day_or_holiday_offset;
				StartDayTime.month	=	Start_month; //Schedule_table[EventOrder].HolidayStartIndex;
				StartDayTime.hour		=	Schedule_table[EventOrder].Hour;
				StartDayTime.minute	=	Schedule_table[EventOrder].Minute;
				StartDayTime.second	=	00;

			startdd= Start_day; //Schedule_table[EventOrder].Start_day_or_holiday_offset;
			startmm= Start_month; //Schedule_table[EventOrder].HolidayStartIndex;
			stopdd = End_day; //Schedule_table[EventOrder].End_day_or_holiday_offset;
			stopmm = End_month; //Schedule_table[EventOrder].End_month_or_holiday_index;
			 stopdays_1 =get_yday(stopdd, stopmm, currentyy);;
     startdays_1 = get_yday(startdd, startmm, currentyy);
     currentdays_1 = get_yday(currentdd, currentmm, currentyy);
			 if(stopdays_1<startdays_1)
     {
         if(currentdays_1>=startdays_1)
         {
            stopdays_1=get_yday(31, 12, currentyy);; //Change end date to 31/12
					 //DO NOTHING FOR START DAYS
         }
         else if(currentdays_1<=stopdays_1)
         {
            startdays_1=get_yday(1, 1, currentyy);; //Change end date to 31/12 //change start date to 1/1
					 	StartDayTime.hour		=	Schedule_table[EventOrder].Hour;
				StartDayTime.minute	=	Schedule_table[EventOrder].Minute;
				StartDayTime.second	=	00;
					 StartDayTime.date=1;
					  StartDayTime.month=1;
					 StartDayTime.year=UserDateTime_1.year;
		//		TotalDays						=	date_time_to_epoch_days(&StartDayTime);
         }

     }


TotalDays	=	date_time_to_epoch_days(&StartDayTime);

	return(TotalDays);
}
/********************************************************************************
  * @brief  uint32_t Stop_EpochDay(uint8_t *Target_Light, uint8_t *EventOrder)
  * @note   Returns the total days equivalent with Stop Time
  * @param  uint8_t Target_Light, uint8_t EventOrder
  * @return uint32_t TotalStart Days
	* @author	BDB
	* @retval None
  ******************************************************************************/
static uint32_t Stop_EpochDay(uint8_t EventOrder)
{
	uint8_t 	Start_month = Schedule_table[EventOrder].Start_month;
	int8_t 		Start_day = Schedule_table[EventOrder].Start_day;
	uint8_t 	End_month =Schedule_table[EventOrder].End_month;
	int8_t 		End_day =Schedule_table[EventOrder].End_day;
	date_time_t       sdate_tim;
	uint64_t Holiday_epoch=0,Holiday_offset_sec=0;
	date_time_t StopDayTime;
	uint32_t		TotalDays	=	0;
	uint32_t stopdays_1=0,startdays_1=0,currentdays_1=0;
	uint8_t stopdd,stopmm,currentyy,currentdd,currentmm,startdd,startmm;
		// Don't calculate Start Day and Stop Day if we haven't received any Event.
				currentdd=UserDateTime_1.date;
	currentmm=UserDateTime_1.month;
	currentyy = UserDateTime_1.year;

	 if(Schedule_table[EventOrder].Date_Range_Type == HOLIDAY_RANGE)
		{
		//	printf("Schedule_table[schedule_record].Date_Range_Type:%d,timeinfo.tm_year:%d \n",Schedule_table[schedule_record].Date_Range_Type,((timeinfo.tm_year +1900)-2020));
			Holiday_epoch=Holiday_table[Schedule_table[EventOrder].HolidayStartIndex-1][(UserDateTime_1.year-20)];
	//		printf("Holiday_epoch:%lld \n",Holiday_epoch);
			Holiday_offset_sec = Schedule_table[EventOrder].HolidayStartOffset*24*60*60;
	//		printf("Holiday_offset_sec:%lld \n",Holiday_offset_sec);
			Holiday_offset_sec =Holiday_epoch + Holiday_offset_sec;
	//		printf("Holiday_offset_sec1:%lld \n",Holiday_offset_sec);
			epoch_to_date_time(&sdate_tim,Holiday_offset_sec-EPOSCH_TO_30_YEAR);
//			printf("Start Holiday date:%02d/%02d, ",sdate_tim.date,sdate_tim.month);
			Start_day = sdate_tim.date;
			Start_month = sdate_tim.month;

			Holiday_epoch=Holiday_table[Schedule_table[EventOrder].HolidayEndIndex-1][(UserDateTime_1.year-20)];
			Holiday_offset_sec = Schedule_table[EventOrder].HolidayEndOffset*24*60*60;
			Holiday_offset_sec =Holiday_epoch + Holiday_offset_sec;
			epoch_to_date_time(&sdate_tim,Holiday_offset_sec-EPOSCH_TO_30_YEAR);
//			printf("End Holiday date:%02d/%02d\n",sdate_tim.date,sdate_tim.month);
			End_day = sdate_tim.date;
			End_month = sdate_tim.month;

		}
				//calculate Event Start total number of Days

				StopDayTime.year		=	UserDateTime_1.year;
				StopDayTime.date		=	End_day; //Schedule_table[EventOrder].End_day_or_holiday_offset;
				StopDayTime.month	    =	End_month; //Schedule_table[EventOrder].End_month_or_holiday_index;
				StopDayTime.hour		=	Schedule_table[EventOrder].Hour;
				StopDayTime.minute	=	Schedule_table[EventOrder].Minute;
				StopDayTime.second	=	00;

			startdd= Start_day; //Schedule_table[EventOrder].Start_day_or_holiday_offset;
			startmm= Start_month; //Schedule_table[EventOrder].Start_month_or_holiday_index;
			stopdd= End_day; //Schedule_table[EventOrder].End_day_or_holiday_offset;
			stopmm = End_month; //Schedule_table[EventOrder].End_month_or_holiday_index;
			 stopdays_1 =get_yday(stopdd, stopmm, currentyy);;
     startdays_1 = get_yday(startdd, startmm, currentyy);
     currentdays_1 = get_yday(currentdd, currentmm, currentyy);
			 if(stopdays_1<startdays_1)
     {
         if(currentdays_1>=startdays_1)
         {
            stopdays_1=get_yday(31, 12, currentyy);; //Change end date to 31/12
					 startdays_1=get_yday(1, 1, currentyy);; //Change end date to 31/12 //change start date to 1/1
				StopDayTime.hour		=	Schedule_table[EventOrder].Hour;
				StopDayTime.minute	=	Schedule_table[EventOrder].Minute;
				StopDayTime.second	=	00;
			    StopDayTime.date=31;
				StopDayTime.month=12;
			    StopDayTime.year= UserDateTime_1.year;
				//TotalDays						=	date_time_to_epoch_days(&StopDayTime);
					 //DO NOTHING FOR START DAYS
         }
         else if(currentdays_1<=stopdays_1)
         {

         }

     }

TotalDays =	date_time_to_epoch_days(&StopDayTime);


	return(TotalDays);
}

static void Tasks_EventCheck(void *pvParameters __attribute__((unused)))
{
	int i=0;
	int16_t ui8LstMin=0;
	uint32_t CurrentTotalDays=0;
	int schedule_record=0;//,End_Record=0;//, start_record = 0, End_Record = 0;
	//AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	static uint64_t u64CurrentTimeStatusprev=0;

	AMessage_st* s_Message_Rx_data = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx1;
	AMessage_st *s_Message_Rx = &s_Message_Rx1;
	//char data_buffer[8192];
	memset(data_buffer,0,sizeof(data_buffer));
	memcpy(s_Message_Rx, s_Message_Rx_data, sizeof(AMessage_st));
	s_Message_Rx->payload_p8 = (uint8_t*)data_buffer;
	strcpy((char*)s_Message_Rx->cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx->src_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx->dest_Actor_a8,"EVENT_ACTOR");
	cJSON *root_JSON = NULL;
	int16_t	CalculateDayMinute	=	0;
	uint8_t	RTC_WeekDay	=	0;
	uint32_t	StartTotalDays		=	0;
	uint32_t	StopTotalDays			=	0;
	uint8_t weekday 		= 0x80;
	int16_t	CheckDayOffSet			=	0;
	uint8_t	dst_enable_flag=RESET;
	int8_t priority_number = 100;
	uint8_t u8Trigger_Event =0;//u8DayChange =0;
	uint8_t DD;
	uint8_t MM;
	uint8_t YY;
	uint64_t u64epoch_seconds= 0;
	uint8_t sunset_time[10] = {0};
	uint8_t sunrise_time[10] = {0};
	date_time_t  sdate_timUTC;
	date_time_t  sdate_tim;
	uint64_t current_epos_sec = 0, current_epos_sec_3 = 0, current_epos_sec_print = 0;//mills;
	uint8_t 	Event_Hour[MAX_EVENT_ORDER] = {0};;
	int16_t 	Event_Minute[MAX_EVENT_ORDER] = {0};
	char str[100] ={0};
	char str_123[50];

	uint8_t flg_gmt_dst1 =0;
	uint8_t dst_old_st =0;
	uint8_t prevDD=0;
	struct timeval currentTime;
	bool executedThisMinute = false;
	if(Event_Execution_Flag != Enable)
	goto exit;


	#ifdef ENABLE_PRINT_MSG
		printf("Please wait for NTP Sync...\n");
	#endif

	while(1)
	{
		if(startEPOC1 == 0)
		{
			_gettimeofday_r(NULL, &currentTime, NULL);
			current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		}
		else
		{
			current_epos_sec = startEPOC1;
		}
//				printf("\n\n current_epos_sec = %lld, BASE_EPOC_SEC = %lld", current_epos_sec, BASE_EPOC_SEC);
		if (current_epos_sec > BASE_EPOC_SEC)
		break;
		vTaskDelay(500/ portTICK_PERIOD_MS);
	}
	#ifdef ENABLE_PRINT_MSG
		printf("********* NTP is Synced. **********\n");
	#endif
		Add_Response_msg("RTC is set and events are enabled.", s_Message_Rx, payLoadDataEvtChk);
	while(1){
		priority_number = 100;
		u8Trigger_Event = SET;
//		if (xSemaphoreTake(EventActorMutex, portMAX_DELAY) == pdTRUE)
		{
			if(u8Trigger_Event == SET)
			{
				if(Event_Execution_Flag != Enable)
				{
//					xSemaphoreGive(EventActorMutex);
					goto exit;
				}
				if(startEPOC1 == 0)
				{
				_gettimeofday_r(NULL, &currentTime, NULL);
				current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				}
				else
				{
					current_epos_sec = startEPOC1;
				}
				#ifdef ENABLE_PRINT_MSG
					printf("Current_epos_sec: %lld\n", current_epos_sec);
				#endif

				current_epos_sec_print = current_epos_sec;
				sdate_timUTC.milSec = current_epos_sec % 1000;  // get msec time of UTC
				current_epos_sec =(current_epos_sec/1000)-EPOSCH_TO_30_YEAR;  //Convert epoch msec to seconds and subtract offset of 30 years
				current_epos_sec_3 = current_epos_sec;
				epoch_to_date_time(&sdate_timUTC,current_epos_sec);
				#ifdef ENABLE_PRINT_MSG
					printf("\n\nUTC TIME: %02d/%02d/%02d %02d:%02d\n\n",sdate_timUTC.year, sdate_timUTC.month,sdate_timUTC.date,sdate_timUTC.hour,sdate_timUTC.minute); // Years since 1900
				#endif


				ui8LstMin= EVENT_Para.gmt_value;
				current_epos_sec = current_epos_sec + (ui8LstMin * 60);//Reverse of set_rtc
				// Apply timezone offset to current time

				if(EVENT_Para.dst_value!=0) //Only then opply the DST
				{
					i=0;
					while(i<DST_TABLE_ENTRIES)
					{

						if((current_epos_sec>=Dst_Start_date[i])&&(current_epos_sec<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
						{
							dst_enable_flag=SET;
							break;
						}
						i++;
					}
					if(dst_enable_flag == SET)
					{
						current_epos_sec = current_epos_sec +3600; //advance clock by 1 hour/3600 seconds
						dst_old_st = dst_apply_flag;
						dst_apply_flag = 1;
						if(dst_old_st != dst_apply_flag)
						{
							flg_gmt_dst1 = 1;
						}
					}
					else
					{
						dst_old_st = dst_apply_flag;
						dst_apply_flag = 0;
						if(dst_old_st != dst_apply_flag)
						{
							flg_gmt_dst1 = 1;
						}
					}
					dst_enable_flag = RESET;
					if(flg_gmt_dst1 == 1)
					{
						flg_gmt_dst1 = 0;

						cJSON *responseObject1 = cJSON_CreateObject();
						cJSON_AddNumberToObject(responseObject1, "GMT_VAL",EVENT_Para.gmt_value);
						cJSON_AddNumberToObject(responseObject1, "DST_VAL",dst_apply_flag);
						memset(payLoadDataEvtChk,0,sizeof(payLoadDataEvtChk));//\0';
						cJSON_PrintPreallocated(responseObject1, payLoadDataEvtChk, sizeof(payLoadDataEvtChk), false);
						Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadDataEvtChk, strlen(payLoadDataEvtChk), "GMT_DST");
						Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", payLoadDataEvtChk, strlen(payLoadDataEvtChk), "LIGHT_GMT_DST");
						Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "FILE_SYSTEM_GMT_DST");
						Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "GMT_DST_SYSTEM");


						cJSON_Delete(responseObject1);
					}
				}

				// Convert to local time
				epoch_to_date_time(&sdate_tim,current_epos_sec);
				UserDateTime_1.hour = sdate_tim.hour;
				UserDateTime_1.minute = sdate_tim.minute;
				UserDateTime_1.second = sdate_tim.second;
				UserDateTime_1.year = sdate_tim.year;
				UserDateTime_1.month = sdate_tim.month;
				UserDateTime_1.date = sdate_tim.date;

					if(UserDateTime_1.hour == 0)
					{
						if(UserDateTime_1.minute == 0)
						{
							if(UserDateTime_1.second <= 3)
							{
								if(startEPOC1 == 0)
								{
									Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "RESETDISCONCOUNT");
									Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", "\0", 0, "RESETINTERNETCOUNT");
								}
							}
						}
					}

				if(EVENT_Para.Reboot_Flag_u8 == 1)
				{
					if(EVENT_Para.Reboot_HH_u8 == UserDateTime_1.hour)
					{
						if(EVENT_Para.Reboot_MM_u8 == UserDateTime_1.minute)
						{
							if(UserDateTime_1.second <= 3)
							{
								if(startEPOC1 == 0)
								{
									vTaskDelay(3 / portTICK_PERIOD_MS); // Delay 3 Second

									Add_Response_msg("Resetting the device. Kindly wait...", s_Message_Rx, payLoadDataEvtChk);
									Restart_ESP_Xface(1);
								}
							}
						}
					}
				}
				if (((UserDateTime_1.second < 6) ) && !executedThisMinute)
				{
					executedThisMinute = true;

					if(startEPOC1 == 0)
					{
						if(root_JSON == NULL)
						{
							root_JSON = cJSON_CreateObject();
						}

						#ifdef ENABLE_PRINT_MSG
							printf("Local TIME: %02d/%02d/%02d %02d:%02d:%02d\n",UserDateTime_1.date, UserDateTime_1.month,UserDateTime_1.year,UserDateTime_1.hour,UserDateTime_1.minute,UserDateTime_1.second); // Years since 1900
						#endif

						if(root_JSON != NULL)
						{
							size_t  ha = xPortGetFreeHeapSize();
							size_t  ha1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
							cJSON_AddNumberToObject(root_JSON, "Current_Epoch_Sec", current_epos_sec_print);
							sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d:%03d", sdate_timUTC.year, sdate_timUTC.month, sdate_timUTC.date, sdate_timUTC.hour,sdate_timUTC.minute, sdate_timUTC.second, sdate_timUTC.milSec); // Years since 1900
							cJSON_AddStringToObject(root_JSON, "UTC TIME", str);
							sprintf(str,"%02d/%02d/%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month,UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
							cJSON_AddStringToObject(root_JSON, "Local TIME", str);
							CalculateSunriseTime(NULL, root_JSON);
							CalculateSunsetTime(NULL, root_JSON);
							cJSON_AddNumberToObject(root_JSON, "INTERNAL HEAP",ha);
							cJSON_AddNumberToObject(root_JSON, "PSRAM HEAP",ha1);
							memset(payLoadDataEvtChk,0, sizeof(payLoadDataEvtChk));
							cJSON_PrintPreallocated(root_JSON, payLoadDataEvtChk, sizeof(payLoadDataEvtChk), false);
							strcpy((char*)s_Message_Rx->payload_p8, payLoadDataEvtChk);
							cJSON_Delete(root_JSON);
							root_JSON = NULL;
							console_send_responce_to_console_xface(s_Message_Rx);
						 }
						sprintf(str_123,"%02d:%02d",Str_SS_SR.SS_HR,Str_SS_SR.SS_MIN); // Years since 1900
						strcpy((char*)&EVENT_Para.ss_time, str_123);
						sprintf(str_123,"%02d:%02d",Str_SS_SR.SR_HR,Str_SS_SR.SR_MIN); // Years since 1900
						strcpy((char*)&EVENT_Para.sr_time, str_123);
					}
					#ifdef ENABLE_PRINT_MSG
						printf("Local TIME: %02d/%02d/%02d %02d:%02d:%02d\n",UserDateTime_1.date, UserDateTime_1.month,UserDateTime_1.year,UserDateTime_1.hour,UserDateTime_1.minute,UserDateTime_1.second); // Years since 1900
					#endif


					CurrentTotalDays	=	date_time_to_epoch_days(&UserDateTime_1);
					CalculateDayMinute	=	(int16_t)((UserDateTime_1.hour * 60) + UserDateTime_1.minute);
					for(schedule_record = 0;(Schedule_table[schedule_record].ScriptID!=0); schedule_record++)
					{
						Event_Hour[schedule_record] = Schedule_table[schedule_record].Hour;
						Event_Minute[schedule_record] = Schedule_table[schedule_record].Minute;
						RTC_WeekDay		=	(CurrentTotalDays - 2)% 7+1;
						StartTotalDays	=	Start_EpochDay(schedule_record );
						StopTotalDays	=	Stop_EpochDay(schedule_record);
						if(StopTotalDays >= StartTotalDays)
						{

							if( (CurrentTotalDays >= StartTotalDays) && (CurrentTotalDays <= StopTotalDays) )
							{
								// Check the UTC Offset Day for proper opration of Events.
								CheckDayOffSet	=	CalculateDayMinute ;//+ Light_veriable.eventid[EventOrder].DayOffSet;
								// Check if Offset Minutes crossed the Day minutes. For Positive Offset It Will increase Dayof Week by Offset Time.
								if(CheckDayOffSet >= TOTAL_DAY_MINUTES)
								{
									if(RTC_WeekDay >= RTC_WEEKDAY_SUNDAY )
									{
										RTC_WeekDay = RTC_WEEKDAY_MONDAY;
									}
									else  RTC_WeekDay++;
								}
								else if(CheckDayOffSet < 0) //Check if Offset is in Minus, Day is lower Than Current Day.
								{
									if(RTC_WeekDay == RTC_WEEKDAY_MONDAY  )
									{
										RTC_WeekDay = RTC_WEEKDAY_SUNDAY;
									}
									else  RTC_WeekDay--;
								}// End DayOfWeek Offset calculation
								if(prevDD != UserDateTime_1.date)
								{
									prevDD = UserDateTime_1.date;
									if(Schedule_table[schedule_record].Time_type==SUNSET)
									{
										YY= UserDateTime_1.year;
										MM=	UserDateTime_1.month;
										DD=	UserDateTime_1.date;
										sunset_function(EVENT_Para.Latitude,EVENT_Para.Longitude,YY,MM,DD,sunset_time);
										u64epoch_seconds = sunset_function_seconds(EVENT_Para.Latitude,EVENT_Para.Longitude*(-1),YY,MM,DD);
										u64epoch_seconds-= EPOSCH_TO_30_YEAR;
										ui8LstMin= EVENT_Para.gmt_value;
										u64epoch_seconds = u64epoch_seconds + (ui8LstMin * 60);//Reverse of set_rtc
										if(EVENT_Para.dst_value!=0) //Only then opply the DST
										{
											i=0;
											while(i<DST_TABLE_ENTRIES)
											{

												if((u64epoch_seconds>=Dst_Start_date[i])&&(u64epoch_seconds<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
												{
													dst_enable_flag=SET;
													break;
												}
												i++;
											}
											if(dst_enable_flag == SET)
											{
												u64epoch_seconds = u64epoch_seconds +3600; //advance clock by 1 hour/3600 seconds
												//	now += 3600;
											}
											dst_enable_flag = RESET;
										}

										epoch_to_date_time(&sdate_tim,u64epoch_seconds);
										Str_SS_SR.SS_HR = sdate_tim.hour;
										Str_SS_SR.SS_MIN = sdate_tim.minute;
										Event_Hour[schedule_record] = ((Str_SS_SR.SS_HR*60+Str_SS_SR.SS_MIN+Schedule_table[schedule_record].OffsetMinutes)/60)%24;
										Event_Minute[schedule_record] = (Str_SS_SR.SS_HR*60+Str_SS_SR.SS_MIN+Schedule_table[schedule_record].OffsetMinutes)%60;
									}
									else if(Schedule_table[schedule_record].Time_type == SUNRISE)
									{
										YY= UserDateTime_1.year;
										MM=	UserDateTime_1.month;
										DD=	 UserDateTime_1.date;
										sunrise_function(EVENT_Para.Latitude,EVENT_Para.Longitude,YY,MM,DD,sunrise_time);

										u64epoch_seconds = sunrise_function_seconds(EVENT_Para.Latitude,EVENT_Para.Longitude*(-1),YY,MM,DD);
										u64epoch_seconds-= EPOSCH_TO_30_YEAR;
										ui8LstMin= EVENT_Para.gmt_value;
										// Apply timezone offset to current time
										u64epoch_seconds = u64epoch_seconds + (ui8LstMin * 60);//Reverse of set_rtc
										if(EVENT_Para.dst_value!=0) //Only then opply the DST
										{
											i=0;
											while(i<DST_TABLE_ENTRIES)
											{

												if((u64epoch_seconds>=Dst_Start_date[i])&&(u64epoch_seconds<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
												{
													dst_enable_flag=SET;
													break;
												}
												i++;
											}
											if(dst_enable_flag == SET)
											{
												u64epoch_seconds = u64epoch_seconds +3600; //advance clock by 1 hour/3600 seconds
												//	now += 3600;
											}
											dst_enable_flag = RESET;
										}
										epoch_to_date_time(&sdate_tim,u64epoch_seconds);
										Str_SS_SR.SR_HR = sdate_tim.hour;
										Str_SS_SR.SR_MIN = sdate_tim.minute;
										Event_Hour[schedule_record] = ((Str_SS_SR.SR_HR*60+Str_SS_SR.SR_MIN+Schedule_table[schedule_record].OffsetMinutes)/60)%24;
										Event_Minute[schedule_record] = (Str_SS_SR.SR_HR*60+Str_SS_SR.SR_MIN+Schedule_table[schedule_record].OffsetMinutes)%60;
									}
								}
								else
								{
									if(Schedule_table[schedule_record].Time_type==SUNSET)
									{
										Event_Hour[schedule_record] = ((Str_SS_SR.SS_HR*60+Str_SS_SR.SS_MIN+Schedule_table[schedule_record].OffsetMinutes)/60)%24;
										Event_Minute[schedule_record] = (Str_SS_SR.SS_HR*60+Str_SS_SR.SS_MIN+Schedule_table[schedule_record].OffsetMinutes)%60;
									}
									else if(Schedule_table[schedule_record].Time_type == SUNRISE)
									{
										Event_Hour[schedule_record] = ((Str_SS_SR.SR_HR*60+Str_SS_SR.SR_MIN+Schedule_table[schedule_record].OffsetMinutes)/60)%24;
										Event_Minute[schedule_record] = (Str_SS_SR.SR_HR*60+Str_SS_SR.SR_MIN+Schedule_table[schedule_record].OffsetMinutes)%60;
									}
								}

								weekday = ( weekday >> RTC_WeekDay);
								if(weekday & (Schedule_table[schedule_record].WeekDays))
								{
									if(Event_Hour[schedule_record] == UserDateTime_1.hour)
									{
										if(Event_Minute[schedule_record] == UserDateTime_1.minute)
										{//-----------scan for Higher priority
											if((Schedule_table[schedule_record].SortOrder <= priority_number)&&(Schedule_table[schedule_record].SortOrder !=0)
											&&(priority_number!=0))
											{
												priority_number = Schedule_table[schedule_record].SortOrder;
											}
										}
									}
								}
								weekday = 0x80;
							}
						}	// end of if(StopTotalDays >= StartTotalDays)
						if(Event_Execution_Flag != Enable)
						{
//							xSemaphoreGive(EventActorMutex);
							goto exit;
						}

					} // end of for loop


					//---------------------Execute All zone event and scan for specific zones
					weekday 		= 0x80;

					for(schedule_record = 0; (Schedule_table[schedule_record].ScriptID!=0); schedule_record++)
					{
						RTC_WeekDay	=	 (CurrentTotalDays - 2)% 7+1;
						StartTotalDays	=	Start_EpochDay(schedule_record );
						StopTotalDays		=	Stop_EpochDay(schedule_record );
						if(StopTotalDays >= StartTotalDays)
						{
							if( (CurrentTotalDays >= StartTotalDays) && (CurrentTotalDays <= StopTotalDays) )
							{
								//WeekCheck Start
								// if it is in between dates then send data as per weekday and timing
								// Check the UTC Offset Day for proper opration of Events.
								CheckDayOffSet	=	CalculateDayMinute ;//+ Light_veriable.eventid[EventOrder].DayOffSet;

								// Check if Offset Minutes crossed the Day minutes. For Positive Offset It Will increase Dayof Week by Offset Time.
								if(CheckDayOffSet >= TOTAL_DAY_MINUTES)
								{
									if(RTC_WeekDay >= RTC_WEEKDAY_SUNDAY )
									{
										RTC_WeekDay = RTC_WEEKDAY_MONDAY;
									}
									else  RTC_WeekDay++;
								}
								else	if(CheckDayOffSet < 0) //Check if Offset is in Minus, Day is lower Than Current Day.
								{
									if(RTC_WeekDay == RTC_WEEKDAY_MONDAY  )
									{
										RTC_WeekDay = RTC_WEEKDAY_SUNDAY;
									}
									else  RTC_WeekDay--;
								}// End DayOfWeek Offset calculation
								weekday = ( weekday >> RTC_WeekDay);

								if(weekday & (Schedule_table[schedule_record].WeekDays))
								{
									if(Event_Hour[schedule_record] == UserDateTime_1.hour)
									{
										if(Event_Minute[schedule_record] == UserDateTime_1.minute)
										{
											if(Schedule_table[schedule_record].SortOrder == priority_number)
											{
												{
													if(Script_Execute_Que == NULL)
													{
														Script_Execute_Que = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, 200, Script_Execute_pucQueueStorage, &Script_Execute_pxQueueBuffer);
														Script_Execute_Que2 = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, ExecuteScript_QUEUE_ITEMSIZE, Script_Execute_pucQueueStorage2, &Script_Execute_pxQueueBuffer2);
													}
													if (Script_Execute_Que == NULL) {
														#ifdef ENABLE_PRINT_MSG
															printf("Error in creating Script_Execute_Que\n ");
														#endif
														continue;
													}
													{
														if(Script_Execute_Que != NULL)
													{

														if(root_JSON == NULL)
														{
															root_JSON = cJSON_CreateObject();
														}

														if(root_JSON != NULL)
														{
															cJSON_AddNumberToObject(root_JSON, "Current_Epoch_Sec", current_epos_sec_print);
															sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d:%03d", sdate_timUTC.year, sdate_timUTC.month, sdate_timUTC.date, sdate_timUTC.hour,sdate_timUTC.minute, sdate_timUTC.second, sdate_timUTC.milSec); // Years since 1900
															cJSON_AddStringToObject(root_JSON, "UTC TIME", str);
															sprintf(str,"%02d/%02d/%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month,UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
															cJSON_AddStringToObject(root_JSON, "Local TIME", str);
															CalculateSunriseTime(NULL, root_JSON);
															CalculateSunsetTime(NULL, root_JSON);
															cJSON_AddNumberToObject(root_JSON, "ScriptId",Schedule_table[schedule_record].ScriptID);
															cJSON_AddNumberToObject(root_JSON, "Event_ID",Schedule_table[schedule_record].EventId);

															cJSON_AddNumberToObject(root_JSON, "RampTime",Schedule_table[schedule_record].RampTime);
															cJSON_AddNumberToObject(root_JSON, "Duration",Schedule_table[schedule_record].Duration);

															memset(payLoadDataEvtChk,0, sizeof(payLoadDataEvtChk));
															cJSON_PrintPreallocated(root_JSON, payLoadDataEvtChk, sizeof(payLoadDataEvtChk), false);
															strcpy((char*)s_Message_Rx->payload_p8, payLoadDataEvtChk);

															cJSON_Delete(root_JSON);
															root_JSON = NULL;
															console_send_responce_to_console_xface(s_Message_Rx);
														 }
														cJSON *my_JSON  	= cJSON_CreateObject();
														cJSON_AddNumberToObject(my_JSON, "ScriptId",Schedule_table[schedule_record].ScriptID);
														cJSON_AddNumberToObject(my_JSON, "Event_ID",Schedule_table[schedule_record].EventId);
														cJSON_AddNumberToObject(root_JSON, "RampTime",Schedule_table[schedule_record].RampTime);
														cJSON_AddNumberToObject(root_JSON, "Duration",Schedule_table[schedule_record].Duration);

														if(startEPOC1)
														{
															cJSON_AddNumberToObject(my_JSON, "NotToExecute",1);
															cJSON_AddNumberToObject(my_JSON, "Start_Epoch",startEPOC1);

														}
														sprintf(str,"%02d/%02d/%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month,UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
														cJSON_AddStringToObject(my_JSON, "Local_Time", str);

														cJSON_AddNumberToObject(my_JSON, "Override_Sport",Schedule_table[schedule_record].OverrideSport);


														//											s_Message_Rx->payload_p8 = (uint8_t*)cJSON_PrintUnformatted(my_JSON);
														s_Message_Rx->payload_p8[0]='\0';
														cJSON_PrintPreallocated(my_JSON, (char*)s_Message_Rx->payload_p8, sizeof(data_buffer), false);
														cJSON_Delete(my_JSON); // Free the cJSON object

														if(ExecuteMethodForEventHandle == NULL)
														{
															ExecuteMethodForEventHandle = xTaskCreateStaticPinnedToCore(
																Tasks_ExecuteMethodForEvent,                 // Task function
																"EVENT_EXECUTE",            // Task name
																EVENT_EXECUTE_TASK_STACK_DEPTH,        // Stack size in words
																s_Message_Rx,                    // Task parameters (not used here)
																EVENT_EXECUTE_TASK_PRIORITY,                       // Task priority
																xTaskStack2,              // Pointer to task stack (allocated in PSRAM)
																&xEventExecuteTaskBuffer,             // Pointer to task control block
																1	//0
															);

															if (ExecuteMethodForEventHandle == NULL) {
																#ifdef ENABLE_PRINT_MSG
																	printf("Failed to create task\n");
																#endif
																// Handle error
																continue;
															}
															else
															{
																execute_com = 0;
															}
														}
														else
														{
															execute_com = 0;
														}

															xQueueSend(Script_Execute_Que, (char*)s_Message_Rx->payload_p8,  QUE_DELAY);
															u64CurrentTimeStatusprev = current_epos_sec_3 - 3600 + 1;
														}
													}
												}
												vTaskDelay(500 / portTICK_PERIOD_MS);
											}
										}
									}
								}
								weekday = 0x80;
							}
						}
						if(Event_Execution_Flag != Enable)
						{
//							xSemaphoreGive(EventActorMutex);
							goto exit;
						}
					}
					weekday 		= 0x80;
					if(Event_Execution_Flag != Enable)
					{
//						xSemaphoreGive(EventActorMutex);
						goto exit;
					}
				}  // end of if (((UserDateTime_1.second == 0) || (UserDateTime_1.second == 1)) && !executedThisMinute)

//				xSemaphoreGive(EventActorMutex);
			} // end of if(u8Trigger_Event == SET)
		}  // end of xSemaphoreTake
		if (UserDateTime_1.second > 5)
		{
			executedThisMinute = false;
		}
		else
		{
//			executedThisMinute = false;
//			vTaskDelay(3000 / portTICK_PERIOD_MS);
		}
			if(startEPOC1 != 0)
			{
				executedThisMinute = false;
				startEPOC1 = startEPOC1+60000;
				if(startEPOC1 >= endEPOC1 )
				{

					if(root_JSON == NULL)
					{
						root_JSON = cJSON_CreateObject();
					}

					if(root_JSON != NULL)
					{
						cJSON_AddStringToObject(root_JSON, "TIME_WRAP", "FINISHED");
						cJSON_AddNumberToObject(root_JSON, "Current_Epoch_Sec", current_epos_sec_print);
						cJSON_AddNumberToObject(root_JSON, "Current_Epoch_Sec", endEPOC1);
						sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d:%03d", sdate_timUTC.year, sdate_timUTC.month, sdate_timUTC.date, sdate_timUTC.hour,sdate_timUTC.minute, sdate_timUTC.second, sdate_timUTC.milSec); // Years since 1900
						cJSON_AddStringToObject(root_JSON, "UTC TIME", str);
						sprintf(str,"%02d/%02d/%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month,UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
						cJSON_AddStringToObject(root_JSON, "Local TIME", str);
						memset(payLoadDataEvtChk,0, sizeof(payLoadDataEvtChk));
						cJSON_PrintPreallocated(root_JSON, payLoadDataEvtChk, sizeof(payLoadDataEvtChk), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadDataEvtChk);

						cJSON_Delete(root_JSON);
						root_JSON = NULL;
						console_send_responce_to_console_xface(s_Message_Rx);
					 }
					startEPOC1 = 0;
					endEPOC1 = 0;
				}
			}
		if(startEPOC1 == 0)
		{
			uint64_t u64CurrentTimeStatus = current_epos_sec_3;
			if(u64CurrentTimeStatusprev == 0)
			{
				u64CurrentTimeStatusprev=u64CurrentTimeStatus;
			}
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
		else
		{
			// Delay for 10 ms before the next check
			vTaskDelay(1 / portTICK_PERIOD_MS);
		}
	}  // end of while

	exit:
	#ifdef ENABLE_PRINT_MSG
		printf("\n exit the task");
	#endif
	EventCheck_Handle =NULL;
	vTaskDelete(EventCheck_Handle);  // Delete the task
}

//static void ExecuteMethodForEvent(uint16_t u16ScriptID,uint8_t u8EventExecuteFlag, AMessage_st* s_Message_Rx)
static void Tasks_ExecuteMethodForEvent(void *pvParameters __attribute__((unused)))
{
	cJSON *root = NULL;
	char Deviceid[32] ={0};
	char time_local[50];
	AMessage_st* s_Message_Rx1 = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx2; 
	AMessage_st* s_Message_Rx=&s_Message_Rx2;
	memcpy(s_Message_Rx,s_Message_Rx1,sizeof(AMessage_st));
	strcpy((char*)s_Message_Rx->cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx->src_Actor_a8,"CONSOLE");
	strcpy((char*)s_Message_Rx->dest_Actor_a8,"EVENT_ACTOR");
	uint16_t u16ScriptID=0, u16ScriptID_11 = 0;
	uint16_t u16ActionIndex=0;
	static uint8_t u8ActionIndex_avl_local=0;
	uint32_t u32RampTimeScene = 0;
	uint32_t u32DwellTimeScene = 0;
	cJSON *D2CMSG = NULL;
	cJSON *Command_Array = NULL;
	char device_ID_found = 0, Event_execute_flag = 0;
	int Script_ID_cnt = 0;
	uint32_t EventID = 0;
	uint8_t OverrideSportVal = 0;

	if(Event_Execution_Flag != Enable)
		goto exit;

 // Create a JSON object to request the Device ID
	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
	cJSON* jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadDataEvtExe,0, sizeof(payLoadDataEvtExe));
	cJSON_PrintPreallocated(jsonObject, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "GET");

	while(1)
	{
		if (pdTRUE == xQueueReceive(Script_Execute_Que, (void*) Eve_Exe_buffer, portMAX_DELAY))  //2000
		{
			uint8_t u8NotToExecuteFlag=0;
			uint64_t startEPOC1Temp=0;
			s_Message_Rx->payload_p8 = (uint8_t*) Evt_Exe_data_buffer;
			strcpy((char*)s_Message_Rx->payload_p8 ,  Eve_Exe_buffer);
			memset(time_local,0,sizeof(time_local));
			if (xSemaphoreTake(EventActorMutex, portMAX_DELAY) == pdTRUE) //2000
			{
				cJSON *name_JSON = cJSON_Parse((char*) Eve_Exe_buffer);
				if (name_JSON == NULL) {
					// Handle parsing error
					sprintf(ExecuteMethodstr, "Invalid JSON input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
					xSemaphoreGive(EventActorMutex);
					goto exit;
				}
				else {
				// Parsing successful, proceed with extracting ScriptID
				cJSON *scriptid = cJSON_GetObjectItem(name_JSON, "ScriptID");
				if (scriptid != NULL && cJSON_IsNumber(scriptid)) {
					// "ScriptID" field exists and is a number
					u16ScriptID = scriptid->valueint;
					u8ActionIndex_avl_local=0;
					u16ScriptID_11 = u16ScriptID;
				}

				// Parsing successful, proceed with extracting ActionIndex
				cJSON *actionindex = cJSON_GetObjectItem(name_JSON, "ActionIndex");
				if (actionindex != NULL && cJSON_IsNumber(actionindex)) {
					// "ActionIndex" field exists and is a number
					u16ActionIndex = actionindex->valueint;
					u8ActionIndex_avl_local=1;
				}

				cJSON *deviceid = cJSON_GetObjectItem(name_JSON, "DEVICEID");
				if (deviceid != NULL && cJSON_IsString(deviceid)) {
					// "DeviceID" field exists and is a number
					strcpy(Deviceid, deviceid->valuestring);
				}

				cJSON *Event_ID_temp = cJSON_GetObjectItem(name_JSON, "Event_ID");
				if (Event_ID_temp != NULL && cJSON_IsNumber(Event_ID_temp)) {
					// "DeviceID" field exists and is a number
					EventID = Event_ID_temp->valueint;
					u8ActionIndex_avl_local=0;
				}

				// Parsing successful, proceed with extracting RampTime
				cJSON *ramptime = cJSON_GetObjectItem(name_JSON, "RampTime");
				if (ramptime != NULL && cJSON_IsNumber(ramptime)) {
					// "RampTime" field exists and is a number
					u32RampTimeScene = ramptime->valueint;
					u8ActionIndex_avl_local=0;
				}

				// Parsing successful, proceed with extracting Duration
				cJSON *durationItem = cJSON_GetObjectItem(name_JSON, "Duration");
				if (durationItem != NULL && cJSON_IsNumber(durationItem)) {
					// "durationItem" field exists and is a number
					u32DwellTimeScene = durationItem->valueint;
					u8ActionIndex_avl_local=0;
				}

#ifdef ENABLE_PRINT_MSG
				printf("RampTimeScene_Value 2 = %ld, DwellTimeScene_Value 2 = %ld \n", u32RampTimeScene, u32DwellTimeScene );
#endif

				cJSON *NotToExecute_ID_temp = cJSON_GetObjectItem(name_JSON, "NotToExecute");
				if (NotToExecute_ID_temp != NULL && cJSON_IsNumber(NotToExecute_ID_temp)) {
					// "DeviceID" field exists and is a number
					u8NotToExecuteFlag = NotToExecute_ID_temp->valueint;
				}
				else
					 u8NotToExecuteFlag=0;

				cJSON *startEPOC1_Temp = cJSON_GetObjectItem(name_JSON, "Start_Epoch");
				if (startEPOC1_Temp != NULL && cJSON_IsNumber(startEPOC1_Temp)) {
					// "DeviceID" field exists and is a number
					startEPOC1Temp = startEPOC1_Temp->valuedouble;
				}
				else
					startEPOC1Temp=0;

				cJSON *time_localjson1 = cJSON_GetObjectItem(name_JSON, "Local_Time");
				if (time_localjson1 != NULL && cJSON_IsString(time_localjson1)) {
					// "DeviceID" field exists and is a number
					strcpy(time_local, time_localjson1->valuestring);
				}


				cJSON *Override_Sport_temp = cJSON_GetObjectItem(name_JSON, "Override_Sport");
				if (Override_Sport_temp != NULL && cJSON_IsNumber(Override_Sport_temp)) {
					// "DeviceID" field exists and is a number
					OverrideSportVal = Override_Sport_temp->valueint;
					u8ActionIndex_avl_local=0;
				}

				// Clean up cJSON object
				cJSON_Delete(name_JSON);
			}
			// Proceed only if Device ID and Script ID are valid

			if(((strlen(Deviceid) != 0) && (u16ScriptID != 0)) || ((strlen(Deviceid) != 0) && (u16ActionIndex != 0)))
			{
				if(u8ActionIndex_avl_local == 1)
				{
					Script_ID_cnt = 1;
				}
				else
				{
					sprintf(ExecuteMethodstr, "SELECT COUNT() FROM Action_table WHERE ScriptId = %d;", u16ScriptID);
				}

				if(u8ActionIndex_avl_local == 0)
				{
					cJSON *min_max_JSON = cJSON_CreateObject();
					if(min_max_JSON != NULL)
					{
						cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Action.db");
						cJSON_AddStringToObject(min_max_JSON, "QUERY", ExecuteMethodstr);
						payLoadDataEvtExe[0]='\0';
						cJSON_PrintPreallocated(min_max_JSON, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
						Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "DB_EXECUTE");
						cJSON_Delete(min_max_JSON);
					}
					Script_ID_cnt = 0;

					if (pdTRUE == xQueueReceive(Script_Execute_Que2, (void*) Eve_Exe_buffer, 1000))
					{
						root = cJSON_Parse((char*) Eve_Exe_buffer);
						if (root == NULL)
						{
							sprintf(ExecuteMethodstr,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
							Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
							xSemaphoreGive(EventActorMutex);
							goto exit;
						}
						cJSON *count = cJSON_GetObjectItem(root, "COUNT()");
						if((count != NULL) && (cJSON_IsString(count)))
						{
							Script_ID_cnt = atoi(count->valuestring);
						}
						cJSON_Delete(root);
						root = NULL;
						if(Script_ID_cnt == 0)
						{
							sprintf(ExecuteMethodstr, "Script ID (%d) not found in the action table.",u16ScriptID);
							Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
						}
						else
						{
							if(u8ActionIndex_avl_local == 1)
							{
								sprintf(ExecuteMethodstr, "SELECT * FROM Action_table where ActionIndex = %d;", u16ActionIndex);
							}
							else
							{
								sprintf(ExecuteMethodstr, "SELECT * FROM Action_table where ScriptId = %d;", u16ScriptID);
							}

							cJSON *min_max_JSON = cJSON_CreateObject();
							if(min_max_JSON != NULL)
							{
								cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Action.db");
								cJSON_AddStringToObject(min_max_JSON, "QUERY", ExecuteMethodstr);

								payLoadDataEvtExe[0]='\0';
								cJSON_PrintPreallocated(min_max_JSON, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
								Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "DB_EXECUTE");

								cJSON_Delete(min_max_JSON);
							}
							Command_Array = cJSON_CreateArray();  // create array to store methods
							while(Script_ID_cnt > 0)
							{
								int defer_command = 0;
								if (pdTRUE == xQueueReceive(Script_Execute_Que2, (void*) Eve_Exe_buffer, 1000))
								{
									cJSON *root_new = cJSON_Parse((char*) Eve_Exe_buffer);
									if (root_new == NULL)
									{
										sprintf(ExecuteMethodstr,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
										Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
										xSemaphoreGive(EventActorMutex);
										goto exit;
									}
									cJSON *Action_Dev_ID = cJSON_GetObjectItemCaseSensitive(root_new, "DeviceId");
									if((Action_Dev_ID != NULL) && cJSON_IsString(Action_Dev_ID))
									{
										if(strcmp(Action_Dev_ID->valuestring, Deviceid) ==0)
										{
											device_ID_found = 1;
											cJSON *Action_Command = cJSON_GetObjectItemCaseSensitive(root_new, "Command");
											if((Action_Command != NULL) && cJSON_IsString(Action_Command))
											{
												cJSON *Act_cmd = cJSON_Parse((char*) Action_Command->valuestring);
												cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(Act_cmd, "methods");
												if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
												{
													int arraySize = cJSON_GetArraySize(methodsArray);
													// Loop through each element in the array
													for (int i = 0; i < arraySize; i++) {
														cJSON *element = cJSON_GetArrayItem(methodsArray, i);
														if (cJSON_IsString(element) && (element->valuestring != NULL)) {
															/* execute the command */
															if(strlen(element->valuestring) == 0)
															{
																Add_Response_msg("Methods array is empty",s_Message_Rx, payLoadDataEvtExe);
																continue;
															}
															strcpy(line, element->valuestring);
															esp_err_t err = ESP_OK;

															if((u8NotToExecuteFlag ==0) && (startEPOC1 == 0))
															{
																int ret;
																int command_dif = 0;

																if(execute_com == 1)
																{
																	if( (u32DwellTimeScene != 0) || (u32RampTimeScene != 0) )
																	{
																		// Find the position of the last closing brace '})' to insert the new key-value
																		char *pos = strstr(line, "})");
																		if (pos != NULL)
																		{
																			char value[10];

																			// Copy the part before '})' into the modified string
																			size_t prefix_len = pos - line;
																			strncpy(line_2, line, prefix_len);
																			line_2[prefix_len] = '\0';

																			// Append the new key-value pair
																				strcat(line_2,  ", \"RampTime\":");
																				sprintf(value, "%ld", u32RampTimeScene); // Convert the integer to a string
																				strcat(line_2, value);

																				strcat(line_2,  ", \"Duration\":");
																				sprintf(value, "%ld", u32DwellTimeScene); // Convert the integer to a string
																				strcat(line_2, value);
	//																		}

																			// Append the remaining part '})'
																			strcat(line_2, "})");
																			strcpy(line, line_2);
#ifdef ENABLE_PRINT_MSG
																			printf("line = %s \n", line);
#endif
																		}
																	}
																}
																else if (strstr(line, "LIGHTING.ON") != NULL)
																{
																	if((EVENT_Para.defer_on_u64 != 0) && (OverrideSportVal == 0))
																	{
																		struct timeval currentTime;
																		_gettimeofday_r(NULL, &currentTime, NULL);
																		uint64_t current_epos_sec11 = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
																		current_epos_sec11 = current_epos_sec11 + ((EVENT_Para.gmt_value)*60*1000);	// Added GMT for local time
																		if(current_epos_sec11 < EVENT_Para.defer_on_u64)
																		{
																			command_dif = 1;
																			defer_command =1;
																			Add_Response_msg("EVENT ON is deferred", s_Message_Rx, payLoadDataEvtExe);
																		}
																	}

																	char insert[20];
																	if(command_dif == 1)
																	{
																		strcpy(insert, ", \"DEFER_eventId\":");
																	}
																	else
																	{
																		strcpy(insert, ", \"eventId\":");
																	}
																	char value[10];
																	if(EventID == 0)
																	{
																		sprintf(value, "%ld", Global_EventID); // Convert the integer to a string
																	}
																	else
																	{
																		Global_EventID = 0;
																		sprintf(value, "%ld", EventID); // Convert the integer to a string
																	}

																	// Find the position of the last closing brace '})' to insert the new key-value
																	char *pos = strstr(line, "})");
																	if (pos != NULL)
																	{
																		// Copy the part before '})' into the modified string
																		size_t prefix_len = pos - line;
																		strncpy(line_2, line, prefix_len);
																		line_2[prefix_len] = '\0';

																		// Append the new key-value pair
																		strcat(line_2, insert);
																		strcat(line_2, value);

																		if( (u32DwellTimeScene != 0) || (u32RampTimeScene != 0) )
																		{
																			strcat(line_2,  ", \"RampTime\":");
																			sprintf(value, "%ld", u32RampTimeScene); // Convert the integer to a string
																			strcat(line_2, value);

																			strcat(line_2,  ", \"Duration\":");
																			sprintf(value, "%ld", u32DwellTimeScene); // Convert the integer to a string
																			strcat(line_2, value);
																		}

																		// Append the remaining part '})'
																		strcat(line_2, "})");
																		strcpy(line, line_2);
																	}
																 }
																 else if (strstr(line, "LIGHTING.OFF") != NULL)
																 {
																	if((EVENT_Para.defer_off_u64 != 0) && (OverrideSportVal == 0))
																	{
																		struct timeval currentTime;
																		_gettimeofday_r(NULL, &currentTime, NULL);
																		uint64_t current_epos_sec11 = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
																		current_epos_sec11 = current_epos_sec11 + ((EVENT_Para.gmt_value)*60*1000);	// Added GMT for local time
																		if(current_epos_sec11 < EVENT_Para.defer_off_u64)
																		{
																			command_dif = 1;
																			defer_command = 1;
																			Add_Response_msg("EVENT OFF is deferred", s_Message_Rx, payLoadDataEvtExe);
																		}
																	}
																	char insert[20];
																	if(command_dif == 1)
																	{
																		strcpy(insert, ", \"DEFER_eventId\":");
																	}
																	else
																	{
																		strcpy(insert, ", \"eventId\":");
																	}
																	char value[10];
																	if(EventID == 0)
																	{
																		sprintf(value, "%ld", Global_EventID); // Convert the integer to a string
																	}
																	else
																	{
																		Global_EventID = 0;
																		sprintf(value, "%ld", EventID); // Convert the integer to a string
																	}

																	// Find the position of the last closing brace '})' to insert the new key-value
																	char *pos = strstr(line, "})");
																	if (pos != NULL)
																	{
																		// Copy the part before '})' into the modified string
																		size_t prefix_len = pos - line;
																		strncpy(line_2, line, prefix_len);
																		line_2[prefix_len] = '\0';

																		// Append the new key-value pair
																		strcat(line_2, insert);
																		strcat(line_2, value);

																		if( (u32DwellTimeScene != 0) || (u32RampTimeScene != 0) )
																		{
																			strcat(line_2,  ", \"RampTime\":");
																			sprintf(value, "%ld", u32RampTimeScene); // Convert the integer to a string
																			strcat(line_2, value);

																			strcat(line_2,  ", \"Duration\":");
																			sprintf(value, "%ld", u32DwellTimeScene); // Convert the integer to a string
																			strcat(line_2, value);
																		}

																		// Append the remaining part '})'
																		strcat(line_2, "})");
																		strcpy(line, line_2);
																		//printf("line = %s \n", line);
																	}
																 }
																 else if (strstr(line, "EVENT_ACTOR.EXECUTESCENE") != NULL)
																 {

																	Global_EventID = EventID;
																	char insert[] = ", \"EVENT_SCENE\":";
																	char value[10];
	//																char line_2[100];
																	sprintf(value, "%d", 1); // Convert the integer to a string

																	// Find the position of the last closing brace '})' to insert the new key-value
																	char *pos = strstr(line, "})");
																	if (pos != NULL) {
																		// Copy the part before '})' into the modified string
																		size_t prefix_len = pos - line;
																		strncpy(line_2, line, prefix_len);
																		line_2[prefix_len] = '\0';

																		// Append the new key-value pair
																		strcat(line_2, insert);
																		strcat(line_2, value);

																		if( (u32DwellTimeScene != 0) || (u32RampTimeScene != 0) )
																		{
																			strcat(line_2,  ", \"RampTime\":");
																			sprintf(value, "%ld", u32RampTimeScene); // Convert the integer to a string
																			strcat(line_2, value);

																			strcat(line_2,  ", \"Duration\":");
																			sprintf(value, "%ld", u32DwellTimeScene); // Convert the integer to a string
																			strcat(line_2, value);
																		}

																		// Append the remaining part '})'
																		strcat(line_2, "})");
																		strcpy(line, line_2);
	//																	------------------------------------------------------
																		{
																			const char *key = "\"scriptId\":"; // Key to search for
																			char *pos1 = strstr(line, key);       // Find the position of the key
																			if (pos1 != NULL) {
																				int script_id;
																				sscanf(pos1 + strlen(key), "%d", &script_id); // Extract the integer value
																				u16ScriptID =  script_id;
																				if(u16ScriptID_11 == u16ScriptID)
																				{
																					Add_Response_msg("Error! As Scene ID and schedule ID are same, cannot execute the event.", s_Message_Rx, payLoadDataEvtExe);
																					xSemaphoreGive(EventActorMutex);
																					cJSON_Delete(root);
																					cJSON_Delete(Act_cmd);  // Free memory
																					if(root_new != NULL)
																					{
																						cJSON_Delete(root_new);
																						root_new = NULL;
																					}
																					goto exit;
																				}
																			}
																			// Proceed only if Device ID and Script ID are valid

																			if(u16ScriptID != 0)
																			{
																				sprintf(ExecuteMethodstr, "SELECT COUNT() FROM Action_table WHERE ScriptId = %d;", u16ScriptID);
																				cJSON *min_max_JSON = cJSON_CreateObject();
																				if(min_max_JSON != NULL)
																				{
																					cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Action.db");
																					cJSON_AddStringToObject(min_max_JSON, "QUERY", ExecuteMethodstr);
																					payLoadDataEvtExe[0]='\0';
																					cJSON_PrintPreallocated(min_max_JSON, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
																					Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "DB_EXECUTE");
																					cJSON_Delete(min_max_JSON);
																				}

																				Script_ID_cnt = 0;
																				if (pdTRUE == xQueueReceive(Script_Execute_Que2, (void*) Eve_Exe_buffer, 1000))
																				{
																					root = cJSON_Parse((char*) Eve_Exe_buffer);
																					if (root == NULL)
																					{
																						sprintf(ExecuteMethodstr,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
																						Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
																						xSemaphoreGive(EventActorMutex);
																						goto exit;
																					}
																					cJSON *count = cJSON_GetObjectItem(root, "COUNT()");
																					if((count != NULL) && (cJSON_IsString(count)))
																					{
																						Script_ID_cnt = atoi(count->valuestring);
																					}
																					cJSON_Delete(root);
																					root = NULL;
																					if(Script_ID_cnt == 0)
																					{
																						sprintf(ExecuteMethodstr, "Script ID (%d) not found in the action table.",u16ScriptID);
																						Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
																					}
																					else
																					{
																						sprintf(ExecuteMethodstr, "SELECT * FROM Action_table where ScriptId = %d;", u16ScriptID);
																						cJSON *min_max_JSON = cJSON_CreateObject();
																						if(min_max_JSON != NULL)
																						{
																							cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Action.db");
																							cJSON_AddStringToObject(min_max_JSON, "QUERY", ExecuteMethodstr);
																							payLoadDataEvtExe[0]='\0';
																							cJSON_PrintPreallocated(min_max_JSON, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
																							Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "DB_EXECUTE");

																							cJSON_Delete(min_max_JSON);
																						}
																						Command_Array = cJSON_CreateArray();  // create array to store methods
																						while(Script_ID_cnt > 0)
																						{
																							if (pdTRUE == xQueueReceive(Script_Execute_Que2, (void*) Eve_Exe_buffer, 1000))
																							{
																								struct timeval currentTime;
																								_gettimeofday_r(NULL, &currentTime, NULL);
																								uint64_t current_epos_sec11 = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);

																								current_epos_sec11 = current_epos_sec11 + ((EVENT_Para.gmt_value)*60*1000);	// Added GMT for local time

																								if (strstr(Eve_Exe_buffer, "LIGHTING.ON") != NULL)
																								{
																									if((current_epos_sec11 < EVENT_Para.defer_on_u64) && (OverrideSportVal == 0))
																									{
																										defer_command =1;
																										Add_Response_msg("EVENT Scene ON is deferred", s_Message_Rx, payLoadDataEvtExe);
																									}
																								}
																								else if (strstr(Eve_Exe_buffer, "LIGHTING.OFF") != NULL)
																								{
																									if((current_epos_sec11 < EVENT_Para.defer_off_u64) && (OverrideSportVal == 0))
																									{
																										defer_command =1;
																										Add_Response_msg("EVENT Scene OFF is deferred", s_Message_Rx, payLoadDataEvtExe);
																									}
																								}

																								Script_ID_cnt--;
																							}
																							vTaskDelay(10 / portTICK_PERIOD_MS);
																						}  //end of for loop
																					}
																				}
																			}
																		}
	//																	---------------------------------------------------------


																}
															 }
															err = esp_console_run_Custom(line, &ret, THIS_ACTOR);
														}
														if (err == ESP_OK)
														{

															if(u8ActionIndex_avl_local == 1)
															{
																sprintf(ExecuteMethodstr,"EVENT ACTION INDEX: %d  METHOD: %s", u16ActionIndex,line);
															}
															else
															{
																sprintf(ExecuteMethodstr,"EVENT SCRIPT ID: %d  METHOD: %s", u16ScriptID,line);
															}

															Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
															cJSON_AddItemToArray(Command_Array, cJSON_Duplicate(methodsArray, 1));
															Event_execute_flag = 1;
															Script_ID_cnt--;
														}
														else
														{
															sprintf(ExecuteMethodstr,"Error in executing method: %s\n", esp_err_to_name(err));
															Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
														}
														}
	//													xSemaphoreGive(arrayMutex);
														vTaskDelay(10 / portTICK_PERIOD_MS);
	//													i++;
													}  //end of for loop

	//												}
											}

											if (Act_cmd != NULL)
											{
												cJSON_Delete(Act_cmd);  // Free memory
											}

										} // end of Action_Command

									}  // end of if(strcmp(Action_Dev_ID, Deviceid) ==0)
									else  // device is not matched
										Script_ID_cnt--;
								}
								if((Event_execute_flag == 1)  && (EventID != 0))
								{
									Event_execute_flag = 0;
									//send D2C message to the server to inform about the event execution
									D2CMSG = cJSON_CreateObject();
									if(D2CMSG != NULL)
									{
										if(startEPOC1Temp != 0)
										cJSON_AddNumberToObject(D2CMSG, "TimeWarp", startEPOC1Temp);
										if( defer_command == 1)
										{
											cJSON_AddNumberToObject(D2CMSG, "DEFER_eventId", EventID);
											strcpy(Global_EventString, "DEFER_eventId");
										}
										else
										{
											cJSON_AddNumberToObject(D2CMSG, "eventId", EventID);
											strcpy(Global_EventString, "eventId");
										}
										if(startEPOC1Temp == 0)
										sprintf(ExecuteMethodstr,"%02d-%02d-%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month, UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
										else
											sprintf(ExecuteMethodstr,"%s",time_local ); // Years since 1900

										cJSON_AddStringToObject(D2CMSG, "locationExecutionTime", ExecuteMethodstr);
										cJSON_AddItemToObject(D2CMSG, "commands", Command_Array);

										payLoadDataEvtExe[0]='\0';
										cJSON_PrintPreallocated(D2CMSG, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
										strcat(payLoadDataEvtExe, "\n");
										if(startEPOC1Temp != 0)
										{
											Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "SAVE_EVENT_LOG");
										}
										if(D2CMSG != NULL)
										{
											cJSON_Delete(D2CMSG);
											D2CMSG = NULL;
											Command_Array = NULL;
										}
									}
									else
										Add_Response_msg("Error! failed to allocate memory for JSON object to send D2C message", s_Message_Rx, payLoadDataEvtExe);
									EventID = 0;
								}
								if(root_new != NULL)
								{
									cJSON_Delete(root_new);
									root_new = NULL;
								}

							} // end of que receive for methods
						} // end of while
						if(device_ID_found == 0)
						{
							sprintf(ExecuteMethodstr, "Received method is not for %s Device ID ", Deviceid);
							Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
		//					Script_ID_cnt--;
						}
						else
						{
							device_ID_found = 0;
						}
					} // end of else (Script_ID_cnt == 0)

				} // end of q receive for srcipt id count
			}
			else
			{
//Action index
				if(u8ActionIndex_avl_local == 1)
				{
					sprintf(ExecuteMethodstr, "SELECT * FROM Action_table where ActionIndex = %d;", u16ActionIndex);

					cJSON *min_max_JSON = cJSON_CreateObject();
					if(min_max_JSON != NULL)
					{
						cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Action.db");
						cJSON_AddStringToObject(min_max_JSON, "QUERY", ExecuteMethodstr);

						payLoadDataEvtExe[0]='\0';
						cJSON_PrintPreallocated(min_max_JSON, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
						cJSON_Delete(min_max_JSON);
						Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "DB_EXECUTE");
					}
					while(Script_ID_cnt > 0)
					{
						if (pdTRUE == xQueueReceive(Script_Execute_Que2, (void*) Eve_Exe_buffer, 1000))
						{
							cJSON *root_new = cJSON_Parse((char*) Eve_Exe_buffer);
							if (root_new == NULL)
							{
								sprintf(ExecuteMethodstr,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
								Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
								xSemaphoreGive(EventActorMutex);
								goto exit;
							}
							cJSON *Action_Dev_ID = cJSON_GetObjectItemCaseSensitive(root_new, "DeviceId");
							if((Action_Dev_ID != NULL) && cJSON_IsString(Action_Dev_ID))
							{
								if(strcmp(Action_Dev_ID->valuestring, Deviceid) ==0)
								{
									device_ID_found = 1;
									cJSON *Action_Command = cJSON_GetObjectItemCaseSensitive(root_new, "Command");
									if((Action_Command != NULL) && cJSON_IsString(Action_Command))
									{
										cJSON *Act_cmd = cJSON_Parse((char*) Action_Command->valuestring);
										cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(Act_cmd, "methods");
										if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
										{
											int arraySize = cJSON_GetArraySize(methodsArray);
											// Loop through each element in the array
											for (int i = 0; i < arraySize; i++) {
												cJSON *element = cJSON_GetArrayItem(methodsArray, i);
												if (cJSON_IsString(element) && (element->valuestring != NULL)) {
													/* execute the command */
													if(strlen(element->valuestring) == 0)
													{
														Add_Response_msg("Methods array is empty",s_Message_Rx, payLoadDataEvtExe);
														continue;
													}
													strcpy(line, element->valuestring);

													esp_err_t err = ESP_OK;

													{
														int ret;
														{
															err = esp_console_run_Custom(line, &ret, THIS_ACTOR);
														}
													}

													if (err == ESP_OK)
													{
														sprintf(ExecuteMethodstr,"EVENT ACTION INDEX: %d  METHOD: %s", u16ActionIndex,line);
														Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
														Event_execute_flag = 1;
														Script_ID_cnt--;
													}
													else
													{
														sprintf(ExecuteMethodstr,"Error in executing method: %s\n", esp_err_to_name(err));
														Add_Response_msg(ExecuteMethodstr, s_Message_Rx, payLoadDataEvtExe);
													}
												}
	//													xSemaphoreGive(arrayMutex);
												vTaskDelay(1 / portTICK_PERIOD_MS);
	//													i++;
											}  //end of for loop
									}

									if (Act_cmd != NULL)
									{
									    cJSON_Delete(Act_cmd);  // Free memory
									}

								} // end of Action_Command
							}  // end of if(strcmp(Action_Dev_ID, Deviceid) ==0)
							else  // device is not matched
								Script_ID_cnt--;
						}

						if(root_new != NULL)
						{
							cJSON_Delete(root_new);
							root_new = NULL;
						}

					} // end of que receive for methods
				} // end of while
				if(device_ID_found == 0)
				{
					sprintf(ExecuteMethodstr, "Received method is not for %s Device ID ", Deviceid);
					Add_Response_msg(ExecuteMethodstr,s_Message_Rx, payLoadDataEvtExe);
				}
				else
				{
					device_ID_found = 0;
				}
				}
				}

				if(Event_Execution_Flag != Enable)
				{

					xSemaphoreGive(EventActorMutex);
					goto exit;
				}
			} // end of if((strlen(Deviceid) != 0) && (u16ScriptID != 0))
			xSemaphoreGive(EventActorMutex);
		}  // end of xSemaphoreTake
	  }  // end of xQueueReceive for script id
	} // end of while(1)

    exit:
	printf("\r\nDeleting Task \r\n");
		ExecuteMethodForEventHandle = NULL;
		vTaskDelete(ExecuteMethodForEventHandle);  // Delete the task
}

static void Update_Location_Table(void)
{
	char str[200] = {0};
	sprintf(str, "SELECT * FROM Location_table;");
	cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
	cJSON_AddStringToObject(my_JSON1, "FILE_NAME", "A:/DATABASE/Location.db");
	cJSON_AddStringToObject(my_JSON1, "QUERY", str);
//	char* Location_payload = cJSON_PrintUnformatted(my_JSON1);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);

	Send_CMD_To_Other_Actor(SQL,"SQL", payLoadData, strlen(payLoadData), "DB_EXECUTE");
	cJSON_Delete(my_JSON1); // Free the cJSON object
//	cJSON_free(Location_payload); // Free the memory allocated by cJSON_PrintUnformatted
}

void Print_Schedule_Table(int end_record)
{
    // Ensure valid range for printing
    if (end_record <= 0)
    {
        printf("No schedule records to display.\r\n");
        return;
    }

    printf("----------------------------- Schedule Table -----------------------------\r\n");
    for (int i = 0; i < end_record; i++)
    {
        printf("\r\nRecord %d:\r\n", i + 1);
        printf("\tEventId:\t\t%ld\r\n", Schedule_table[i].EventId);
        printf("\tScriptID:\t\t%d\r\n", Schedule_table[i].ScriptID);
        printf("\tDate_Range_Type:\t%d\r\n", Schedule_table[i].Date_Range_Type);
        printf("\tStart Month:\t\t%d\r\n", Schedule_table[i].Start_month);
        printf("\tStart Day:\t\t%d\r\n", Schedule_table[i].Start_day);
        printf("\tEnd Month:\t\t%d\r\n", Schedule_table[i].End_month);
        printf("\tEnd Day:\t\t%d\r\n", Schedule_table[i].End_day);
        printf("\tWeekDays:\t\t%d\r\n", Schedule_table[i].WeekDays);
        printf("\tTime_type:\t\t%d\r\n", Schedule_table[i].Time_type);
        printf("\tHolidayStartIndex:\t%d\r\n", Schedule_table[i].HolidayStartIndex);
        printf("\tHolidayStartOffset:\t%d\r\n", Schedule_table[i].HolidayStartOffset);
        printf("\tHolidayEndIndex:\t%d\r\n", Schedule_table[i].HolidayEndIndex);
        printf("\tHolidayEndOffset:\t%d\r\n", Schedule_table[i].HolidayEndOffset);
        printf("\tOffsetMinutes:\t\t%d\r\n", Schedule_table[i].OffsetMinutes);
        printf("\tHour:\t\t\t%d\r\n", Schedule_table[i].Hour);
        printf("\tMinute:\t\t\t%d\r\n", Schedule_table[i].Minute);
        printf("\tSortOrder:\t\t%d", Schedule_table[i].SortOrder);
        printf("\tOverrideSport:\t\t%d", Schedule_table[i].OverrideSport);
        printf("\tRampTime:\t\t%ld\r\n", Schedule_table[i].RampTime);
        printf("\tDuration:\t\t%ld\r\n", Schedule_table[i].Duration);

    }
}
static void Update_Schedule_Table(void *pvParameters __attribute__((unused)))
{
    char str[200] = {0};
    cJSON *root_new = NULL;     // JSON root object
    cJSON *name_new_JSON = NULL; // JSON object for specific data extraction
    int  End_Record = 0, Start_Record = 0; // Record counters and boundaries  //schedule_record = 0,
    int records_processed = 0;  // Counter for processed records
	AMessage_st* s_Message_Rx1 = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx2; 
	AMessage_st* s_Message_Rx=&s_Message_Rx2;
	
	memcpy(s_Message_Rx,s_Message_Rx1,sizeof(AMessage_st));
	
	s_Message_Rx->payload_p8 = (uint8_t*) Sch_Tab_data_buffer;
    char out[100] = {0};        // Buffer for output commands
    int32_t file_size = 0;      // Variable to store the file size
    const int batch_size = 64;  // Set the batch size to 50 records per batch
    const int max_retries = 3;  // Maximum number of retries for queue operations
    int retry_count = 0;        // Retry counter for the queue receives

    // Check if event execution is enabled
    if (Event_Execution_Flag != Enable)
        goto exit;

    printf("****************************************Schedule Table****************************************\r\n");

    // Request the file size of the schedule database
    strcpy(out, "{\"FILE_NAME\":\"A:/Database/Schedule.db\"}");
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", out, strlen(out), "GET_FILE_SIZE");
    if (xSemaphoreTake(EventActorMutex, portMAX_DELAY) == pdTRUE) //2000

    {
    // Wait for the file size response with retry logic
    while (retry_count < max_retries)
    {
        if (pdTRUE == xQueueReceive(Schedule_Record_Que, (void*)Sch_Tab_buffer, 5000))
        {
            root_new = cJSON_Parse((char*)Sch_Tab_buffer); // Parse the received JSON data
            if (root_new == NULL)
            {
            	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
                Add_Response_msg(str, s_Message_Rx, payLoadDataUpdate_sch);
                xSemaphoreGive(EventActorMutex);
                goto exit;
            }

            // Extract the file size from the JSON response
            name_new_JSON = cJSON_GetObjectItem(root_new, "FILE_SIZE");
            if ((name_new_JSON != NULL) && cJSON_IsNumber(name_new_JSON))
            {
                file_size = name_new_JSON->valueint;
                break; // Exit the retry loop if successful
            }

            cJSON_Delete(root_new);
        }

        retry_count++; // Increment the retry count
        if (retry_count >= max_retries)
        {
            sprintf(str, "Failed to get file size after %d retries", max_retries);
            Add_Response_msg(str, s_Message_Rx, payLoadDataUpdate_sch);
            xSemaphoreGive(EventActorMutex);
            goto exit;
        }
    }

    // Handle cases where the file size is zero or invalid
    if (file_size <= 0)
    {
    	 xSemaphoreGive(EventActorMutex);
        goto exit;
    }

    // Reset retry count for the next queue receive operation
    retry_count = 0;

    // Query the minimum and maximum row IDs in the schedule table
    sprintf(str, "SELECT MIN(rowid) AS SCH_MIN_ID, MAX(rowid) AS SCH_MAX_ID FROM Schedule_table;");
    cJSON *min_max_JSON = cJSON_CreateObject();
    cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/schedule.db");
    cJSON_AddStringToObject(min_max_JSON, "QUERY", str);
//    char* out_val_min_max = cJSON_PrintUnformatted(min_max_JSON);
    payLoadDataUpdate_sch[0]='\0';
	cJSON_PrintPreallocated(min_max_JSON, payLoadDataUpdate_sch, sizeof(payLoadDataUpdate_sch), false);
    Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataUpdate_sch, strlen(payLoadDataUpdate_sch), "DB_EXECUTE");
    cJSON_Delete(min_max_JSON);

    // Wait for the min/max row ID response with retry logic
    while (retry_count < max_retries)
    {
        if (pdTRUE == xQueueReceive(Schedule_Record_Que, (void*)Sch_Tab_buffer, 5000))
        {
            root_new = cJSON_Parse((char*)Sch_Tab_buffer);
            if (root_new == NULL)
            {
            	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
                Add_Response_msg(str, s_Message_Rx, payLoadDataUpdate_sch);
                xSemaphoreGive(EventActorMutex);
                goto exit;
            }

            // Extract the maximum and minimum row IDs
            name_new_JSON = cJSON_GetObjectItem(root_new, "SCH_MAX_ID");
            if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                End_Record = atoi(name_new_JSON->valuestring);

            name_new_JSON = cJSON_GetObjectItem(root_new, "SCH_MIN_ID");
            if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                Start_Record = atoi(name_new_JSON->valuestring);

            cJSON_Delete(root_new);
            break; // Exit the retry loop if successful
        }

        retry_count++; // Increment the retry count
        if (retry_count >= max_retries)
        {
            sprintf(str, "Failed to get min/max record IDs after %d retries", max_retries);
            Add_Response_msg(str, s_Message_Rx, payLoadDataUpdate_sch);
            xSemaphoreGive(EventActorMutex);
            goto exit;
        }
    }

    // Initialize the allocated memory to zero
    memset(Schedule_table, 0, sizeof(Schedule_table));

    // Process records in batches
    while (Start_Record <= End_Record)
    {
        // Calculate the number of records to fetch in the current batch
        int records_to_fetch = (Start_Record + batch_size - 1 <= End_Record) ? batch_size : (End_Record - Start_Record + 1);

        // Query to fetch the next batch of records starting from Start_Record
        sprintf(str, "SELECT * FROM Schedule_table WHERE rowid >= %d LIMIT %d;", Start_Record, records_to_fetch);
        cJSON *batch_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(batch_JSON, "FILE_NAME", "A:/DATABASE/schedule.db");
        cJSON_AddStringToObject(batch_JSON, "QUERY", str);
//        char* Schedule_payload = cJSON_PrintUnformatted(batch_JSON);
        payLoadDataUpdate_sch[0]='\0';
    	cJSON_PrintPreallocated(batch_JSON, payLoadDataUpdate_sch, sizeof(payLoadDataUpdate_sch), false);

        Send_CMD_To_Other_Actor(SQL, "SQL", payLoadDataUpdate_sch, strlen(payLoadDataUpdate_sch), "DB_EXECUTE");
        cJSON_Delete(batch_JSON);

        // Process each record in the batch
        for (int i = 0; i < records_to_fetch; i++)
        {
            if (pdTRUE == xQueueReceive(Schedule_Record_Que, (void*)Sch_Tab_buffer, 5000))
            {
                root_new = cJSON_Parse((char*)Sch_Tab_buffer);
                if (root_new == NULL)
                {
                	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
                    Add_Response_msg(str, s_Message_Rx, payLoadDataUpdate_sch);
                    xSemaphoreGive(EventActorMutex);
                    goto exit;
                }

                // Ensure valid JSON object before accessing its fields
                if (root_new != NULL)
                {
                    // Extract fields and store them in the schedule table
                    name_new_JSON = cJSON_GetObjectItem(root_new, "EventId");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].EventId = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "ScriptId");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].ScriptID = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "DateType");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Date_Range_Type = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "StartMonth");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Start_month = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "StartDay");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Start_day = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "EndMonth");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].End_month = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "EndDay");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].End_day = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "WeekDays");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                    {
                        Schedule_table[Start_Record + records_processed - 1].WeekDays = 0;
                        for (int j = (strlen(name_new_JSON->valuestring) - 1); j >= 0; j--)
                        {
                            switch (name_new_JSON->valuestring[j])
                            {
                                case 'M': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 6); break;
                                case 'T': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 5); break;
                                case 'W': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 4); break;
                                case 'R': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 3); break;
                                case 'F': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 2); break;
                                case 'A': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 1); break;
                                case 'S': Schedule_table[Start_Record + records_processed - 1].WeekDays |= (1 << 0); break;
                                default: break;
                            }
                        }
                    }

                    name_new_JSON = cJSON_GetObjectItem(root_new, "TimeType");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Time_type = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "HolidayStartIndex");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].HolidayStartIndex = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "HolidayStartOffset");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].HolidayStartOffset = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "HolidayEndIndex");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].HolidayEndIndex = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "HolidayEndOffset");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].HolidayEndOffset = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "OffsetMinutes");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].OffsetMinutes = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "Hour");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Hour = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "Minute");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Minute = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "SortOrder");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].SortOrder = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "OverrideSport");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].OverrideSport = atoi(name_new_JSON->valuestring);


                    name_new_JSON = cJSON_GetObjectItem(root_new, "RampTime");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].RampTime = atoi(name_new_JSON->valuestring);

                    name_new_JSON = cJSON_GetObjectItem(root_new, "Duration");
                    if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL))
                        Schedule_table[Start_Record + records_processed - 1].Duration = atoi(name_new_JSON->valuestring);

//                    printf("OverrideSport = %d \n", Schedule_table[Start_Record + records_processed - 1].OverrideSport);

                    cJSON_Delete(root_new);

                    // Increment the processed records count
                    records_processed++;
                }
            }
            else
            {
            	 xSemaphoreGive(EventActorMutex);
                goto exit;
            }
        }

        Start_Record += records_processed; // Move to the next batch
        records_processed = 0; // Reset the processed records counter for the next batch
    }
    xSemaphoreGive(EventActorMutex);
}

exit:
    UpdateSchdule_Handle = NULL;
    vTaskDelete(UpdateSchdule_Handle); // Delete the task
}

/* Convert degree angle to radians */
static double  degToRad(double angleDeg)
{
  return (M_PI * angleDeg / 180.0);
}
static double radToDeg(double angleRad)
{
  return (180.0 * angleRad / M_PI);
}
static double calcMeanObliquityOfEcliptic(double t)
{
  double seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
  double e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;
  return e0;              // in degrees
}
static double calcGeomMeanLongSun(double t)
{
  double L = 280.46646 + t * (36000.76983 + 0.0003032 * t);
  while( (int) L >  360 )
    {
      L -= 360.0;
    }
  while(  L <  0)
    {
      L += 360.0;
    }
  return L;              // in degrees
}
static double calcObliquityCorrection(double t)
{
  double e0 = calcMeanObliquityOfEcliptic(t);
  double omega = 125.04 - 1934.136 * t;
  double e = e0 + 0.00256 * cos(degToRad(omega));
  return e;               // in degrees
}
static double calcEccentricityEarthOrbit(double t)
{
  double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  return e;               // unitless
}
static double calcGeomMeanAnomalySun(double t)
{
  double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  return M;               // in degrees
}
static double calcEquationOfTime(double t)
{
  double epsilon = calcObliquityCorrection(t);
  double  l0 = calcGeomMeanLongSun(t);
  double e = calcEccentricityEarthOrbit(t);
  double m = calcGeomMeanAnomalySun(t);
  double y = tan(degToRad(epsilon)/2.0);
	y *= y;
  double sin2l0 = sin(2.0 * degToRad(l0));
  double sinm   = sin(degToRad(m));
  double cos2l0 = cos(2.0 * degToRad(l0));
  double sin4l0 = sin(4.0 * degToRad(l0));
  double sin2m  = sin(2.0 * degToRad(m));
  double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0
				- 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
  return radToDeg(Etime)*4.0;	// in minutes of time
	}
static double calcTimeJulianCent(double jd)
{
  double T = ( jd - 2451545.0)/36525.0;
  return T;
}
static double calcSunTrueLong(double t)
{
  double l0 = calcGeomMeanLongSun(t);
  double c = calcSunEqOfCenter(t);
  double O = l0 + c;
  return O;               // in degrees
}
static double calcSunApparentLong(double t)
{
  double o = calcSunTrueLong(t);
  double  omega = 125.04 - 1934.136 * t;
  double  lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
  return lambda;          // in degrees
}
static double calcSunDeclination(double t)
{
  double e = calcObliquityCorrection(t);
  double lambda = calcSunApparentLong(t);
  double sint = sin(degToRad(e)) * sin(degToRad(lambda));
  double theta = radToDeg(asin(sint));
  return theta;           // in degrees
}
static double calcHourAngleSunrise(double lat, double solarDec)
{
  double latRad = degToRad(lat);
  double sdRad  = degToRad(solarDec);
  double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));
  return HA;              // in radians
}
static double calcHourAngleSunset(double lat, double solarDec)
{
  double latRad = degToRad(lat);
  double sdRad  = degToRad(solarDec);
  double HA = (acos(cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad)));
  return -HA;              // in radians
}
static double calcJD(int year,int month,int day)
	{
		if (month <= 2) {
			year -= 1;
			month += 12;
		}
		int A = floor(year/100);
		int B = 2 - A + floor(A/4);
		double JD = floor(365.25*(year + 4716)) + floor(30.6001*(month+1)) + day + B - 1524.5;
		return JD;
	}
static double calcJDFromJulianCent(double t)
{
  double JD = t * 36525.0 + 2451545.0;
  return JD;
}
static double calcSunEqOfCenter(double t)
{
		double m = calcGeomMeanAnomalySun(t);
		double mrad = degToRad(m);
		double sinm = sin(mrad);
		double sin2m = sin(mrad+mrad);
		double sin3m = sin(mrad+mrad+mrad);
		double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;
		return C;		// in degrees
}
static double calcSunriseUTC(double JD, double latitude, double longitude)
 {

	double t = calcTimeJulianCent(JD);// *** First pass to approximate sunrise
	double  eqTime = calcEquationOfTime(t);
	double  solarDec = calcSunDeclination(t);
	double  hourAngle = calcHourAngleSunrise(latitude, solarDec);
  double  delta = longitude - radToDeg(hourAngle);
	double  timeDiff = 4 * delta;	// in minutes of time
	double  timeUTC = 720 + timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);
					eqTime = calcEquationOfTime(newt);
					solarDec = calcSunDeclination(newt);
					hourAngle = calcHourAngleSunrise(latitude, solarDec);
					delta = longitude - radToDeg(hourAngle);
					timeDiff = 4 * delta;
					timeUTC = 720 + timeDiff - eqTime; // in minutes
		return timeUTC;
	}

static double calcSunsetUTC(double JD, double latitude, double longitude)
 {
	double t = calcTimeJulianCent(JD);   // *** First pass to approximate sunset
	double  eqTime = calcEquationOfTime(t);
	double  solarDec = calcSunDeclination(t);
	double  hourAngle = calcHourAngleSunset(latitude, solarDec);
  double  delta = longitude - radToDeg(hourAngle);
	double  timeDiff = 4 * delta;	// in minutes of time
	double  timeUTC = 720 + timeDiff - eqTime;	// in minutes
  double  newt = calcTimeJulianCent(calcJDFromJulianCent(t) + timeUTC/1440.0);
					eqTime = calcEquationOfTime(newt);
					solarDec = calcSunDeclination(newt);
					hourAngle = calcHourAngleSunset(latitude, solarDec);
					delta = longitude - radToDeg(hourAngle);
					timeDiff = 4 * delta;
					timeUTC = 720 + timeDiff - eqTime; // in minutes
		// printf("************ eqTime = %f  \nsolarDec = %f \ntimeUTC = %f\n\n",eqTime,solarDec,timeUTC);
		return timeUTC;
	}


static int sunset_function(double latitude,double longitude,int year,int month,int day,uint8_t*sunset_time)
{
  time_t seconds;
  time_t tseconds;
  struct tm  tm;
  char buffer[30];
  float JD=calcJD(year,month,day);
	double Latitude;
	double Longitude;


   Latitude = latitude;  //convert to just degrees.  No min/sec
   Longitude= longitude ;//convert to just degrees.  No min/sec
year=2000+year;
tm.tm_year= year-1900;
  tm.tm_mon=month-1;  /* Jan = 0, Feb = 1,.. Dec = 11 */
  tm.tm_mday=day;
  tm.tm_hour=0;
  tm.tm_min=0;
  tm.tm_sec=0;
  tm.tm_isdst= EVENT_Para.dst_value;                //-1;
  seconds = mktime(&tm);
//	int delta;
//  delta= EVENT_Para.gmt_value * 60;
  tseconds= seconds;


  seconds=tseconds;
  seconds+=calcSunsetUTC( JD,  Latitude,  Longitude)*60;
  //seconds= seconds - delta*3600+19800;
	strftime(buffer,30,"%T",localtime(&seconds));
  sprintf((char*)sunset_time,"%s",buffer);


  return 0;
}
static int sunrise_function(double latitude,double longitude,int year,int month,int day,uint8_t *sunrise_time)
{
  time_t seconds;
//  time_t tseconds;
//  struct tm  *ptm=NULL;
  struct tm  tm;
  char buffer[30];
  float JD=calcJD(year,month,day);
  double Latitude;
  double Longitude;


   Latitude = latitude;  //convert to just degrees.  No min/sec
   Longitude= longitude  ;  //convert to just degrees.  No min/sec
	year=2000+year;
	tm.tm_year= year-1900;
  tm.tm_mon=month-1;  /* Jan = 0, Feb = 1,.. Dec = 11 */
  tm.tm_mday=day;
  tm.tm_hour=0;
  tm.tm_min=0;
  tm.tm_sec=0;
  tm.tm_isdst=EVENT_Para.dst_value;  //-1
  seconds = mktime(&tm);
  seconds= seconds + calcSunriseUTC( JD,  Latitude,  Longitude)*60;
  strftime(buffer,30,"%T",localtime(&seconds));
  sprintf((char*)sunrise_time,"%s",buffer);
  return 0;
}



static uint64_t sunset_function_seconds(double latitude,double longitude,int year,int month,int day)
{
  time_t seconds;
  time_t tseconds;
  struct tm  tm;
  float JD=calcJD(year,month,day);
  double Latitude;
  double Longitude;
  Latitude = latitude;  //convert to just degrees.  No min/sec
  Longitude= longitude ;//convert to just degrees.  No min/sec
  year=2000+year;
  tm.tm_year= year-1900;
  tm.tm_mon=month-1;  /* Jan = 0, Feb = 1,.. Dec = 11 */
  tm.tm_mday=day;
  tm.tm_hour=0;
  tm.tm_min=0;
  tm.tm_sec=0;
  tm.tm_isdst= EVENT_Para.dst_value;                //-1;
  seconds = mktime(&tm);
  tseconds= seconds;
  seconds=tseconds;
  seconds+=calcSunsetUTC( JD,  Latitude,  Longitude)*60;
  return seconds;
}

static uint64_t sunrise_function_seconds(double latitude,double longitude,int year,int month,int day)
{
  time_t seconds;
  struct tm  tm;
  float JD=calcJD(year,month,day);
  double Longitude;
  double Latitude;
  Latitude = latitude;  //convert to just degrees.  No min/sec
  Longitude= longitude  ;  //convert to just degrees.  No min/sec
  year=2000+year;
  tm.tm_year= year-1900;
  tm.tm_mon=month-1;  /* Jan = 0, Feb = 1,.. Dec = 11 */
  tm.tm_mday=day;
  tm.tm_hour=0;
  tm.tm_min=0;
  tm.tm_sec=0;
  tm.tm_isdst=EVENT_Para.dst_value;  //-1
  seconds = mktime(&tm);
  seconds= seconds + calcSunriseUTC( JD,  Latitude,  Longitude)*60;
  return seconds;
}


static void ExecuteSceneCommand(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if(New_JSON == NULL)
	{
		Add_Response_msg("Payload is invalid for executing the scene", s_Message_Rx, payLoadData);
		return;
	}

	if(Script_Execute_Que == NULL)
	{
		Script_Execute_Que = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, 200, Script_Execute_pucQueueStorage, &Script_Execute_pxQueueBuffer);
		Script_Execute_Que2 = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, ExecuteScript_QUEUE_ITEMSIZE, Script_Execute_pucQueueStorage2, &Script_Execute_pxQueueBuffer2);

	}
	if (Script_Execute_Que == NULL) {
	#ifdef ENABLE_PRINT_MSG
		printf("Error in creating Script_Execute_Que\n ");
	#endif
	return;
	}
	cJSON *script_IDItem = cJSON_GetObjectItem(New_JSON, "scriptId");  //SCRIPT_ID
	cJSON *script_EventScenetem = cJSON_GetObjectItem(New_JSON, "EVENT_SCENE");  //SCRIPT_ID
	cJSON *RampTimeSceneItem = cJSON_GetObjectItem(New_JSON, "rampTime");  //RampTimeScene
	cJSON *DwellTimeSceneItem = cJSON_GetObjectItem(New_JSON, "Duration");  //DwellTimeScene

   if ((script_IDItem != NULL) && (cJSON_IsNumber(script_IDItem)))
	{
	   uint16_t script_ID = script_IDItem->valueint;
	   uint16_t Event_Scene = 0;
	   uint32_t RampTimeScene_Value = 0;
	   uint32_t DwellTimeScene_Value = 0;

	   if ((RampTimeSceneItem != NULL) && (cJSON_IsNumber(RampTimeSceneItem)))
	   {
		   RampTimeScene_Value = RampTimeSceneItem->valueint;
	   }
	   if ((DwellTimeSceneItem != NULL) && (cJSON_IsNumber(DwellTimeSceneItem)))
	   {
		   DwellTimeScene_Value = DwellTimeSceneItem->valueint;
	   }


#ifdef ENABLE_PRINT_MSG
	   printf("RampTimeScene_Value = %ld, DwellTimeScene_Value = %ld \n", RampTimeScene_Value, DwellTimeScene_Value );
#endif
	   if ((script_EventScenetem != NULL) && (cJSON_IsNumber(script_EventScenetem)))
	   {
		   Event_Scene = script_EventScenetem->valueint;
	   }
		cJSON *my_JSON  	= cJSON_CreateObject();
		cJSON_AddNumberToObject(my_JSON, "ScriptId",script_ID);
		cJSON_AddNumberToObject(my_JSON, "RampTime",RampTimeScene_Value);
		cJSON_AddNumberToObject(my_JSON, "Duration",DwellTimeScene_Value);
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
		{
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		}
		if(ExecuteMethodForEventHandle == NULL)
		{
			ExecuteMethodForEventHandle = xTaskCreateStaticPinnedToCore(
							Tasks_ExecuteMethodForEvent,                 // Task function
							"EVENT_EXECUTE",            // Task name
							EVENT_EXECUTE_TASK_STACK_DEPTH,        // Stack size in words
							s_Message_Rx,                    // Task parameters (not used here)
							EVENT_EXECUTE_TASK_PRIORITY,                       // Task priority
							xSceneTaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xEventExecuteTaskBuffer,             // Pointer to task control block
							1	//0
			);
			if (ExecuteMethodForEventHandle == NULL)
			{
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create task\n");
#endif
			// Handle error
			}
			else
			{
				if(Event_Scene == 0)
				{
					execute_com = 1;
				}
				else
				{
					execute_com = 0;
				}
			}
		}
		else
		{
			if(Event_Scene == 0)
			{
				execute_com = 1;
			}
			else
			{
				execute_com = 0;
			}
		}
		 if(Script_Execute_Que != NULL)
			 xQueueSend(Script_Execute_Que, (char*)s_Message_Rx->payload_p8,  QUE_DELAY);

		cJSON_Delete(my_JSON); // Free the cJSON object
	}
   else
	   Add_Response_msg("Invalid input, \"scriptId\" key is missing.", s_Message_Rx, payLoadData);
   cJSON_Delete(New_JSON); // Free the cJSON object
}

static void ExecuteActionCommand(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if(New_JSON == NULL)
	{
		Add_Response_msg("Payload is invalid for executing the Action", s_Message_Rx, payLoadData);
		return;
	}
	if(Script_Execute_Que == NULL)
	{
		Script_Execute_Que = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, 200, Script_Execute_pucQueueStorage, &Script_Execute_pxQueueBuffer);
		Script_Execute_Que2 = xQueueCreateStatic(ScriptExecute_Ack_QUE_COUNT, ExecuteScript_QUEUE_ITEMSIZE, Script_Execute_pucQueueStorage2, &Script_Execute_pxQueueBuffer2);
	}

	if (Script_Execute_Que == NULL)
	{
		#ifdef ENABLE_PRINT_MSG
			printf("Error in creating Script_Execute_Que\n ");
		#endif

			cJSON_Delete(New_JSON); // Free the cJSON object
		return;
	}


	cJSON *action_IndexItem = cJSON_GetObjectItem(New_JSON, "ActionIndex");  //SCRIPT_ID
   if ((action_IndexItem != NULL) && (cJSON_IsNumber(action_IndexItem)))
	{
	   uint16_t action_Index = action_IndexItem->valueint;
			cJSON *my_JSON  	= cJSON_CreateObject();
			cJSON_AddNumberToObject(my_JSON, "ActionIndex",action_Index);
	        memset(payLoadData,0,sizeof(payLoadData));//\0';
	    	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
	    	cJSON_Delete(my_JSON); // Free the cJSON object
	    	cJSON_Delete(New_JSON); // Free the cJSON object
			{
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			}
			if(ExecuteMethodForEventHandle == NULL)
			{
				ExecuteMethodForEventHandle = xTaskCreateStaticPinnedToCore(
								Tasks_ExecuteMethodForEvent,                 // Task function
								"EVENT_EXECUTE",            // Task name
								EVENT_EXECUTE_TASK_STACK_DEPTH,        // Stack size in words
								s_Message_Rx,                    // Task parameters (not used here)
								EVENT_EXECUTE_TASK_PRIORITY,                       // Task priority
								xSceneTaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xEventExecuteTaskBuffer,             // Pointer to task control block
								1	//0
				);
				if (ExecuteMethodForEventHandle == NULL)
				{
#ifdef ENABLE_PRINT_MSG
					printf("Failed to create task\n");
#endif
					cJSON_Delete(New_JSON);  // Free memory before returning
					return;
				// Handle error
				}
			}
			 if(Script_Execute_Que != NULL)
				 xQueueSend(Script_Execute_Que, (char*)s_Message_Rx->payload_p8,  QUE_DELAY);
	}
   else
   {
	   Add_Response_msg("Invalid input, \"ActionIndex\" key is missing.", s_Message_Rx, payLoadData);
	   cJSON_Delete(New_JSON); // Free the cJSON object
   }
}

static void Set_Event_Task__Flag(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	cJSON *enableItem = cJSON_GetObjectItem(New_JSON, "EVT_TASK_DIS_FLAG");
	if (cJSON_IsNumber(enableItem))
	{
	   uint8_t u8evt_task_dis_fg = enableItem->valueint;
		if(u8evt_task_dis_fg)
		{
			Add_Response_msg("Event Execution Flag Enabled", s_Message_Rx, payLoadData);
			Event_Execution_Flag = Enable;
		}
		else
		{
			Add_Response_msg("Event Execution Flag Disabled", s_Message_Rx, payLoadData);
			Event_Execution_Flag = Disable;
		}
	}
}


static void CalculateSunsetTime(AMessage_st* s_Message_Rx,  cJSON * root)
{
	uint8_t DD;
	uint8_t MM;
	uint8_t YY;
	uint64_t u64epoch_seconds= 0;
	int16_t ui8LstMin=0;
	date_time_t       sdate_tim;
	uint8_t	dst_enable_flag=RESET;
	int i;
	uint8_t sunset_time[10] = {0};
	char str[100]={0};
	YY= UserDateTime_1.year;
	MM=	UserDateTime_1.month;
	DD=	UserDateTime_1.date;
	sunset_function(EVENT_Para.Latitude,EVENT_Para.Longitude,YY,MM,DD,sunset_time);
	u64epoch_seconds = sunset_function_seconds(EVENT_Para.Latitude,EVENT_Para.Longitude*(-1),YY,MM,DD);
	u64epoch_seconds-= EPOSCH_TO_30_YEAR;
	ui8LstMin= EVENT_Para.gmt_value;
	// Apply timezone offset to current time
	u64epoch_seconds = u64epoch_seconds + (ui8LstMin * 60);//Reverse of set_rtc
	if(EVENT_Para.dst_value!=0) //Only then opply the DST
	{
		 i=0;
		 while(i<DST_TABLE_ENTRIES)
		 {
			if((u64epoch_seconds>=Dst_Start_date[i])&&(u64epoch_seconds<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
			{
				dst_enable_flag=SET;
				break;
			}
			i++;
		}
		if(dst_enable_flag == SET)
		{
			u64epoch_seconds = u64epoch_seconds +3600; //advance clock by 1 hour/3600 seconds
		}
		dst_enable_flag = RESET;
	}

	epoch_to_date_time(&sdate_tim,u64epoch_seconds);

	 Str_SS_SR.SS_HR = sdate_tim.hour;
	 Str_SS_SR.SS_MIN = sdate_tim.minute;
	 sprintf(str,"%02d:%02d",sdate_tim.hour,sdate_tim.minute);
	 cJSON_AddStringToObject(root, "SUNSET Time", str);
}

static void CalculateSunriseTime(AMessage_st* s_Message_Rx, cJSON * root)
{
	uint8_t DD;
	uint8_t MM;
	uint8_t YY;
	uint64_t u64epoch_seconds= 0;
	int16_t ui8LstMin=0;
	date_time_t       sdate_tim;
	uint8_t	dst_enable_flag=RESET;
	int i;
	char str[100]={0};
	uint8_t sunrise_time[10] = {0};
	YY= UserDateTime_1.year;
	MM=	UserDateTime_1.month;
	DD=	UserDateTime_1.date;

	sunrise_function(EVENT_Para.Latitude,EVENT_Para.Longitude,YY,MM,DD,sunrise_time);
	u64epoch_seconds = sunrise_function_seconds(EVENT_Para.Latitude,EVENT_Para.Longitude*(-1),YY,MM,DD);
	u64epoch_seconds-= EPOSCH_TO_30_YEAR;
	ui8LstMin= EVENT_Para.gmt_value;
	// Apply timezone offset to current time
	u64epoch_seconds = u64epoch_seconds + (ui8LstMin * 60);//Reverse of set_rtc
	if(EVENT_Para.dst_value!=0) //Only then opply the DST
	{
		 i=0;
		 while(i<DST_TABLE_ENTRIES)
		 {
			if((u64epoch_seconds>=Dst_Start_date[i])&&(u64epoch_seconds<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
			{
					dst_enable_flag=SET;
					break;
			}
			i++;
		}
		if(dst_enable_flag == SET)
		{
			u64epoch_seconds = u64epoch_seconds +3600; //advance clock by 1 hour/3600 seconds
		}
		dst_enable_flag = RESET;
	}

	epoch_to_date_time(&sdate_tim,u64epoch_seconds);
	Str_SS_SR.SR_HR = sdate_tim.hour;
	Str_SS_SR.SR_MIN = sdate_tim.minute;
	sprintf(str,"%02d:%02d",sdate_tim.hour,sdate_tim.minute);
	cJSON_AddStringToObject(root, "SUNRISE Time", str);
}

static void get_RTC_time(AMessage_st* s_Message_Rx)
{
	date_time_t       sdate_tim;
	uint64_t current_epos_sec;//mills;
	char str[100] ={0};
	date_time_t  sdate_timUTC;
	int i=0;
	int16_t ui8LstMin=0;
	uint8_t	dst_enable_flag=RESET;

	{
		struct timeval currentTime;
		_gettimeofday_r(NULL, &currentTime, NULL);
		current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
#ifdef ENABLE_PRINT_MSG
		printf("Current_epos_sec: %lld\n", current_epos_sec);
#endif
	    cJSON *root_JSON = cJSON_CreateObject();
	    cJSON_AddNumberToObject(root_JSON, "Current_Epoch_Sec", current_epos_sec);

		current_epos_sec =(current_epos_sec/1000)-EPOSCH_TO_30_YEAR;  //Convert epoch msec to seconds and subtract offset of 30 years
		epoch_to_date_time(&sdate_timUTC,current_epos_sec);
#ifdef ENABLE_PRINT_MSG
		printf("\n\nUTC TIME: %02d/%02d/%02d %02d:%02d:%02d\n\n", sdate_timUTC.year, sdate_timUTC.month, sdate_timUTC.date, sdate_timUTC.hour,sdate_timUTC.minute,sdate_timUTC.second); // Years since 1900
#endif


		ui8LstMin = EVENT_Para.gmt_value;
		current_epos_sec = current_epos_sec + (ui8LstMin * 60);//Reverse of set_rtc
		// Apply timezone offset to current time

		if(EVENT_Para.dst_value!=0) //Only then opply the DST
	    {
			 i=0;
			 while(i<DST_TABLE_ENTRIES)
			 {

				if((current_epos_sec>=Dst_Start_date[i])&&(current_epos_sec<Dst_End_date[i])/*&&(dst_over_fg==0)*/)
				{
						dst_enable_flag=SET;
						break;
				}
				i++;
			}
			if(dst_enable_flag == SET)
			{
					current_epos_sec = current_epos_sec +3600; //advance clock by 1 hour/3600 seconds
			}

			dst_enable_flag = RESET;

	    }

		// Convert to local time
		epoch_to_date_time(&sdate_tim,current_epos_sec);
		UserDateTime_1.hour = sdate_tim.hour;
		UserDateTime_1.minute = sdate_tim.minute;
		UserDateTime_1.second = sdate_tim.second;
		UserDateTime_1.year = sdate_tim.year;
		UserDateTime_1.month = sdate_tim.month;
		UserDateTime_1.date = sdate_tim.date;


#ifdef ENABLE_PRINT_MSG
		printf("Local TIME: %02d/%02d/%02d %02d:%02d:%02d\n",UserDateTime_1.date, UserDateTime_1.month,UserDateTime_1.year,UserDateTime_1.hour,UserDateTime_1.minute,UserDateTime_1.second); // Years since 1900
#endif


		sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d:%03d", sdate_timUTC.year, sdate_timUTC.month, sdate_timUTC.date, sdate_timUTC.hour,sdate_timUTC.minute, sdate_timUTC.second, sdate_timUTC.milSec); // Years since 1900
		cJSON_AddStringToObject(root_JSON, "UTC TIME", str);
		sprintf(str,"%02d/%02d/%02d %02d:%02d",UserDateTime_1.year, UserDateTime_1.month,UserDateTime_1.date,UserDateTime_1.hour,UserDateTime_1.minute); // Years since 1900
		cJSON_AddStringToObject(root_JSON, "Local TIME", str);
		CalculateSunriseTime(NULL, root_JSON);
		CalculateSunsetTime(NULL, root_JSON);
        memset(payLoadData,0,sizeof(payLoadData));//\0';
    	cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
		cJSON_Delete(root_JSON);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
}

static void ExecuteEventIDCommand(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	int schedule_record=0;
	char Event_Idfound = 0;
	if(New_JSON == NULL)
	{
		Add_Response_msg("Payload is invalid for executing the scene", s_Message_Rx, payLoadData);
		return;
	}

	cJSON *event_IDItem = cJSON_GetObjectItem(New_JSON, "eventId");  //SCRIPT_ID

	   if ((event_IDItem != NULL) && (cJSON_IsNumber(event_IDItem)))
		{
		   uint32_t event_ID = event_IDItem->valueint;
		   for(schedule_record = 0; (Schedule_table[schedule_record].EventId!=0); schedule_record++)
		   {
			   if (Schedule_table[schedule_record].EventId == event_ID)
			   {
				   Event_Idfound = 1;
				   cJSON *my_JSON  	= cJSON_CreateObject();
				   cJSON_AddNumberToObject(my_JSON, "ScriptID",Schedule_table[schedule_record].ScriptID);
				   cJSON_AddNumberToObject(my_JSON, "EVENT_SCENE",1);
				   cJSON_AddNumberToObject(my_JSON, "Override_Sport",Schedule_table[schedule_record].OverrideSport);
				   cJSON_AddNumberToObject(my_JSON, "RampTime",Schedule_table[schedule_record].RampTime);
				   cJSON_AddNumberToObject(my_JSON, "Duration",Schedule_table[schedule_record].Duration);
			       memset(payLoadData,0,sizeof(payLoadData));//\0';
			       cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
				   strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				   ExecuteSceneCommand(s_Message_Rx);
				   cJSON_Delete(my_JSON);
				   break;
			   }
		   }
		}
   cJSON_Delete(New_JSON); // Free the cJSON object
   if(Event_Idfound == 0)
   {
	   Add_Response_msg("Event ID is not found", s_Message_Rx, payLoadData);
   }
}


static void TimeWarpCommand(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if(New_JSON == NULL)
	{
		Add_Response_msg("Payload is invalid for executing the TimeWarpCommand", s_Message_Rx, payLoadData);
		return;
	}
	cJSON *startEPOCItem = cJSON_GetObjectItem(New_JSON, "startEPOC");  //SCRIPT_ID
	if ((startEPOCItem != NULL) && (cJSON_IsNumber(startEPOCItem)))
	{
	   startEPOC1 = startEPOCItem->valuedouble;
	   startEPOC1=startEPOC1 - startEPOC1%60000;
	}
	cJSON *endEPOCItem = cJSON_GetObjectItem(New_JSON, "endEPOC");  //SCRIPT_ID
    if ((endEPOCItem != NULL) && (cJSON_IsNumber(endEPOCItem)))
	{
	   endEPOC1 = endEPOCItem->valuedouble;
	   endEPOC1=endEPOC1 - endEPOC1%60000;
	}
   cJSON_Delete(New_JSON); // Free the cJSON object
}




/**************************************************************************************
*
*	BITS Pilani - Copyright (c) 2026
*	All rights reserved.
*
*	Project 		: Smart Home Energy Monitor and control System - Semester 3 - IOT
*	Author			: Ganesh
*
*	Revision History
***************************************************************************************
*	Date			Version		Name		Description
***************************************************************************************
*	23/03/2026		1.0			Ganesh		Initial Development
*
**************************************************************************************/

/*** Includes ***/
#include "general.h"
#include "ui_lvgl.h"

#define	CUR_SENS_SIMULATOR	curSs
#define MODBUS_DEBUG		modDebug
#define DEBUG_LOG			debug

/*** Globals ***/
UINT64	flag1;
MP_INST	mpInst;
UINT16	curSs;
BOOL	debug,modDebug;

#define IDX_HD1     0
#define IDX_LMC1    1
#define IDX_FMC1    2
#define IDX_HD2     3
#define IDX_LMC2    4
#define IDX_AC2     5
#define IDX_RM2     6

/****************************************************************
* Private Function
****************************************************************/
/*************************************************************************
* @brief        Generates a timestamp from the system RTC.
*
* @details      This function generates a timestamp in the format "YYYY-MM-DD HH:MM:SS"
*               from the system RTC.
*
* @param[out]   buffer      Pointer to the buffer where the timestamp will be stored.
* @param[in]    bufferSize  The size of the buffer.
*
* @return       void
*************************************************************************/
void generateTimestamp(char *buffer, size_t bufferSize)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", t);
}

/*************************************************************************
* @brief        Gets the current time in milliseconds.
*
* @details      This function gets the current time in milliseconds using the CLOCK_MONOTONIC clock.
*
* @return       UINT64  The current time in milliseconds.
*************************************************************************/
static UINT64 getTimeMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((UINT64)ts.tv_sec * 1000ULL) + ((UINT64)ts.tv_nsec / 1000000ULL);
}
/*************************************************************************
* @brief        Reads configuration from a file.
*
* @details      This function reads and parses the configuration file provided
*               to the main process. It extracts the IP address of the sensor simulator,
*               Modbus TCP port, periodic interval to read the data, MQTT broker IP or URL,
*               MQTT port, MQTT username, MQTT password, and MQTT publish periodic interval.
*
* @param[in]    filename    The name of the configuration file.
* @param[out]   args        Pointer to the structure where the arguments will be stored.
*
* @return       ERROR_CODE  Returns RET_OK if the configuration is successfully read and valid,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static int iniHandler(void* user, const char* section, const char* name, const char* value)
{
    PROGRAM_ARGS *args = (PROGRAM_ARGS*)user;
	const CHAR *ssSection[MAX_SENS_SIMULATOR] = {"HD_1","LMC_1","FMC_1","HD_2","LMC_2","AMC_2","RM_2"};
	UINT16 ssIdx = 0;

	for(ssIdx=0;ssIdx < CUR_SENS_SIMULATOR;ssIdx++)
	{
		if (strcmp(section, ssSection[ssIdx]) == 0)
		{
			if (strcmp(name, "sensorIP") == 0)
				args->sensorIP[ssIdx] = strdup(value);
			else if (strcmp(name, "sensorPort") == 0)
				args->sensorPort[ssIdx] = (UINT16)atoi(value);
			else if (strcmp(name, "readInterval") == 0)
				args->readInterval[ssIdx] = (UINT16)atoi(value);
            else if (strcmp(name, "pirPollMs") == 0)
                args->pirPollMs[ssIdx] = (UINT16)atoi(value);
		}
	}

	if (strcmp(section, "mqtt") == 0)
	{
        if (strcmp(name, "mqttIP") == 0)
            args->mqttIP = strdup(value);
        else if (strcmp(name, "mqttPort") == 0)
            args->mqttPort = (UINT16)atoi(value);
        else if (strcmp(name, "mqttUsername") == 0)
            args->mqttUsername = strdup(value);
        else if (strcmp(name, "mqttPassword") == 0)
            args->mqttPassword = strdup(value);
        else if (strcmp(name, "publishInterval") == 0)
            args->publishInterval = (UINT16)atoi(value);
        else if (strcmp(name, "statePublishInterval") == 0)
            args->statePublishInterval = (UINT16)atoi(value);
    }

    if (strcmp(section, "ui") == 0)
    {
        if (strcmp(name, "fbdev") == 0)
            args->uiFbdev = strdup(value);
        else if (strcmp(name, "touchDev") == 0)
            args->uiTouchDev = strdup(value);
    }

    return RET_SUCCESS;
}

static ERROR_CODE readConfig(const CHAR *filename, PROGRAM_ARGS *args)
{
	UINT16 ssIdx = 0;
    if(ini_parse(filename, iniHandler, args) < 0)
	{
        fprintf(stderr, "Failed to load config file: %s\n", filename);
        return RET_FAILURE;
    }

	if(DEBUG_LOG)
	{
		fprintf(stdout,"Reading configuration..\n");
		fprintf(stdout,"Number of sensor simulator : %d out of %d\n",CUR_SENS_SIMULATOR,MAX_SENS_SIMULATOR);
	}

	for(ssIdx=0;ssIdx < CUR_SENS_SIMULATOR;ssIdx++)
	{
		if(!args->sensorIP[ssIdx] || !args->sensorPort[ssIdx] || !args->readInterval[ssIdx] )
		{
			fprintf(stderr, "SS: Invalid configuration values\n");
			return RET_FAILURE;
		}
		else
		{
			if(DEBUG_LOG)
				fprintf(stdout,"Sensor ID : %d\n\tSensor simulator IP : %s\n\tPort: %d\n\tInterval : %d\n",
							ssIdx,args->sensorIP[ssIdx],args->sensorPort[ssIdx],args->readInterval[ssIdx]);
		}
	}

	if( !args->mqttIP || !args->publishInterval)
	{
		fprintf(stderr, "MQTT: Invalid configuration values\n");
		return RET_FAILURE;
	}
	else
	{
		if(DEBUG_LOG)
			fprintf(stdout,"\nMQTT Broker IP/URL : %s\nPort: %d\nInterval : %d\n",
								args->mqttIP,args->mqttPort,args->publishInterval);
	}

    if(args->publishInterval < MIN_MQTT_PUB_INTERVAL || args->publishInterval > MAX_MQTT_PUB_INTERVAL)
    {
        fprintf(stderr, "Error: MQTT publish interval must be between %d and %d seconds.\n",MIN_MQTT_PUB_INTERVAL,MAX_MQTT_PUB_INTERVAL);
        return RET_FAILURE;
    }

    if (args->statePublishInterval == 0)
        args->statePublishInterval = 1;

    if (args->pirPollMs[IDX_HD1] == 0)
        args->pirPollMs[IDX_HD1] = 500;
    if (args->pirPollMs[IDX_HD2] == 0)
        args->pirPollMs[IDX_HD2] = 500;

    if((args->pirPollMs[IDX_HD1] < MIN_PIR_POLL_MS || args->pirPollMs[IDX_HD1] > MAX_PIR_POLL_MS) ||
       (args->pirPollMs[IDX_HD2] < MIN_PIR_POLL_MS || args->pirPollMs[IDX_HD2] > MAX_PIR_POLL_MS))
    {
        fprintf(stderr, "Error: PIR poll interval must be between %d and %d ms.\n", MIN_PIR_POLL_MS, MAX_PIR_POLL_MS);
        return RET_FAILURE;
    }

    if (args->uiFbdev == NULL)
        args->uiFbdev = strdup("/dev/fb0");
    if (args->uiTouchDev == NULL)
        args->uiTouchDev = strdup("/dev/input/event0");

    return RET_OK;
}

/*************************************************************************
* @brief        Connects to the Modbus TCP server.
*
* @details      This function connects to the Modbus TCP server using the provided
*               IP address and port.
*
* @param[out]   ctx         Pointer to the Modbus context.
* @param[in]    ip          The IP address of the Modbus TCP server.
* @param[in]    port        The port of the Modbus TCP server.
*
* @return       ERROR_CODE  Returns RET_OK if the connection is successful,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE connectModbus(modbus_t **ctx, const CHAR *ip, UINT16 port)
{
	if(DEBUG_LOG)
		fprintf(stdout, "Modbus Connecting to %s:%d\n",ip,port);

    *ctx = modbus_new_tcp(ip, port);
    if(*ctx == NULL)
    {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return RET_FAILURE;
    }

    if(modbus_connect(*ctx) == RET_FAILURE)
    {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(*ctx);
		*ctx = NULL;
        return RET_FAILURE;
    }
	else
	{
		if(DEBUG_LOG)
			fprintf(stdout, "Modbus Connected to %s:%d\n",ip,port);
	}

    return RET_OK;
}

/*************************************************************************
* @brief        Checks if the index is a PIR index.
*
* @details      This function checks if the index is a PIR index.
*
* @param[in]    idx         The index to check.
*
* @return       BOOL        Returns TRUE if the index is a PIR index, otherwise returns FALSE.
*************************************************************************/
static BOOL isPIRIndex(UINT16 idx)
{
    return (idx == IDX_HD1 || idx == IDX_HD2) ? TRUE : FALSE;
}

/*************************************************************************
* @brief        Checks if the index is a control index.
*
* @details      This function checks if the index is a control index.
*
* @param[in]    idx         The index to check.
*
* @return       BOOL        Returns TRUE if the index is a control index, otherwise returns FALSE.
*************************************************************************/
static BOOL isControlIndex(UINT16 idx)
{
    return (idx == IDX_LMC1 || idx == IDX_FMC1 || idx == IDX_LMC2 || idx == IDX_AC2) ? TRUE : FALSE;
}

/*************************************************************************
* @brief        Reads holding register from Modbus TCP server.
*
* @details      This function reads power consumption value from holding register 3000.
*
* @param[in]    ctx         The Modbus context.
* @param[out]   power       Pointer to the variable where the power consumption data will be stored.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully read,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE readModbusPower(modbus_t *ctx, UINT16 *power)
{
    UINT16 regVal = 0;
	if(modbus_read_registers(ctx, MODBUS_HOLDING_ADDR_POWER, 1, &regVal) != 1)
    {
        fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
        return RET_FAILURE;
    }

	*power = regVal;
	if(DEBUG_LOG)
		fprintf(stdout, "Received modbus data %d\n",*power);

    return RET_OK;
}

/*************************************************************************
* @brief        Reads coil from Modbus TCP server.
*
* @details      This function reads coil from Modbus TCP server.
*
* @param[in]    ctx         The Modbus context.
* @param[in]    address     The address of the coil.
* @param[out]   state       Pointer to the variable where the coil state will be stored.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully read,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE readModbusCoil(modbus_t *ctx, UINT16 address, UINT8 *state)
{
    UINT8 coil = 0;
    if (modbus_read_bits(ctx, address, 1, &coil) != 1)
    {
        fprintf(stderr, "Failed to read coil(%u): %s\n", address, modbus_strerror(errno));
        return RET_FAILURE;
    }
    *state = coil;
    return RET_OK;
}

/*************************************************************************
* @brief        Writes coil to Modbus TCP server.
*
* @details      This function writes coil to Modbus TCP server.
*
* @param[in]    ctx         The Modbus context.
* @param[in]    address     The address of the coil.
* @param[in]    state       The state of the coil.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully written,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE writeModbusCoil(modbus_t *ctx, UINT16 address, UINT8 state)
{
    if (modbus_write_bit(ctx, address, state) == RET_FAILURE)
    {
        fprintf(stderr, "Failed to write coil(%u): %s\n", address, modbus_strerror(errno));
        return RET_FAILURE;
    }
    return RET_OK;
}

/*************************************************************************
* @brief        Inserts data into the SQLite database.
*
* @details      This function inserts the power consumption data into the SQLite database.
*
* @param[in]    db          The SQLite database connection.
* @param[in]    sensorID    The ID of the sensor.
* @param[in]    power       The power consumption data.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully inserted,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE insertDB(sqlite3 *db, UINT16 sensorID, UINT16 power)
{
    INT32 rc=0;
    CHAR sql[SIZE_256] = {0};
    snprintf(sql, sizeof(sql), "INSERT INTO SensorData (Device_ID, Power_Consumption) VALUES (%d, %d);", sensorID, power);

    if(sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK)
    {
        fprintf(stderr, "INSERT SQL error: %s\n", sqlite3_errmsg(db));
        /* Publish it directly to Server */
        generateTimestamp(mpInst.timestamp, sizeof(mpInst.timestamp));
        memset(sql,0,sizeof(sql));
        snprintf(sql, sizeof(sql), "[{\"sensorID\": %d, \"power\": %d, \"Timestamp\": \"%s\"}]", sensorID, power, mpInst.timestamp);
        if((rc = mosquitto_publish(mpInst.mosq, NULL, MQTT_TOPIC, strlen(sql), sql, 0, false)) != MOSQ_ERR_SUCCESS)
            fprintf(stderr, "Failed to publish message: %s\n", mosquitto_strerror(rc));
    }
	else
	{
		if(DEBUG_LOG)
			fprintf(stdout, "Modbus data of sensor ID %d inserted to DB : %d\n",sensorID,power);
	}

    /* Delete old data beyond 24 hours */
	memset(sql,0,sizeof(sql));
    snprintf(sql, sizeof(sql), "DELETE FROM SensorData WHERE Timestamp < datetime('now', '-1 day');");
    if(sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK)
        fprintf(stderr, "DELETE SQL error: %s\n", sqlite3_errmsg(db));

    return RET_OK;
}

/*************************************************************************
* @brief        Publishes data to the MQTT broker.
*
* @details      This function publishes the power consumption data to the MQTT broker
*               in JSON format.
*
* @param[in]    mosq        The Mosquitto instance.
* @param[in]    db          The SQLite database connection.
* @param[in]    publishInterval The interval in seconds to publish the data.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully published,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE publishMQTT(struct mosquitto *mosq, sqlite3 *db, UINT16 publishInterval)
{
    sqlite3_stmt *stmt=NULL;
    CHAR temp[SIZE_256]={0};
    INT32 rc=0;

    snprintf(temp, sizeof(temp), "SELECT Device_ID, Power_Consumption, Timestamp FROM SensorData WHERE Timestamp >= datetime('now', '-%d seconds');", publishInterval);

    rc = sqlite3_prepare_v2(db, temp, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return RET_FAILURE;
    }

	memset(mpInst.payload,0,sizeof(mpInst.payload));
    strcpy(mpInst.payload, "[");
    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
		memset(temp,0,sizeof(temp));
        snprintf(temp, sizeof(temp), "{\"sensorID\": %d, \"power\": %d, \"Timestamp\": \"%s\"},",
                 sqlite3_column_int(stmt, 0),
                 sqlite3_column_int(stmt, 1),
                 sqlite3_column_text(stmt, 2));
        strcat(mpInst.payload, temp);
    }
    sqlite3_finalize(stmt);

    /* Remove the trailing comma and close the JSON array */
    if(mpInst.payload[strlen(mpInst.payload) - 1] == ',')
        mpInst.payload[strlen(mpInst.payload) - 1] = '\0';

    strcat(mpInst.payload, "]");

    if(strlen(mpInst.payload) > MQTT_PAYLOAD_MIN_SIZE) // to check if there is any data to publish
    {
        if((rc = mosquitto_publish(mpInst.mosq, NULL, MQTT_TOPIC, strlen(mpInst.payload), mpInst.payload, 0, false)) != MOSQ_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to publish message: %s\n",mosquitto_strerror(rc));
            return RET_FAILURE;
        }
    }
    return RET_OK;
}

/*************************************************************************
* @brief        Publishes state topic to the MQTT broker.
*
* @details      This function publishes the state topic to the MQTT broker.
*
* @param[in]    topic       The topic to publish.
* @param[in]    payload     The payload to publish.
*
* @return       void
*************************************************************************/
static void publishStateTopic(const CHAR *topic, const CHAR *payload)
{
    INT32 rc = mosquitto_publish(mpInst.mosq, NULL, topic, (INT32)strlen(payload), payload, 0, false);
    if (rc != MOSQ_ERR_SUCCESS && DEBUG_LOG)
        fprintf(stderr, "State publish failed for %s: %s\n", topic, mosquitto_strerror(rc));
}

/*************************************************************************
* @brief        Callback for successful connection to the MQTT broker.
*
* @details      This function is called when the MQTT broker is successfully connected.
*
* @param[in]    mosq        The Mosquitto instance.
* @param[in]    obj         The user data.
* @param[in]    rc          The return code.
*************************************************************************/
static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    if(rc == 0)
	{
		SET_FLAG(MQTT_CONNECTED);
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_CMD_MODE, 0);
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_CMD_R1_LIGHT, 0);
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_CMD_R1_FAN, 0);
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_CMD_R2_LIGHT, 0);
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_CMD_R2_AC, 0);
		if(DEBUG_LOG)
			fprintf(stdout,"Connected to MQTT broker successfully.\n");
	}
    else
        fprintf(stderr, "Failed to connect to MQTT broker, return code: %d\n", rc);
}

/*************************************************************************
* @brief        Callback for successful message publication.
*
* @details      This function is called when a message is successfully published.
*
* @param[in]    mosq        The Mosquitto instance.
* @param[in]    obj         The user data.
* @param[in]    mid         The message ID.
*************************************************************************/
static void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
	if(DEBUG_LOG)
		fprintf(stdout,"Message published successfully, message ID: %d\n", mid);
}

/*************************************************************************
* @brief        Callback for logging.
*
* @details      This function is called when a log message is received.
*
* @param[in]    mosq        The Mosquitto instance.
* @param[in]    obj         The user data.
* @param[in]    level       The level of the log message.
* @param[in]    str         The log message.
*************************************************************************/
static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	if(DEBUG_LOG)
		fprintf(stdout,"MQTT Log: %s\n", str);
}

/*************************************************************************
* @brief        Parses the switch payload.
*
* @details      This function parses the switch payload.
*
* @param[in]    payload     The payload to parse.
*
* @return       UINT8       Returns 1 if the payload is "1", "on", or "true", otherwise returns 0.
*************************************************************************/
static UINT8 parseSwitchPayload(const CHAR *payload)
{
    if ((strcmp(payload, "1") == 0) || (strcasecmp(payload, "on") == 0) || (strcasecmp(payload, "true") == 0))
        return 1;
    return 0;
}

/*************************************************************************
* @brief        Callback for message reception.
*
* @details      This function is called when a message is received.
*
* @param[in]    mosq        The Mosquitto instance.
* @param[in]    obj         The user data.
* @param[in]    msg         The message.
*************************************************************************/
static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    CHAR payload[SIZE_64] = {0};
    size_t copyLen = 0;

    if ((msg == NULL) || (msg->payload == NULL) || (msg->topic == NULL))
        return;

    copyLen = (msg->payloadlen >= (INT32)sizeof(payload)) ? (sizeof(payload) - 1U) : (size_t)msg->payloadlen;
    memcpy(payload, msg->payload, copyLen);
    payload[copyLen] = '\0';

    if (strcmp(msg->topic, MQTT_TOPIC_CMD_MODE) == 0)
    {
        mpInst.isAutoMode = (strcasecmp(payload, MQTT_MODE_AUTO) == 0) ? TRUE : FALSE;
        return;
    }
    if (strcmp(msg->topic, MQTT_TOPIC_CMD_R1_LIGHT) == 0)
        mpInst.manualState[IDX_LMC1] = parseSwitchPayload(payload);
    else if (strcmp(msg->topic, MQTT_TOPIC_CMD_R1_FAN) == 0)
        mpInst.manualState[IDX_FMC1] = parseSwitchPayload(payload);
    else if (strcmp(msg->topic, MQTT_TOPIC_CMD_R2_LIGHT) == 0)
        mpInst.manualState[IDX_LMC2] = parseSwitchPayload(payload);
    else if (strcmp(msg->topic, MQTT_TOPIC_CMD_R2_AC) == 0)
        mpInst.manualState[IDX_AC2] = parseSwitchPayload(payload);
}

/*************************************************************************
* @brief        Applies the control policy.
*
* @details      This function applies the control policy based on the mode and PIR sensor values.
*
* @return       void
*************************************************************************/
static VOID applyControlPolicy(VOID)
{
    if (mpInst.isAutoMode)
    {
        mpInst.outputState[IDX_LMC1] = mpInst.pir[IDX_HD1];
        mpInst.outputState[IDX_FMC1] = mpInst.pir[IDX_HD1];
        mpInst.outputState[IDX_LMC2] = mpInst.pir[IDX_HD2];
        mpInst.outputState[IDX_AC2] = mpInst.pir[IDX_HD2];
    }
    else
    {
        mpInst.outputState[IDX_LMC1] = mpInst.manualState[IDX_LMC1];
        mpInst.outputState[IDX_FMC1] = mpInst.manualState[IDX_FMC1];
        mpInst.outputState[IDX_LMC2] = mpInst.manualState[IDX_LMC2];
        mpInst.outputState[IDX_AC2] = mpInst.manualState[IDX_AC2];
    }
}

/*************************************************************************
* @brief        Applies the UI commands.
*
* @details      This function applies the UI commands.
*
* @return       void
*************************************************************************/
static VOID applyUICommands(VOID)
{
    UI_COMMANDS cmd;

    if (uiFetchCommands(&cmd) != RET_OK)
        return;

    if (cmd.hasMode)
        mpInst.isAutoMode = cmd.isAutoMode ? TRUE : FALSE;

    if (cmd.hasRoom1Light)
        mpInst.manualState[IDX_LMC1] = cmd.room1Light ? 1U : 0U;
    if (cmd.hasRoom1Fan)
        mpInst.manualState[IDX_FMC1] = cmd.room1Fan ? 1U : 0U;
    if (cmd.hasRoom2Light)
        mpInst.manualState[IDX_LMC2] = cmd.room2Light ? 1U : 0U;
    if (cmd.hasRoom2AC)
        mpInst.manualState[IDX_AC2] = cmd.room2AC ? 1U : 0U;
}

/*************************************************************************
* @brief        Prints the usage information.
*
* @details      This function prints the usage information.
*
* @return       void
*************************************************************************/
static void printUsage(void)
{
    fprintf(stdout,"Usage: ems_mainProc [OPTIONS]\n");
    fprintf(stdout,"Options:\n");
    fprintf(stdout,"  -n <max sensor>       Max number of sensor simulator(Upto 7)\n");
    fprintf(stdout,"  -d                    Enable debug\n");
    fprintf(stdout,"  -h, --help            Show this help message and exit\n");
}

/*************************************************************************
* @brief        Main function.
*
* @details      This function is the main function of the program.
*
* @param[in]    argc        The number of arguments.
* @param[in]    argv        The arguments.
* @param[in]    envp        The environment variables.
*
* @return       INT32       Returns 0 if the program exits successfully, otherwise returns -1.
*************************************************************************/
INT32 main(INT32 argc, CHAR **argv, CHAR **envp)
{
    INT32	rc = 0;
	UINT16	idx = 0;
    time_t nowTs = 0;
    UINT64 nowMs = 0;

	while((rc = getopt(argc, argv, "n:h:d")) != RET_FAILURE)
    {
        switch (rc)
        {
            case 'n':
                curSs = (UINT16)atoi(optarg);
            break;
            case 'd':
				modDebug = debug = TRUE;
            break;
            case 'h':
                printUsage();
                exit(RET_OK);
			break;
            default:
				printUsage();
                return RET_FAILURE;
			break;
        }
    }

	if(curSs > MAX_SENS_SIMULATOR || curSs <= 0)
	{
		fprintf(stderr, "Invalid inputs\n");
		printUsage();
        return RET_FAILURE;
	}

	if(DEBUG_LOG)
		fprintf(stdout,"\n<< EMS - Main Process v%s >>\n\n",APP_VERSION);

    while(mpInst.state != STATE_ERROR)
    {
        switch(mpInst.state)
        {
            case STATE_INIT:
			{
				if(readConfig(CONFIG_FILE, &mpInst.args) != RET_OK)
				{
					mpInst.state = STATE_ERROR;
					break;
				}

                if (uiInit(mpInst.args.uiFbdev, mpInst.args.uiTouchDev) != RET_OK)
                {
                    mpInst.state = STATE_ERROR;
                    break;
                }

				/* Initialize SQLite database */
				rc = sqlite3_open(DB_NAME, &mpInst.db);
				if(rc != SQLITE_OK)
				{
					fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(mpInst.db));
					mpInst.state = STATE_ERROR;
					break;
				}

				/* Create table if not exists */
				const CHAR *sql = "CREATE TABLE IF NOT EXISTS SensorData ("
								  "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
								  "Device_ID INTEGER, "
								  "Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
								  "Power_Consumption INTEGER);";
				rc = sqlite3_exec(mpInst.db, sql, 0, 0, 0);
				if(rc != SQLITE_OK)
				{
					fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(mpInst.db));
					sqlite3_close(mpInst.db);
					mpInst.db = NULL;
					mpInst.state = STATE_ERROR;
					break;
				}

				/* Initialize MQTT */
				CLR_FLAG(MQTT_CONNECTED);
				mosquitto_lib_init();
				mpInst.mosq = mosquitto_new(MQTT_CLIENT_ID, true, &mpInst);
				if(!mpInst.mosq)
				{
					fprintf(stderr, "Failed to create mosquitto instance\n");
					sqlite3_close(mpInst.db);
					mpInst.db = NULL;
					mosquitto_lib_cleanup();
					mpInst.state = STATE_ERROR;
					break;
				}

				if(mpInst.args.mqttUsername && mpInst.args.mqttPassword)
					mosquitto_username_pw_set(mpInst.mosq, mpInst.args.mqttUsername, mpInst.args.mqttPassword);

				/* Set callbacks */
				mosquitto_connect_callback_set(mpInst.mosq, on_connect);
				mosquitto_publish_callback_set(mpInst.mosq, on_publish);
				mosquitto_log_callback_set(mpInst.mosq, on_log);
                mosquitto_message_callback_set(mpInst.mosq, on_message);

				/* Connect to MQTT broker */
				rc = mosquitto_connect(mpInst.mosq, mpInst.args.mqttIP, mpInst.args.mqttPort, 60);
				if(rc != MOSQ_ERR_SUCCESS)
				{
					fprintf(stderr, "Failed to connect to MQTT broker: %s\n", mosquitto_strerror(rc));
					sqlite3_close(mpInst.db);
					mpInst.db = NULL;
					mosquitto_destroy(mpInst.mosq);
					mpInst.mosq = NULL;
					mosquitto_lib_cleanup();
					mpInst.state = STATE_ERROR;
					break;
				}
				else
				{
					/* Create Mqtt Network Handle Thread */
					rc = mosquitto_loop_start(mpInst.mosq);
					if( rc != MOSQ_ERR_SUCCESS )
					{
						if(DEBUG_LOG)
							fprintf(stderr,"Mqtt Loop thread start error..\n");

						mosquitto_disconnect(mpInst.mosq);
						mosquitto_destroy(mpInst.mosq);
						mpInst.mosq = NULL;
						sqlite3_close(mpInst.db);
						mpInst.db = NULL;
						mosquitto_lib_cleanup();
						mpInst.state = STATE_ERROR;
						break;
					}
				}
                mpInst.state = STATE_CONNECT_MQTT;
			}
            break;
			case STATE_CONNECT_MQTT:
			{
				sleep(1);
				mpInst.state = (CHECK_FLAG(MQTT_CONNECTED)) ? STATE_CONNECT_MODBUS : STATE_CONNECT_MQTT;
			}
			break;
            case STATE_CONNECT_MODBUS:
			{
                UI_STATE uiState;
                memset(&uiState, 0, sizeof(uiState));

                uiProcess();
                applyUICommands();

                for(idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
                {
                    if(!mpInst.mConnected[idx] && connectModbus(&mpInst.ctx[idx], mpInst.args.sensorIP[idx], mpInst.args.sensorPort[idx]) != RET_OK)
                    {
                        mpInst.mConnected[idx] = FALSE;
                        continue;
                    }
                    else
                        mpInst.mConnected[idx] = TRUE;

					if(MODBUS_DEBUG) // Enable debug mode for the Modbus context
						modbus_set_debug(mpInst.ctx[idx], TRUE);

                    if(mpInst.mConnected[idx])
                    {
                        if (isPIRIndex(idx))
                        {
                            nowMs = getTimeMs();
                            if ((mpInst.lastPirPollTs[idx] == 0U) || ((nowMs - mpInst.lastPirPollTs[idx]) >= mpInst.args.pirPollMs[idx]))
                            {
                                if(readModbusCoil(mpInst.ctx[idx], MODBUS_COIL_ADDR_READ, &mpInst.pir[idx]) != RET_OK)
                                    mpInst.mConnected[idx] = FALSE;
                                else
                                    mpInst.lastPirPollTs[idx] = nowMs;
                            }
                        }
                        else
                        {
                            if(readModbusPower(mpInst.ctx[idx], &mpInst.power[idx]) != RET_OK)
                                mpInst.mConnected[idx] = FALSE;
                        }
                    }
                }

                applyControlPolicy();
                for (idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
                {
                    if (mpInst.mConnected[idx] && isControlIndex(idx))
                    {
                        if (writeModbusCoil(mpInst.ctx[idx], MODBUS_COIL_ADDR_WRITE, mpInst.outputState[idx]) != RET_OK)
                            mpInst.mConnected[idx] = FALSE;
                    }
                }

                uiState.isAutoMode = mpInst.isAutoMode;
                uiState.pirRoom1 = mpInst.pir[IDX_HD1];
                uiState.pirRoom2 = mpInst.pir[IDX_HD2];
                uiState.room1Light = mpInst.outputState[IDX_LMC1];
                uiState.room1Fan = mpInst.outputState[IDX_FMC1];
                uiState.room2Light = mpInst.outputState[IDX_LMC2];
                uiState.room2AC = mpInst.outputState[IDX_AC2];
                uiUpdateState(&uiState);

                mpInst.state = STATE_INSERT_DB;
			}
            break;
            case STATE_INSERT_DB:
			{
                for(idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
                {
                    if(mpInst.mConnected[idx] && !isPIRIndex(idx))
                        insertDB(mpInst.db, (idx + 1), mpInst.power[idx]);
                }
                mpInst.state = STATE_PUBLISH_MQTT;
			}
            break;
            case STATE_PUBLISH_MQTT:
			{
                nowTs = time(NULL);
                if ((mpInst.lastDataPubTs == 0U) || ((UINT64)nowTs - mpInst.lastDataPubTs >= mpInst.args.publishInterval))
                {
                    if(publishMQTT(mpInst.mosq, mpInst.db, mpInst.args.publishInterval) != RET_OK)
                    {
                        mpInst.state = STATE_ERROR;
                        break;
                    }
                    mpInst.lastDataPubTs = (UINT64)nowTs;
                }

                if ((mpInst.lastStatePubTs == 0U) || ((UINT64)nowTs - mpInst.lastStatePubTs >= mpInst.args.statePublishInterval))
                {
                    publishStateTopic(MQTT_TOPIC_STATE_MODE, mpInst.isAutoMode ? MQTT_MODE_AUTO : MQTT_MODE_MANUAL);
                    publishStateTopic(MQTT_TOPIC_STATE_R1_PIR, mpInst.pir[IDX_HD1] ? "1" : "0");
                    publishStateTopic(MQTT_TOPIC_STATE_R2_PIR, mpInst.pir[IDX_HD2] ? "1" : "0");
                    publishStateTopic(MQTT_TOPIC_STATE_R1_LIGHT, mpInst.outputState[IDX_LMC1] ? "1" : "0");
                    publishStateTopic(MQTT_TOPIC_STATE_R1_FAN, mpInst.outputState[IDX_FMC1] ? "1" : "0");
                    publishStateTopic(MQTT_TOPIC_STATE_R2_LIGHT, mpInst.outputState[IDX_LMC2] ? "1" : "0");
                    publishStateTopic(MQTT_TOPIC_STATE_R2_AC, mpInst.outputState[IDX_AC2] ? "1" : "0");
                    mpInst.lastStatePubTs = (UINT64)nowTs;
                }

                usleep(100000U);
                mpInst.state = STATE_CONNECT_MODBUS;
			}
            break;
            default:
                mpInst.state = STATE_ERROR;
            break;
        }
    }

    /* Cleanup */
    for(idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
    {
        if(mpInst.ctx[idx])
        {
            modbus_close(mpInst.ctx[idx]);
            modbus_free(mpInst.ctx[idx]);
        }
    }

    if(mpInst.db)
        sqlite3_close(mpInst.db);

    if(mpInst.mosq)
    {
        mosquitto_destroy(mpInst.mosq);
        mosquitto_lib_cleanup();
    }

    return RET_OK;
}

/* EOF */

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

#define	CUR_SENS_SIMULATOR	curSs
#define MODBUS_DEBUG		modDebug
#define DEBUG_LOG			debug

/*** Globals ***/
UINT64	flag1;
MP_INST	mpInst;
UINT16	curSs;
BOOL	debug,modDebug;

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
	const CHAR *ssSection[MAX_SENS_SIMULATOR] = {"sensor1","sensor2","sensor3"};
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
* @brief        Reads data from the Modbus TCP server.
*
* @details      This function reads the power consumption data from the Modbus TCP server.
*
* @param[in]    ctx         The Modbus context.
* @param[out]   power       Pointer to the variable where the power consumption data will be stored.
*
* @return       ERROR_CODE  Returns RET_OK if the data is successfully read,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE readModbus(modbus_t *ctx, UINT16 *power)
{
    UINT8 tab_reg[MODBUS_TCP_MAX_ADU_LENGTH]={0};
	UINT16 val=0;
	const uint8_t raw_req[] = { 0xFF, MODBUS_FC_READ_HOLDING_REGISTERS, 0x00, 0x00, 0x00, 0x01 };
	int req_length = modbus_send_raw_request(ctx, raw_req, 6 * sizeof(uint8_t));

	if(modbus_receive_confirmation(ctx, tab_reg) == -1)
    {
        fprintf(stderr, "Failed to read registers: %s\n", modbus_strerror(errno));
        return RET_FAILURE;
    }

	*power = tab_reg[1];
	*power = (*power << 8) | tab_reg[0];
	if(DEBUG_LOG)
		fprintf(stdout, "Received modbus data %d\n",*power);

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

/* Callback for successful connection to the MQTT broker */
static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    if(rc == 0)
	{
		SET_FLAG(MQTT_CONNECTED);
		if(DEBUG_LOG)
			fprintf(stdout,"Connected to MQTT broker successfully.\n");
	}
    else
        fprintf(stderr, "Failed to connect to MQTT broker, return code: %d\n", rc);
}

/* Callback for successful message publication */
static void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
	if(DEBUG_LOG)
		fprintf(stdout,"Message published successfully, message ID: %d\n", mid);
}

/* Callback for logging */
static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	if(DEBUG_LOG)
		fprintf(stdout,"MQTT Log: %s\n", str);
}

static void printUsage(void)
{
    fprintf(stdout,"Usage: ems_mainProc [OPTIONS]\n");
    fprintf(stdout,"Options:\n");
    fprintf(stdout,"  -n <max sensor>       Max number of sensor simulator(Upto 3)\n");
    fprintf(stdout,"  -d                    Enable debug\n");
    fprintf(stdout,"  -h, --help            Show this help message and exit\n");
}

/****************************************************************
* Main
****************************************************************/
INT32 main(INT32 argc, CHAR **argv, CHAR **envp)
{
    INT32	rc = 0;
	UINT16	idx = 0;

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
					mpInst.state = STATE_ERROR;
					break;
				}

				if(mpInst.args.mqttUsername && mpInst.args.mqttPassword)
					mosquitto_username_pw_set(mpInst.mosq, mpInst.args.mqttUsername, mpInst.args.mqttPassword);

				/* Set callbacks */
				mosquitto_connect_callback_set(mpInst.mosq, on_connect);
				mosquitto_publish_callback_set(mpInst.mosq, on_publish);
				mosquitto_log_callback_set(mpInst.mosq, on_log);

				/* Connect to MQTT broker */
				rc = mosquitto_connect(mpInst.mosq, mpInst.args.mqttIP, mpInst.args.mqttPort, 60);
				if(rc != MOSQ_ERR_SUCCESS)
				{
					fprintf(stderr, "Failed to connect to MQTT broker: %s\n", mosquitto_strerror(rc));
					sqlite3_close(mpInst.db);
					mosquitto_destroy(mpInst.mosq);
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
                        if(readModbus(mpInst.ctx[idx], &mpInst.power[idx]) != RET_OK)
                            mpInst.mConnected[idx] = FALSE;
                    }
                }

                mpInst.state = STATE_INSERT_DB;
			}
            break;
            case STATE_INSERT_DB:
			{
                for(idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
                {
                    if(mpInst.mConnected[idx])
                        insertDB(mpInst.db, (idx + 1), mpInst.power[idx]);
                }
                mpInst.state = STATE_PUBLISH_MQTT;
			}
            break;
            case STATE_PUBLISH_MQTT:
			{
                if(publishMQTT(mpInst.mosq, mpInst.db, mpInst.args.publishInterval) != RET_OK)
					mpInst.state = STATE_ERROR;
                else
                {
                    for(idx = 0; idx < CUR_SENS_SIMULATOR; idx++)
                        sleep(mpInst.args.readInterval[idx]);

                    mpInst.state = STATE_CONNECT_MODBUS;
                }
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

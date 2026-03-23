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

#ifndef _GENERAL_H_
#define _GENERAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <modbus/modbus.h>
#include <sqlite3.h>
#include <mosquitto.h>
#include "common.h"
#include "ini.h"

/*
*Macros
*/
#define APP_VERSION				"MP 1.2.0 09042025"
#define MAX_SENS_SIMULATOR		3
#define MIN_MQTT_PUB_INTERVAL	1
#define MAX_MQTT_PUB_INTERVAL	59
#define MQTT_PAYLOAD_MIN_SIZE   2

#define CONFIG_FILE				"/root/config/config.ini"
#define MQTT_CLIENT_ID			"ems_main_proc"
#define MQTT_TOPIC				"sensor/data"
#define DB_NAME					"/root/sensor_data.db"

//for Flags use only
extern UINT64 flag1;

#define	POWER_ON				0
#define	MQTT_CONNECTED			1

#define SET_FLAG(n)				((flag1) |= (UINT64)(1ULL << (n)))
#define CLR_FLAG(n)				((flag1) &= (UINT64)~((1ULL) << (n)))
#define CHECK_FLAG(n)			((flag1) & (UINT64)(1ULL<<(n)))

/*
*Enum
*/
/* Define state machine states */
typedef enum {
    STATE_INIT,
    STATE_CONNECT_MODBUS,
	STATE_CONNECT_MQTT,
    STATE_READ_MODBUS,
    STATE_INSERT_DB,
    STATE_PUBLISH_MQTT,
    STATE_ERROR
} STATE_TYPE;

/*
*Structure
*/
#pragma pack(push,1)
/* Define structure to hold program arguments */
typedef struct
{
    CHAR		*sensorIP[MAX_SENS_SIMULATOR];
    UINT16		sensorPort[MAX_SENS_SIMULATOR];
    UINT16		readInterval[MAX_SENS_SIMULATOR];
    CHAR		*mqttIP;
    UINT16		mqttPort;
    CHAR		*mqttUsername;
    CHAR		*mqttPassword;
    UINT16		publishInterval;
}PROGRAM_ARGS;

typedef struct
{
    PROGRAM_ARGS		args;
    STATE_TYPE			state;
    modbus_t			*ctx[MAX_SENS_SIMULATOR];
    sqlite3				*db;
    struct mosquitto	*mosq;
    UINT8               mConnected[MAX_SENS_SIMULATOR];
    CHAR                timestamp[SIZE_32];
    UINT16				power[MAX_SENS_SIMULATOR];
    CHAR				payload[SIZE_2048];
}MP_INST;
#pragma pack(pop)

/*
*Function declarations
*/

#endif

/* EOF */

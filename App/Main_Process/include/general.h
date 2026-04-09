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
#define MAX_SENS_SIMULATOR		7
#define MIN_MQTT_PUB_INTERVAL	1
#define MAX_MQTT_PUB_INTERVAL	59
#define MIN_PIR_POLL_MS         100
#define MAX_PIR_POLL_MS         5000
#define MQTT_PAYLOAD_MIN_SIZE   2

#define CONFIG_FILE				"/home/root/config/config_MP.ini"
#define MQTT_CLIENT_ID			"ems_main_proc"
#define MQTT_TOPIC				"sensor/data"
#define MQTT_MODE_AUTO          "AUTO"
#define MQTT_MODE_MANUAL        "MANUAL"

#define MQTT_TOPIC_CMD_MODE         "smarthome/cmd/mode"
#define MQTT_TOPIC_CMD_R1_LIGHT     "smarthome/cmd/room1/light"
#define MQTT_TOPIC_CMD_R1_FAN       "smarthome/cmd/room1/fan"
#define MQTT_TOPIC_CMD_R2_LIGHT     "smarthome/cmd/room2/light"
#define MQTT_TOPIC_CMD_R2_AC        "smarthome/cmd/room2/ac"
#define MQTT_TOPIC_STATE_MODE       "smarthome/state/mode"
#define MQTT_TOPIC_STATE_R1_PIR     "smarthome/state/room1/pir"
#define MQTT_TOPIC_STATE_R1_LIGHT   "smarthome/state/room1/light"
#define MQTT_TOPIC_STATE_R1_FAN     "smarthome/state/room1/fan"
#define MQTT_TOPIC_STATE_R2_PIR     "smarthome/state/room2/pir"
#define MQTT_TOPIC_STATE_R2_LIGHT   "smarthome/state/room2/light"
#define MQTT_TOPIC_STATE_R2_AC      "smarthome/state/room2/ac"

#define MODBUS_HOLDING_ADDR_POWER   3000
#define MODBUS_COIL_ADDR_READ       1000
#define MODBUS_COIL_ADDR_WRITE      5000
#define DB_NAME					"/home/root/config/sensor_data.db"

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
    UINT16      pirPollMs[MAX_SENS_SIMULATOR];
    CHAR		*mqttIP;
    UINT16		mqttPort;
    CHAR		*mqttUsername;
    CHAR		*mqttPassword;
    UINT16		publishInterval;
    UINT16      statePublishInterval;
    CHAR        *uiFbdev;
    CHAR        *uiTouchDev;
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
    UINT8               pir[MAX_SENS_SIMULATOR];
    UINT8               outputState[MAX_SENS_SIMULATOR];
    UINT8               manualState[MAX_SENS_SIMULATOR];
    UINT8               isAutoMode;
    UINT64              lastPirPollTs[MAX_SENS_SIMULATOR];
    UINT64              lastDataPubTs;
    UINT64              lastStatePubTs;
    CHAR				payload[SIZE_2048];
}MP_INST;
#pragma pack(pop)

/*
*Function declarations
*/

#endif

/* EOF */

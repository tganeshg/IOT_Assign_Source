/**************************************************************************************
*		
*	BITS Pilani - Copyright (c) 2025
*	All rights reserved.
*
*	Project 		: Assignment - Energy Monitoring System - Semester 1 - SES
*	Author			: Ganesh
*
*	Revision History
***************************************************************************************
*	Date			Version		Name		Description
***************************************************************************************
*	11/02/2025		1.0			Ganesh		Initial Development
*
**************************************************************************************/

#ifndef _GENERAL_H_
#define _GENERAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <modbus/modbus.h>
#include "common.h"

/*
*Macros
*/
#define APP_VERSION				    "SS 1.2.0 09042025"
#define MAX_SENS_SIMULATOR			3

/* Define power consumption ranges */
#define FAN_MIN_POWER               10
#define FAN_MAX_POWER               120
#define AC_MIN_POWER                500
#define AC_MAX_POWER                3500
#define FRIDGE_MIN_POWER            300
#define FRIDGE_MAX_POWER            800

/* Define Modbus register address */
#define MODBUS_REGISTER_ADDRESS     0
#define MODBUS_REGISTER_COUNT       1

/*************************************************************************
* @brief        Enumeration for the state machine states.
*
* @details      This enumeration defines the possible states for the state machine
*               used in the sensor simulator program.
*************************************************************************/
typedef enum {
    STATE_INIT,            /**< Initial state */
    STATE_READ_SENSOR,     /**< State for reading sensor data */
    STATE_SIMULATE_ACCEPT, /**< State for accept the socket */
    STATE_SIMULATE_POWER,  /**< State for simulating power consumption */
    STATE_OUTPUT_POWER,    /**< State for outputting power consumption */
    STATE_RESPOND_MODBUS,  /**< State for responding to Modbus queries */
    STATE_ERROR            /**< Error state */
} STATE_TYPE;

//for Flags use only
extern UINT64 flag1;

#define	POWER_ON				0

#define SET_FLAG(n)				((flag1) |= (U64)(1ULL << (n)))
#define CLR_FLAG(n)				((flag1) &= (U64)~((1ULL) << (n)))
#define CHECK_FLAG(n)			((flag1) & (U64)(1ULL<<(n)))

/*
*Structure
*/
#pragma pack(push,1)

/*************************************************************************
* @brief        Structure to hold the simulation instance data.
*
* @details      This structure contains all the necessary data for a simulation instance,
*               including the state of the state machine, Modbus TCP port, sensor ID,
*               power consumption range, power value, server socket, and Modbus context
*               and mapping.
*************************************************************************/
typedef struct
{
    STATE_TYPE          state;          /**< The current state of the state machine */
    UINT16              modbusPort;     /**< The Modbus TCP port */
    UINT16              sensorID;       /**< The ID of the sensor */
    UINT16              minPower;       /**< The minimum power consumption value */
    UINT16              maxPower;       /**< The maximum power consumption value */
    UINT16              power;          /**< The current power consumption value */
    INT32               serverSocket;   /**< The server socket for Modbus TCP */
    modbus_t            *ctx;           /**< The Modbus context */
    modbus_mapping_t    *mbMapping;     /**< The Modbus mapping */
} SIM_INSTANCE;

#pragma pack(pop)

/*
*Function declarations
*/

#endif

/* EOF */

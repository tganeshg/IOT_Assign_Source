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
#include <string.h>
#include <ctype.h>

#define MODBUS_DEBUG			modDebug
#define DEBUG_LOG				debug
#define MODBUS_COIL_READ_ADDRESS         1000
#define MODBUS_COIL_WRITE_ADDRESS        5000
#define MODBUS_COIL_COUNT_FOR_READ       (MODBUS_COIL_READ_ADDRESS + 1)
#define MODBUS_COIL_COUNT_FOR_CONTROL    (MODBUS_COIL_WRITE_ADDRESS + 1)
/*** Globals ***/
UINT64	flag1;
BOOL	debug,modDebug;

SIM_INSTANCE	simInst;

/****************************************************************
* Private Functions
****************************************************************/

/****************************************************************************************
* @brief        Prints the usage information for the sensor simulator program.
*
* @details      This function prints the usage information for the sensor simulator program,
*               including the available command line options and their descriptions.
*
* @param        None
*
* @return       None
****************************************************************************************/
static void printUsage(void)
{
    fprintf(stdout,"Usage: SensorSimulator -c <config_file>\n");
    fprintf(stdout,"Example:\n");
    fprintf(stdout,"  ./SensorSimulator -c config/config_HD_1.ini\n");
    fprintf(stdout,"Required keys in the sensor config file:\n");
    fprintf(stdout,"  sensorID      (1=HD_1, 2=LMC_1, 3=FMC_1, 4=HD_2, 5=LMC_2, 6=AMC_2, 7=RM_2)\n");
    fprintf(stdout,"  minPower      Minimum power consumption (positive value, not needed for PIR IDs 1/4)\n");
    fprintf(stdout,"  maxPower      Maximum power consumption (positive value, not needed for PIR IDs 1/4)\n");
    fprintf(stdout,"  modbusPort    Modbus TCP port\n");
    fprintf(stdout,"  ConfigGPIO    GPIO pin number (required for control LMC/FMC and PIR HD sensors)\n");
    fprintf(stdout,"Optional keys:\n");
    fprintf(stdout,"  debug         0/1 or false/true\n");
    fprintf(stdout,"  modbusDebug   0/1 or false/true\n");
}

/*************************************************************************
* @brief        Reads configuration from INI style file for sensor simulator.
*
* @details      This function reads and parses key=value pairs from a config file.
*               It extracts sensor ID, minimum power, maximum power, Modbus port,
*               and optional debug flags.
*
* @param[in]    configPath  Path to the config file.
* @param[out]   sensorID    Pointer to the variable where the sensor ID will be stored.
* @param[out]   minPower    Pointer to the variable where the minimum power will be stored.
* @param[out]   maxPower    Pointer to the variable where the maximum power will be stored.
* @param[out]   modbusPort  Pointer to the variable where the Modbus TCP port will be stored.
*
* @return       ERROR_CODE  Returns RET_OK if the arguments are successfully read and valid,
*                           otherwise returns RET_FAILURE.
*************************************************************************/
static ERROR_CODE readRuntimeArguments(INT32 argc, CHAR **argv, CHAR *configPath, size_t configPathSize)
{
    INT32 opt = 0;
    BOOL isConfigProvided = FALSE;

    if (argc <= 1)
    {
        fprintf(stderr, "Missing required option: -c <config_file>\n");
        printUsage();
        return RET_FAILURE;
    }

    while ((opt = getopt(argc, argv, "c:h")) != RET_FAILURE)
    {
        switch (opt)
        {
            case 'c':
                strncpy(configPath, optarg, configPathSize - 1U);
                configPath[configPathSize - 1U] = '\0';
                isConfigProvided = TRUE;
            break;
            case 'h':
                printUsage();
                return RET_FAILURE;
            default:
                printUsage();
                return RET_FAILURE;
        }
    }

    if (isConfigProvided == FALSE)
    {
        fprintf(stderr, "Please provide a config file using -c\n");
        printUsage();
        return RET_FAILURE;
    }

    return RET_OK;
}

static CHAR* trimWhitespace(CHAR *str)
{
    CHAR *end = NULL;

    while (*str && isspace((unsigned char)*str))
    {
        str++;
    }

    if (*str == '\0')
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return str;
}

static BOOL parseBoolValue(const CHAR *value)
{
    if ((strcmp(value, "1") == 0) || (strcasecmp(value, "true") == 0) || (strcasecmp(value, "yes") == 0))
    {
        return TRUE;
    }
    return FALSE;
}

static BOOL isControlSensor(UINT16 sensorID)
{
    if ((sensorID == 2U) || (sensorID == 3U) || (sensorID == 5U) || (sensorID == 6U))
    {
        return TRUE;
    }
    return FALSE;
}

static BOOL isPIRSensor(UINT16 sensorID)
{
    if ((sensorID == 1U) || (sensorID == 4U))
    {
        return TRUE;
    }
    return FALSE;
}

static ERROR_CODE writeStringToFile(const CHAR *path, const CHAR *value)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        return RET_FAILURE;
    }

    if (fprintf(fp, "%s", value) < 0)
    {
        fclose(fp);
        return RET_FAILURE;
    }

    fclose(fp);
    return RET_OK;
}

static ERROR_CODE configureGPIO(UINT16 gpioPin, const CHAR *direction, BOOL defaultHigh)
{
    CHAR path[128] = {0};
    CHAR value[16] = {0};

    snprintf(value, sizeof(value), "%u", gpioPin);
    if (writeStringToFile("/sys/class/gpio/export", value) != RET_OK)
    {
        if (errno != EBUSY)
        {
            fprintf(stderr, "Failed to export GPIO %u\n", gpioPin);
            return RET_FAILURE;
        }
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", gpioPin);
    if (writeStringToFile(path, direction) != RET_OK)
    {
        fprintf(stderr, "Failed to set GPIO %u direction to %s\n", gpioPin, direction);
        return RET_FAILURE;
    }

    if (strcmp(direction, "out") == 0)
    {
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpioPin);
        if (writeStringToFile(path, (defaultHigh == TRUE) ? "1" : "0") != RET_OK)
        {
            fprintf(stderr, "Failed to initialize GPIO %u value\n", gpioPin);
            return RET_FAILURE;
        }
    }

    return RET_OK;
}

static ERROR_CODE setGPIOValue(UINT16 gpioPin, BOOL isHigh)
{
    CHAR path[128] = {0};

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpioPin);
    return writeStringToFile(path, (isHigh == TRUE) ? "1" : "0");
}

static ERROR_CODE readGPIOValue(UINT16 gpioPin, UINT8 *value)
{
    FILE *fp = NULL;
    CHAR path[128] = {0};
    CHAR c = '0';

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpioPin);
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return RET_FAILURE;
    }

    if (fread(&c, 1, 1, fp) != 1)
    {
        fclose(fp);
        return RET_FAILURE;
    }

    fclose(fp);
    *value = (c == '1') ? 1U : 0U;
    return RET_OK;
}

static ERROR_CODE readConfigFile(const CHAR *configPath, UINT16 *sensorID, UINT16 *minPower, UINT16 *maxPower, UINT16 *modbusPort, UINT16 *controlGPIO)
{
    FILE *fp = NULL;
    CHAR line[256] = {0};
    BOOL foundSensorID = FALSE, foundMinPower = FALSE, foundMaxPower = FALSE, foundModbusPort = FALSE, foundControlGPIO = FALSE;
    CHAR *linePtr = NULL, *key = NULL, *value = NULL, *eqPtr = NULL;

    fp = fopen(configPath, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to open config file: %s\n", configPath);
        return RET_FAILURE;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        linePtr = trimWhitespace(line);
        if ((*linePtr == '\0') || (*linePtr == ';') || (*linePtr == '#') || (*linePtr == '['))
        {
            continue;
        }

        eqPtr = strchr(linePtr, '=');
        if (eqPtr == NULL)
        {
            continue;
        }

        *eqPtr = '\0';
        key = trimWhitespace(linePtr);
        value = trimWhitespace(eqPtr + 1);

        if (strcmp(key, "sensorID") == 0)
        {
            *sensorID = (UINT16)atoi(value);
            foundSensorID = TRUE;
        }
        else if (strcmp(key, "minPower") == 0)
        {
            *minPower = (UINT16)atoi(value);
            foundMinPower = TRUE;
        }
        else if (strcmp(key, "maxPower") == 0)
        {
            *maxPower = (UINT16)atoi(value);
            foundMaxPower = TRUE;
        }
        else if (strcmp(key, "modbusPort") == 0)
        {
            *modbusPort = (UINT16)atoi(value);
            foundModbusPort = TRUE;
        }
        else if ((strcmp(key, "controlGPIO") == 0) || (strcmp(key, "ControlGPIO") == 0) || (strcmp(key, "ConfigGPIO") == 0) || (strcmp(key, "configGPIO") == 0))
        {
            *controlGPIO = (UINT16)atoi(value);
            foundControlGPIO = TRUE;
        }
        else if (strcmp(key, "debug") == 0)
        {
            debug = parseBoolValue(value);
        }
        else if (strcmp(key, "modbusDebug") == 0)
        {
            modDebug = parseBoolValue(value);
        }
    }

    fclose(fp);

    if ((foundSensorID == FALSE) || (foundModbusPort == FALSE))
    {
        fprintf(stderr, "Missing required configuration keys in %s\n", configPath);
        printUsage();
        return RET_FAILURE;
    }

    if ((isPIRSensor(*sensorID) == FALSE) && ((foundMinPower == FALSE) || (foundMaxPower == FALSE)))
    {
        fprintf(stderr, "Missing required minPower/maxPower for non-PIR sensor in %s\n", configPath);
        printUsage();
        return RET_FAILURE;
    }

    if (((isControlSensor(*sensorID) == TRUE) || (isPIRSensor(*sensorID) == TRUE)) && (foundControlGPIO == FALSE))
    {
        fprintf(stderr, "Missing required key ConfigGPIO for sensor in %s\n", configPath);
        printUsage();
        return RET_FAILURE;
    }

    if ((*sensorID < 1 || *sensorID > MAX_SENS_SIMULATOR) || (*modbusPort == 0))
	{
		fprintf(stderr, "Invalid inputs\n");
		printUsage();
        return RET_FAILURE;
	}

    if ((isPIRSensor(*sensorID) == FALSE) && ((*minPower == 0) || (*maxPower == 0) || (*minPower > *maxPower)))
    {
        fprintf(stderr, "Invalid minPower/maxPower\n");
        printUsage();
        return RET_FAILURE;
    }

    if (((isControlSensor(*sensorID) == TRUE) || (isPIRSensor(*sensorID) == TRUE)) && (*controlGPIO == 0))
    {
        fprintf(stderr, "Invalid ConfigGPIO\n");
        printUsage();
        return RET_FAILURE;
    }

    return RET_OK;
}

/*************************************************************************
* @brief        Simulates power consumption within a specified range.
*
* @details      This function generates a gradual power consumption value within
*               the specified minimum and maximum power range.
*
* @param[in]    minPower    The minimum power consumption value.
* @param[in]    maxPower    The maximum power consumption value.
*
* @return       UINT16      Returns a gradual power consumption value within the specified range.
*************************************************************************/
static UINT16 simulatePowerConsumption(UINT16 minPower, UINT16 maxPower)
{
    static UINT16 currentPower = 0;
    static BOOL increasing = TRUE;
    static BOOL seeded = FALSE;
	UINT16 num = 0;

    if ((currentPower < minPower) || (currentPower > maxPower))
    {
        currentPower = minPower;
    }

    if (seeded == FALSE)
    {
        srand((UINT32)time(NULL) ^ (UINT32)getpid());
        seeded = TRUE;
    }

	num = (UINT16)(rand() % 6);
    if (increasing) {
        currentPower += num;
        if (currentPower >= maxPower) {
            increasing = FALSE;
        }
    } else {
        currentPower -= num;
        if (currentPower <= minPower) {
            increasing = TRUE;
        }
    }

    return currentPower;
}

/*************************************************************************
* @brief        Outputs the power consumption for a given sensor.
*
* @details      This function prints the power consumption value for a specified sensor ID.
*
* @param[in]    sensorID    The ID of the sensor.
* @param[in]    power       The power consumption value to be printed.
*
* @return       None
*************************************************************************/
static void outputPowerConsumption(UINT16 sensorID, UINT16 power)
{
    fprintf(stdout,"Sensor ID: %d, Power Consumption: %d watts\n", sensorID, power);
}

/****************************************************************
* Main
****************************************************************/
/****************************************************************
* @brief        Main function for the sensor simulator program.
*
* @details      This function initializes the sensor simulator, reads command line arguments,
*               sets up the Modbus TCP server, and runs the state machine to simulate power
*               consumption and respond to Modbus queries.
*
* @param[in]    argc        The number of command line arguments.
* @param[in]    argv        The array of command line arguments.
* @param[in]    envp        The array of environment variables.
*
* @return       INT32       Returns RET_OK if the program runs successfully,
*                           otherwise returns RET_FAILURE.
****************************************************************/
INT32 main(INT32 argc, CHAR **argv, CHAR **envp)
{
	struct sockaddr_in clientAddr;
	socklen_t addrLen = 0;
	CHAR clientIp[INET_ADDRSTRLEN]={0};
    UINT8 query[MODBUS_TCP_MAX_ADU_LENGTH] = {0};
    UINT8 prevCoilState = 0;
    UINT16 coilCount = 0;
    UINT16 registerCount = 0;
    CHAR configPath[256] = {0};
    INT32 rc=0,clientSocket=0;
	const CHAR *sensorName[MAX_SENS_SIMULATOR] = {"HD_1","LMC_1","FMC_1","HD_2","LMC_2","AMC_2","RM_2"};

    (void)envp;

    if(readRuntimeArguments(argc, argv, configPath, sizeof(configPath)) != RET_OK)
    {
        return RET_FAILURE;
    }

    if(readConfigFile(configPath, &simInst.sensorID, &simInst.minPower, &simInst.maxPower, &simInst.modbusPort, &simInst.controlGPIO) != RET_OK)
	{
        return RET_FAILURE;
	}

    if (isControlSensor(simInst.sensorID) == TRUE)
    {
        coilCount = MODBUS_COIL_COUNT_FOR_CONTROL;
    }
    else if (isPIRSensor(simInst.sensorID) == TRUE)
    {
        coilCount = MODBUS_COIL_COUNT_FOR_READ;
    }
    else
    {
        coilCount = 0;
    }
    registerCount = (isPIRSensor(simInst.sensorID) == TRUE) ? 0 : MODBUS_REGISTER_COUNT;

    if ((isControlSensor(simInst.sensorID) == TRUE) && (configureGPIO(simInst.controlGPIO, "out", FALSE) != RET_OK))
    {
        return RET_FAILURE;
    }

    if ((isPIRSensor(simInst.sensorID) == TRUE) && (configureGPIO(simInst.controlGPIO, "in", FALSE) != RET_OK))
    {
        return RET_FAILURE;
    }

	if(DEBUG_LOG)
	{
		fprintf(stdout,"\n<< EMS - Sensor Simulator (%s) v%s >>\n\n",sensorName[simInst.sensorID-1],APP_VERSION);
		fprintf(stdout,"Sensor ID :%d\n\tRange of power %d to %d watts\n\tModbus Port : %d\n",simInst.sensorID, simInst.minPower, simInst.maxPower, simInst.modbusPort);
        if ((isControlSensor(simInst.sensorID) == TRUE) || (isPIRSensor(simInst.sensorID) == TRUE))
        {
            fprintf(stdout,"\tConfig GPIO : %d\n", simInst.controlGPIO);
        }
	}

	while (simInst.state != STATE_ERROR)
    {
        switch (simInst.state)
        {
            case STATE_INIT:
			{
				simInst.ctx = modbus_new_tcp("0.0.0.0", simInst.modbusPort);
				if (simInst.ctx == NULL)
				{
					fprintf(stderr, "Unable to allocate libmodbus context\n");
					return RET_FAILURE;
				}

				simInst.mbMapping = modbus_mapping_new(coilCount, 0, registerCount, 0);
				if (simInst.mbMapping == NULL)
				{
					fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
					modbus_close(simInst.ctx);
					modbus_free(simInst.ctx);
					return RET_FAILURE;
				}
                if (isControlSensor(simInst.sensorID) == TRUE)
                {
                    simInst.mbMapping->tab_bits[MODBUS_COIL_READ_ADDRESS] = 0;
                    simInst.mbMapping->tab_bits[MODBUS_COIL_WRITE_ADDRESS] = 0;
                }

				simInst.serverSocket = modbus_tcp_listen(simInst.ctx, 1);
				if (simInst.serverSocket == RET_FAILURE)
				{
					fprintf(stderr, "Unable to listen TCP connection: %s\n", modbus_strerror(errno));
					modbus_mapping_free(simInst.mbMapping);
					modbus_close(simInst.ctx);
					modbus_free(simInst.ctx);
					return RET_FAILURE;
				}
                simInst.state = STATE_SIMULATE_ACCEPT;
			}
            break;
            case STATE_SIMULATE_ACCEPT:
			{
                if(DEBUG_LOG)
					fprintf(stdout,"Waiting for server request from Main Process..\n");

				clientSocket = modbus_tcp_accept(simInst.ctx, &simInst.serverSocket);
				if(clientSocket == RET_FAILURE)
				{
					fprintf(stderr,"Failed accept request from Main Process..\n");
					break;
				}
				else
				{
					if(DEBUG_LOG)
					{
						fprintf(stdout,"Accepted request from Main Process..\n");

						// Get the IP address of the client
						addrLen = sizeof(clientAddr);
						if (getpeername(clientSocket, (struct sockaddr *)&clientAddr, &addrLen) == RET_FAILURE)
						{
							perror("Get Peer Name of MP");
							break;
						}

						inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
						fprintf(stdout,"Client connected from IP: %s\n", clientIp);
					}
				}
				// Enable debug mode for the Modbus context
				if(MODBUS_DEBUG)
					modbus_set_debug(simInst.ctx, TRUE);

                simInst.state = STATE_SIMULATE_POWER;
			}
			break;
            case STATE_SIMULATE_POWER:
			{
                if (isPIRSensor(simInst.sensorID) == TRUE)
                {
                    simInst.state = STATE_RESPOND_MODBUS;
                }
                else
                {
                    simInst.power = simulatePowerConsumption(simInst.minPower, simInst.maxPower);
                    simInst.state = STATE_OUTPUT_POWER;
                }
			}
            break;
            case STATE_OUTPUT_POWER:
			{
				if(DEBUG_LOG)
					outputPowerConsumption(simInst.sensorID, simInst.power);

                simInst.state = STATE_RESPOND_MODBUS;
			}
            break;
			case STATE_RESPOND_MODBUS:
			{
				rc = modbus_receive(simInst.ctx, query);
				if (rc > 0)
				{
                    if (isPIRSensor(simInst.sensorID) == TRUE)
                    {
                        if (readGPIOValue(simInst.controlGPIO, &simInst.mbMapping->tab_bits[MODBUS_COIL_READ_ADDRESS]) != RET_OK)
                        {
                            fprintf(stderr, "Failed to read PIR GPIO %u\n", simInst.controlGPIO);
                        }
                    }

                    if (registerCount > 0U)
                    {
					    simInst.mbMapping->tab_registers[MODBUS_REGISTER_ADDRESS] = (UINT16)simInst.power;
                    }
                    if ((isPIRSensor(simInst.sensorID) == TRUE) && (query[7] == MODBUS_FC_WRITE_SINGLE_COIL))
                    {
                        modbus_reply_exception(simInst.ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                    }
                    else
                    {
					    modbus_reply(simInst.ctx, query, rc, simInst.mbMapping);
                    }

                    if (isControlSensor(simInst.sensorID) == TRUE)
                    {
                        if (simInst.mbMapping->tab_bits[MODBUS_COIL_WRITE_ADDRESS] != prevCoilState)
                        {
                            prevCoilState = simInst.mbMapping->tab_bits[MODBUS_COIL_WRITE_ADDRESS];
                            simInst.mbMapping->tab_bits[MODBUS_COIL_READ_ADDRESS] = prevCoilState;
                            if (setGPIOValue(simInst.controlGPIO, (BOOL)prevCoilState) != RET_OK)
                            {
                                fprintf(stderr, "Failed to set GPIO %u state to %u\n", simInst.controlGPIO, prevCoilState);
                            }
                            else if (DEBUG_LOG)
                            {
                                fprintf(stdout, "ControlGPIO %u set to %s\n", simInst.controlGPIO, (prevCoilState == 1U) ? "HIGH" : "LOW");
                            }
                        }
                    }

					simInst.state = STATE_SIMULATE_POWER;
				}
				else
				{
					if(errno == ECONNRESET)
					{
						fprintf(stderr, "Socket disconnected: %s\n", modbus_strerror(errno));
						simInst.state = STATE_SIMULATE_ACCEPT;
					}
				}
			}
			break;
            default:
				simInst.state = STATE_ERROR;
			break;
        }
    }

    modbus_mapping_free(simInst.mbMapping);
    modbus_close(simInst.ctx);
    modbus_free(simInst.ctx);

    return RET_OK;
}

/* EOF */

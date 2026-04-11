#ifndef _UI_LVGL_H_
#define _UI_LVGL_H_

#include "common.h"

typedef struct
{
    UINT8   isAutoModeRoom1;
    UINT8   isAutoModeRoom2;
    UINT8   mqttConnected;
    UINT8   modbusConnectedCount;
    UINT8   modbusTotalDevices;
    UINT8   pirRoom1;
    UINT8   pirRoom2;
    /* Auto: alternate-motion “occupied” (matches loads); Manual: raw PIR for this column. */
    UINT8   personOccupiedRoom1;
    UINT8   personOccupiedRoom2;
    UINT8   room1Light;
    UINT8   room1Fan;
    UINT8   room2Light;
    UINT8   room2AC;
    UINT16  room1LightPowerW;
    UINT16  room1FanPowerW;
    UINT16  room2LightPowerW;
    UINT16  room2ACPowerW;
    UINT16  room2FridgePowerW;
} UI_STATE;

typedef struct
{
    UINT8   hasRoom1Mode;
    UINT8   room1Auto;
    UINT8   hasRoom2Mode;
    UINT8   room2Auto;
    UINT8   hasRoom1Light;
    UINT8   room1Light;
    UINT8   hasRoom1Fan;
    UINT8   room1Fan;
    UINT8   hasRoom2Light;
    UINT8   room2Light;
    UINT8   hasRoom2AC;
    UINT8   room2AC;
} UI_COMMANDS;

ERROR_CODE uiInit(const CHAR *fbdevPath, const CHAR *touchDevPath);
VOID uiProcess(VOID);
VOID uiUpdateState(const UI_STATE *state);
ERROR_CODE uiFetchCommands(UI_COMMANDS *cmd);

#endif

/* EOF */

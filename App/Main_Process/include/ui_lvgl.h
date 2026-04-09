#ifndef _UI_LVGL_H_
#define _UI_LVGL_H_

#include "common.h"

typedef struct
{
    UINT8   isAutoMode;
    UINT8   pirRoom1;
    UINT8   pirRoom2;
    UINT8   room1Light;
    UINT8   room1Fan;
    UINT8   room2Light;
    UINT8   room2AC;
} UI_STATE;

typedef struct
{
    UINT8   hasMode;
    UINT8   isAutoMode;
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

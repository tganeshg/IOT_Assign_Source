#ifndef _LVGL_PORT_LINUX_H_
#define _LVGL_PORT_LINUX_H_

#include "common.h"

ERROR_CODE lv_port_linux_init(const CHAR *fbdevPath, const CHAR *touchDevPath);
VOID lv_port_linux_tick(VOID);

#endif

#include "common.h"
#include "ui_lvgl.h"
#include "lvgl_port_linux.h"
#include <stdint.h>
#include <string.h>

static UI_STATE gUiState;
static UI_COMMANDS gUiCmd;

#ifdef ENABLE_LVGL
#include <lvgl.h>

static lv_obj_t *gModeSwitch;
static lv_obj_t *gModeLabel;
static lv_obj_t *gPir1Label;
static lv_obj_t *gPir2Label;
static lv_obj_t *gR1LightSwitch;
static lv_obj_t *gR1FanSwitch;
static lv_obj_t *gR2LightSwitch;
static lv_obj_t *gR2ACSwitch;
static BOOL gUiSyncInProgress = FALSE;

typedef enum
{
    CTRL_MODE = 0,
    CTRL_R1_LIGHT,
    CTRL_R1_FAN,
    CTRL_R2_LIGHT,
    CTRL_R2_AC
} UI_CTRL_ID;

static VOID setSwitchState(lv_obj_t *sw, UINT8 isOn)
{
    if (sw == NULL)
        return;

    if (isOn)
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

static UINT8 getSwitchState(lv_obj_t *sw)
{
    if (sw == NULL)
        return 0U;

    return (lv_obj_has_state(sw, LV_STATE_CHECKED) != 0) ? 1U : 0U;
}

static VOID onSwitchEvent(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    UI_CTRL_ID id = (UI_CTRL_ID)(INT32)(intptr_t)lv_event_get_user_data(e);
    UINT8 state = getSwitchState(target);

    if (gUiSyncInProgress == TRUE)
        return;

    switch (id)
    {
        case CTRL_MODE:
            gUiCmd.hasMode = 1U;
            gUiCmd.isAutoMode = state;
        break;
        case CTRL_R1_LIGHT:
            gUiCmd.hasRoom1Light = 1U;
            gUiCmd.room1Light = state;
        break;
        case CTRL_R1_FAN:
            gUiCmd.hasRoom1Fan = 1U;
            gUiCmd.room1Fan = state;
        break;
        case CTRL_R2_LIGHT:
            gUiCmd.hasRoom2Light = 1U;
            gUiCmd.room2Light = state;
        break;
        case CTRL_R2_AC:
            gUiCmd.hasRoom2AC = 1U;
            gUiCmd.room2AC = state;
        break;
        default:
        break;
    }
}

static lv_obj_t* createLabeledSwitch(lv_obj_t *parent, const CHAR *text, UI_CTRL_ID id)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_add_event_cb(sw, onSwitchEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
    return sw;
}
#endif

ERROR_CODE uiInit(const CHAR *fbdevPath, const CHAR *touchDevPath)
{
#ifndef ENABLE_LVGL
    (void)fbdevPath;
    (void)touchDevPath;
#endif
#ifdef ENABLE_LVGL
    lv_init();
    if (lv_port_linux_init(fbdevPath, touchDevPath) != RET_OK)
    {
        return RET_FAILURE;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_pad_all(scr, 8, 0);

    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *title = lv_label_create(root);
    lv_label_set_text(title, "Smart Home Control");

    lv_obj_t *modeRow = lv_obj_create(root);
    lv_obj_set_width(modeRow, lv_pct(100));
    lv_obj_set_height(modeRow, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(modeRow, 6, 0);
    lv_obj_set_flex_flow(modeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modeRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gModeLabel = lv_label_create(modeRow);
    lv_label_set_text(gModeLabel, "Mode: MANUAL");
    gModeSwitch = lv_switch_create(modeRow);
    lv_obj_add_event_cb(gModeSwitch, onSwitchEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)CTRL_MODE);

    gPir1Label = lv_label_create(root);
    lv_label_set_text(gPir1Label, "Room1 PIR: 0");
    gPir2Label = lv_label_create(root);
    lv_label_set_text(gPir2Label, "Room2 PIR: 0");

    gR1LightSwitch = createLabeledSwitch(root, "Room1 Light", CTRL_R1_LIGHT);
    gR1FanSwitch = createLabeledSwitch(root, "Room1 Fan", CTRL_R1_FAN);
    gR2LightSwitch = createLabeledSwitch(root, "Room2 Light", CTRL_R2_LIGHT);
    gR2ACSwitch = createLabeledSwitch(root, "Room2 AC", CTRL_R2_AC);
#endif
    memset(&gUiState, 0, sizeof(gUiState));
    memset(&gUiCmd, 0, sizeof(gUiCmd));
    return RET_OK;
}

VOID uiProcess(VOID)
{
#ifdef ENABLE_LVGL
    lv_port_linux_tick();
    lv_timer_handler();
#endif
}

VOID uiUpdateState(const UI_STATE *state)
{
    if (state == NULL)
        return;

    gUiState = *state;

#ifdef ENABLE_LVGL
    gUiSyncInProgress = TRUE;
    setSwitchState(gModeSwitch, gUiState.isAutoMode);
    if (gModeLabel)
        lv_label_set_text(gModeLabel, gUiState.isAutoMode ? "Mode: AUTO" : "Mode: MANUAL");

    if (gPir1Label)
        lv_label_set_text_fmt(gPir1Label, "Room1 PIR: %u", gUiState.pirRoom1);
    if (gPir2Label)
        lv_label_set_text_fmt(gPir2Label, "Room2 PIR: %u", gUiState.pirRoom2);

    setSwitchState(gR1LightSwitch, gUiState.room1Light);
    setSwitchState(gR1FanSwitch, gUiState.room1Fan);
    setSwitchState(gR2LightSwitch, gUiState.room2Light);
    setSwitchState(gR2ACSwitch, gUiState.room2AC);

    if (gUiState.isAutoMode)
    {
        lv_obj_add_state(gR1LightSwitch, LV_STATE_DISABLED);
        lv_obj_add_state(gR1FanSwitch, LV_STATE_DISABLED);
        lv_obj_add_state(gR2LightSwitch, LV_STATE_DISABLED);
        lv_obj_add_state(gR2ACSwitch, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_clear_state(gR1LightSwitch, LV_STATE_DISABLED);
        lv_obj_clear_state(gR1FanSwitch, LV_STATE_DISABLED);
        lv_obj_clear_state(gR2LightSwitch, LV_STATE_DISABLED);
        lv_obj_clear_state(gR2ACSwitch, LV_STATE_DISABLED);
    }
    gUiSyncInProgress = FALSE;
#endif
}

ERROR_CODE uiFetchCommands(UI_COMMANDS *cmd)
{
    if (cmd == NULL)
        return RET_FAILURE;

    *cmd = gUiCmd;
    memset(&gUiCmd, 0, sizeof(gUiCmd));
    return RET_OK;
}

/* EOF */

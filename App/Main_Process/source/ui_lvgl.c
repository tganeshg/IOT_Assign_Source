#include "common.h"
#include "ui_lvgl.h"
#include "lvgl_port_linux.h"
#include <stdint.h>
#include <string.h>

static UI_STATE gUiState;
static UI_COMMANDS gUiCmd;

#ifdef ENABLE_LVGL
#include <lvgl.h>

#if !defined(LVGL_VERSION_MAJOR) || (LVGL_VERSION_MAJOR < 9)
#error "ui_lvgl requires LVGL 9 or newer."
#endif

static lv_style_t gStyleScreen;
static lv_style_t gStyleCard;
static lv_style_t gStyleTitle;
static lv_style_t gStyleAppTitle;
static lv_style_t gStyleMuted;
static lv_style_t gStyleGood;
static lv_style_t gStyleBad;
static lv_style_t gStyleHomeBtnOff;
static lv_style_t gStyleHomeBtnOn;

/* Prefer larger built-in fonts when enabled in lv_conf.h */
#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
#define UI_FONT_TITLE   (&lv_font_montserrat_28)
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
#define UI_FONT_TITLE   (&lv_font_montserrat_24)
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
#define UI_FONT_TITLE   (&lv_font_montserrat_20)
#else
#define UI_FONT_TITLE   lv_font_get_default()
#endif

#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
#define UI_FONT_BODY    (&lv_font_montserrat_24)
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
#define UI_FONT_BODY    (&lv_font_montserrat_20)
#elif defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
#define UI_FONT_BODY    (&lv_font_montserrat_18)
#else
#define UI_FONT_BODY    lv_font_get_default()
#endif

#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
#define UI_FONT_SMALL   (&lv_font_montserrat_20)
#elif defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
#define UI_FONT_SMALL   (&lv_font_montserrat_18)
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
#define UI_FONT_SMALL   (&lv_font_montserrat_16)
#else
#define UI_FONT_SMALL   lv_font_get_default()
#endif

/* Larger than card titles — “Smart Home” headline (enable bigger sizes in lv_conf when possible). */
#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
#define UI_FONT_APP_TITLE (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_44) && LV_FONT_MONTSERRAT_44
#define UI_FONT_APP_TITLE (&lv_font_montserrat_44)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
#define UI_FONT_APP_TITLE (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
#define UI_FONT_APP_TITLE (&lv_font_montserrat_36)
#elif defined(LV_FONT_MONTSERRAT_34) && LV_FONT_MONTSERRAT_34
#define UI_FONT_APP_TITLE (&lv_font_montserrat_34)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
#define UI_FONT_APP_TITLE (&lv_font_montserrat_32)
#elif defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
#define UI_FONT_APP_TITLE (&lv_font_montserrat_28)
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
#define UI_FONT_APP_TITLE (&lv_font_montserrat_24)
#else
#define UI_FONT_APP_TITLE UI_FONT_TITLE
#endif

static lv_obj_t *gR1ModeSwitch;
static lv_obj_t *gR1ModeLabel;
static lv_obj_t *gR2ModeSwitch;
static lv_obj_t *gR2ModeLabel;
static lv_obj_t *gR2FridgePowerLbl;
static lv_obj_t *gStatusLineLabel;
static lv_obj_t *gRoomSelBtn1;
static lv_obj_t *gRoomSelBtn2;
static lv_obj_t *gRoom1Panel;
static lv_obj_t *gRoom2Panel;

static lv_obj_t *gPir1ValueLabel;
static lv_obj_t *gPir2ValueLabel;

static lv_obj_t *gR1LightBtn;
static lv_obj_t *gR1LightOnLbl;
static lv_obj_t *gR1LightPowerLbl;
static lv_obj_t *gR1FanBtn;
static lv_obj_t *gR1FanOnLbl;
static lv_obj_t *gR1FanPowerLbl;
static lv_obj_t *gR2LightBtn;
static lv_obj_t *gR2LightOnLbl;
static lv_obj_t *gR2LightPowerLbl;
static lv_obj_t *gR2ACBtn;
static lv_obj_t *gR2ACOnLbl;
static lv_obj_t *gR2ACPowerLbl;

static BOOL gUiSyncInProgress = FALSE;

typedef enum
{
    CTRL_MODE_R1 = 0,
    CTRL_MODE_R2,
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

static VOID setHomeBtnVisual(lv_obj_t *btn, lv_obj_t *onLbl, UINT8 isOn)
{
    if (btn == NULL)
        return;

    setSwitchState(btn, isOn);
    if (onLbl != NULL)
        lv_label_set_text(onLbl, isOn ? "ON" : "OFF");
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
        case CTRL_MODE_R1:
            gUiCmd.hasRoom1Mode = 1U;
            gUiCmd.room1Auto = state;
        break;
        case CTRL_MODE_R2:
            gUiCmd.hasRoom2Mode = 1U;
            gUiCmd.room2Auto = state;
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

    /* Update ON/OFF label on the pressed home button immediately */
    switch (id)
    {
        case CTRL_R1_LIGHT:
            if (gR1LightOnLbl)
                lv_label_set_text(gR1LightOnLbl, state ? "ON" : "OFF");
        break;
        case CTRL_R1_FAN:
            if (gR1FanOnLbl)
                lv_label_set_text(gR1FanOnLbl, state ? "ON" : "OFF");
        break;
        case CTRL_R2_LIGHT:
            if (gR2LightOnLbl)
                lv_label_set_text(gR2LightOnLbl, state ? "ON" : "OFF");
        break;
        case CTRL_R2_AC:
            if (gR2ACOnLbl)
                lv_label_set_text(gR2ACOnLbl, state ? "ON" : "OFF");
        break;
        default:
        break;
    }
}

static VOID setRoomTabVisual(lv_obj_t *btn, UINT8 selected)
{
    if (btn == NULL)
        return;

    if (selected)
    {
        lv_obj_set_style_bg_opa(btn, LV_OPA_100, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1565C0), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x0D47A1), 0);
    }
    else
    {
        lv_obj_set_style_bg_opa(btn, LV_OPA_100, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xECEFF1), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x37474F), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xB0BEC5), 0);
    }
}

static VOID applyRoomSelection(uint32_t room)
{
    if (room == 0U)
    {
        lv_obj_clear_flag(gRoom1Panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(gRoom2Panel, LV_OBJ_FLAG_HIDDEN);
        setRoomTabVisual(gRoomSelBtn1, 1U);
        setRoomTabVisual(gRoomSelBtn2, 0U);
    }
    else
    {
        lv_obj_add_flag(gRoom1Panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(gRoom2Panel, LV_OBJ_FLAG_HIDDEN);
        setRoomTabVisual(gRoomSelBtn1, 0U);
        setRoomTabVisual(gRoomSelBtn2, 1U);
    }
}

static VOID onRoomSelectClick(lv_event_t *e)
{
    uint32_t room = (uint32_t)(intptr_t)lv_event_get_user_data(e);

    if (gUiSyncInProgress == TRUE)
        return;

    applyRoomSelection(room);
}

/* Left column width — keeps labels aligned and puts controls just after it (no huge center gap). */
#define UI_ROW_LABEL_PCT 38

static VOID createRoomModeRow(lv_obj_t *parent, UI_CTRL_ID modeId, lv_obj_t **outSw, lv_obj_t **outLbl,
                               lv_obj_t **outFridgeLbl)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 2, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    {
        lv_obj_t *k = lv_label_create(row);
        lv_label_set_text(k, "Mode");
        lv_obj_add_style(k, &gStyleMuted, 0);
        lv_obj_set_width(k, lv_pct(UI_ROW_LABEL_PCT));
    }

    {
        /* Controls start at ~38% — not pinned to far right, so no empty “dead” band in the middle. */
        lv_obj_t *right = lv_obj_create(row);
        lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right, 0, 0);
        lv_obj_set_style_pad_all(right, 0, 0);
        lv_obj_set_height(right, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_layout(right, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(right, 6, 0);
        lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

        *outLbl = lv_label_create(right);
        lv_label_set_text(*outLbl, "Manual");
        lv_obj_add_style(*outLbl, &gStyleMuted, 0);
        lv_obj_set_style_pad_top(*outLbl, 0, 0);
        lv_obj_set_style_pad_bottom(*outLbl, 0, 0);

        *outSw = lv_switch_create(right);
        lv_obj_set_size(*outSw, 68, 32);
        lv_obj_add_event_cb(*outSw, onSwitchEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)modeId);

        if (outFridgeLbl != NULL)
        {
            *outFridgeLbl = lv_label_create(right);
            lv_label_set_text(*outFridgeLbl, "Fridge 0 W");
            lv_obj_add_style(*outFridgeLbl, &gStyleMuted, 0);
            lv_obj_set_style_text_font(*outFridgeLbl, UI_FONT_TITLE, 0);
            lv_obj_set_flex_grow(*outFridgeLbl, 1);
            lv_obj_set_style_text_align(*outFridgeLbl, LV_TEXT_ALIGN_RIGHT, 0);
        }
    }
}

static lv_obj_t *createHomeToggleRow(lv_obj_t *parent, const CHAR *name, UI_CTRL_ID id, lv_obj_t **outOnLbl, lv_obj_t **outPowerLbl)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 46, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFAFBFC), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xDDE3EA), 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nameL = lv_label_create(row);
    lv_label_set_text(nameL, name);
    lv_obj_set_style_text_font(nameL, UI_FONT_BODY, 0);
    lv_obj_set_width(nameL, lv_pct(UI_ROW_LABEL_PCT));

    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(btn, lv_pct(26));
    lv_obj_set_height(btn, 44);
    lv_obj_add_style(btn, &gStyleHomeBtnOff, 0);
    lv_obj_add_style(btn, &gStyleHomeBtnOn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn, onSwitchEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);

    *outOnLbl = lv_label_create(btn);
    lv_label_set_text(*outOnLbl, "OFF");
    lv_obj_set_style_text_font(*outOnLbl, UI_FONT_TITLE, 0);
    lv_obj_center(*outOnLbl);

    if (outPowerLbl != NULL)
    {
        *outPowerLbl = lv_label_create(row);
        lv_label_set_text(*outPowerLbl, "");
        lv_obj_add_style(*outPowerLbl, &gStyleMuted, 0);
        lv_obj_set_style_text_font(*outPowerLbl, UI_FONT_TITLE, 0);
        lv_obj_set_flex_grow(*outPowerLbl, 1);
        lv_obj_set_style_text_align(*outPowerLbl, LV_TEXT_ALIGN_RIGHT, 0);
    }

    return btn;
}

static lv_obj_t *createCard(lv_obj_t *parent, const CHAR *title, INT32 flexGrow)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    if (flexGrow > 0)
    {
        lv_obj_set_flex_grow(card, flexGrow);
        lv_obj_set_height(card, lv_pct(100));
    }
    else
    {
        lv_obj_set_height(card, LV_SIZE_CONTENT);
    }
    lv_obj_add_style(card, &gStyleCard, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 3, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *titleLabel = lv_label_create(card);
    lv_obj_add_style(titleLabel, &gStyleTitle, 0);
    lv_label_set_text(titleLabel, title);
    return card;
}

static VOID setStatusValue(lv_obj_t *label, BOOL isOk, const CHAR *okTxt, const CHAR *badTxt)
{
    if (label == NULL)
        return;

    lv_obj_remove_style(label, &gStyleGood, 0);
    lv_obj_remove_style(label, &gStyleBad, 0);
    lv_obj_add_style(label, isOk ? &gStyleGood : &gStyleBad, 0);
    lv_label_set_text(label, isOk ? okTxt : badTxt);
}

static VOID setTogglePowerLabel(lv_obj_t *lbl, UINT8 isOn, UINT16 wattsW)
{
    if (lbl == NULL)
        return;

    if (isOn != 0U)
        lv_label_set_text_fmt(lbl, "%u W", (unsigned)wattsW);
    else
        lv_label_set_text(lbl, "0 W");
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

    lv_style_init(&gStyleScreen);
    lv_style_set_bg_opa(&gStyleScreen, LV_OPA_100);
    lv_style_set_bg_grad_dir(&gStyleScreen, LV_GRAD_DIR_VER);
    lv_style_set_bg_color(&gStyleScreen, lv_color_hex(0xF5F7FB));
    lv_style_set_bg_grad_color(&gStyleScreen, lv_color_hex(0xE8EEF5));

    lv_style_init(&gStyleCard);
    lv_style_set_bg_opa(&gStyleCard, LV_OPA_100);
    lv_style_set_bg_color(&gStyleCard, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&gStyleCard, 1);
    lv_style_set_border_color(&gStyleCard, lv_color_hex(0xCFD8E3));
    lv_style_set_radius(&gStyleCard, 12);
    lv_style_set_pad_all(&gStyleCard, 6);

    lv_style_init(&gStyleTitle);
    lv_style_set_text_color(&gStyleTitle, lv_color_hex(0x1F2D3D));
    lv_style_set_text_font(&gStyleTitle, UI_FONT_TITLE);

    lv_style_init(&gStyleAppTitle);
    lv_style_set_text_font(&gStyleAppTitle, UI_FONT_APP_TITLE);
    lv_style_set_text_color(&gStyleAppTitle, lv_color_hex(0x1565C0));
    lv_style_set_text_letter_space(&gStyleAppTitle, 2);

    lv_style_init(&gStyleMuted);
    lv_style_set_text_color(&gStyleMuted, lv_color_hex(0x5C6B7A));
    lv_style_set_text_font(&gStyleMuted, UI_FONT_BODY);

    lv_style_init(&gStyleGood);
    lv_style_set_text_color(&gStyleGood, lv_color_hex(0x1A8D50));
    lv_style_set_text_font(&gStyleGood, UI_FONT_BODY);

    lv_style_init(&gStyleBad);
    lv_style_set_text_color(&gStyleBad, lv_color_hex(0xB23A48));
    lv_style_set_text_font(&gStyleBad, UI_FONT_BODY);

    lv_style_init(&gStyleHomeBtnOff);
    lv_style_set_radius(&gStyleHomeBtnOff, 10);
    lv_style_set_border_width(&gStyleHomeBtnOff, 2);
    lv_style_set_border_color(&gStyleHomeBtnOff, lv_color_hex(0xC62828));
    lv_style_set_bg_opa(&gStyleHomeBtnOff, LV_OPA_100);
    lv_style_set_bg_color(&gStyleHomeBtnOff, lv_color_hex(0xFFCDD2));
    lv_style_set_text_color(&gStyleHomeBtnOff, lv_color_hex(0xB71C1C));

    lv_style_init(&gStyleHomeBtnOn);
    lv_style_set_radius(&gStyleHomeBtnOn, 10);
    lv_style_set_border_width(&gStyleHomeBtnOn, 2);
    lv_style_set_border_color(&gStyleHomeBtnOn, lv_color_hex(0x1B5E20));
    lv_style_set_bg_opa(&gStyleHomeBtnOn, LV_OPA_100);
    lv_style_set_bg_color(&gStyleHomeBtnOn, lv_color_hex(0x2E7D32));
    lv_style_set_text_color(&gStyleHomeBtnOn, lv_color_hex(0xFFFFFF));

    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &gStyleScreen, 0);
    lv_obj_set_style_pad_left(scr, 16, 0);
    lv_obj_set_style_pad_right(scr, 16, 0);
    lv_obj_set_style_pad_bottom(scr, 8, 0);
    lv_obj_set_style_pad_top(scr, 22, 0);
    lv_obj_set_style_text_font(scr, UI_FONT_BODY, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_left(root, 6, 0);
    lv_obj_set_style_pad_right(root, 6, 0);
    lv_obj_set_style_pad_bottom(root, 4, 0);
    lv_obj_set_style_pad_top(root, 0, 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(root, 0, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);

    /* Header: title only (per-room Auto/Manual lives inside each room card). */
    lv_obj_t *header = lv_obj_create(root);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    {
        lv_obj_t *title = lv_label_create(header);
        lv_obj_add_style(title, &gStyleAppTitle, 0);
        lv_label_set_text(title, LV_SYMBOL_HOME "  Smart Home");
        lv_obj_set_width(title, lv_pct(100));
        lv_obj_set_style_pad_top(title, 0, 0);
        lv_obj_set_style_pad_bottom(title, 14, 0);
    }

    /* One-line connectivity strip — tight under header */
    gStatusLineLabel = lv_label_create(root);
    lv_obj_add_style(gStatusLineLabel, &gStyleMuted, 0);
    lv_obj_set_style_text_font(gStatusLineLabel, UI_FONT_BODY, 0);
    lv_obj_set_style_pad_top(gStatusLineLabel, 0, 0);
    lv_obj_set_style_pad_bottom(gStatusLineLabel, 14, 0);
    lv_obj_set_style_margin_top(gStatusLineLabel, 0, 0);
    lv_label_set_text(gStatusLineLabel, "MQTT: -- | Modbus: -- | Sensors: --/--");

    /* Room selector */
    lv_obj_t *ddRow = lv_obj_create(root);
    lv_obj_set_width(ddRow, lv_pct(100));
    lv_obj_set_height(ddRow, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ddRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ddRow, 0, 0);
    lv_obj_set_style_pad_top(ddRow, 2, 0);
    lv_obj_set_style_pad_bottom(ddRow, 10, 0);
    lv_obj_clear_flag(ddRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(ddRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ddRow, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ddRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *ddLbl = lv_label_create(ddRow);
    lv_label_set_text(ddLbl, "Room");
    lv_obj_add_style(ddLbl, &gStyleMuted, 0);

    {
        lv_obj_t *roomTabRow = lv_obj_create(ddRow);
        lv_obj_set_width(roomTabRow, lv_pct(100));
        lv_obj_set_height(roomTabRow, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(roomTabRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(roomTabRow, 0, 0);
        lv_obj_set_style_pad_all(roomTabRow, 0, 0);
        lv_obj_set_style_pad_column(roomTabRow, 8, 0);
        lv_obj_set_layout(roomTabRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(roomTabRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(roomTabRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(roomTabRow, LV_OBJ_FLAG_SCROLLABLE);

        gRoomSelBtn1 = lv_btn_create(roomTabRow);
        gRoomSelBtn2 = lv_btn_create(roomTabRow);
        lv_obj_set_flex_grow(gRoomSelBtn1, 1);
        lv_obj_set_flex_grow(gRoomSelBtn2, 1);
        lv_obj_set_height(gRoomSelBtn1, 36);
        lv_obj_set_height(gRoomSelBtn2, 36);
        lv_obj_set_style_radius(gRoomSelBtn1, 10, 0);
        lv_obj_set_style_radius(gRoomSelBtn2, 10, 0);
        lv_obj_set_style_border_width(gRoomSelBtn1, 2, 0);
        lv_obj_set_style_border_width(gRoomSelBtn2, 2, 0);
        lv_obj_set_style_text_font(gRoomSelBtn1, UI_FONT_BODY, 0);
        lv_obj_set_style_text_font(gRoomSelBtn2, UI_FONT_BODY, 0);
        lv_obj_add_event_cb(gRoomSelBtn1, onRoomSelectClick, LV_EVENT_CLICKED, (void *)(intptr_t)0);
        lv_obj_add_event_cb(gRoomSelBtn2, onRoomSelectClick, LV_EVENT_CLICKED, (void *)(intptr_t)1);
        {
            lv_obj_t *l1 = lv_label_create(gRoomSelBtn1);
            lv_label_set_text(l1, "Room 1");
            lv_obj_center(l1);
            lv_obj_t *l2 = lv_label_create(gRoomSelBtn2);
            lv_label_set_text(l2, "Room 2");
            lv_obj_center(l2);
        }
    }

    /* Room panels share the remaining height (no scrolling) */
    gRoom1Panel = createCard(root, "Room 1", 0);
    createRoomModeRow(gRoom1Panel, CTRL_MODE_R1, &gR1ModeSwitch, &gR1ModeLabel, NULL);
    {
        lv_obj_t *pirRow = lv_obj_create(gRoom1Panel);
        lv_obj_set_width(pirRow, lv_pct(100));
        lv_obj_set_height(pirRow, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(pirRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(pirRow, 0, 0);
        lv_obj_set_layout(pirRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(pirRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pirRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(pirRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *pirK = lv_label_create(pirRow);
        lv_obj_add_style(pirK, &gStyleMuted, 0);
        lv_label_set_text(pirK, "Motion");
        lv_obj_set_width(pirK, lv_pct(UI_ROW_LABEL_PCT));
        gPir1ValueLabel = lv_label_create(pirRow);
        lv_label_set_text(gPir1ValueLabel, "--");
        lv_obj_set_flex_grow(gPir1ValueLabel, 1);
        lv_obj_set_style_text_align(gPir1ValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
    }

    gR1LightBtn = createHomeToggleRow(gRoom1Panel, "Light", CTRL_R1_LIGHT, &gR1LightOnLbl, &gR1LightPowerLbl);
    gR1FanBtn = createHomeToggleRow(gRoom1Panel, "Fan", CTRL_R1_FAN, &gR1FanOnLbl, &gR1FanPowerLbl);

    gRoom2Panel = createCard(root, "Room 2", 0);
    createRoomModeRow(gRoom2Panel, CTRL_MODE_R2, &gR2ModeSwitch, &gR2ModeLabel, &gR2FridgePowerLbl);
    {
        lv_obj_t *pirRow = lv_obj_create(gRoom2Panel);
        lv_obj_set_width(pirRow, lv_pct(100));
        lv_obj_set_height(pirRow, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(pirRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(pirRow, 0, 0);
        lv_obj_set_layout(pirRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(pirRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(pirRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(pirRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *pirK = lv_label_create(pirRow);
        lv_obj_add_style(pirK, &gStyleMuted, 0);
        lv_label_set_text(pirK, "Motion");
        lv_obj_set_width(pirK, lv_pct(UI_ROW_LABEL_PCT));
        gPir2ValueLabel = lv_label_create(pirRow);
        lv_label_set_text(gPir2ValueLabel, "--");
        lv_obj_set_flex_grow(gPir2ValueLabel, 1);
        lv_obj_set_style_text_align(gPir2ValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
    }

    gR2LightBtn = createHomeToggleRow(gRoom2Panel, "Light", CTRL_R2_LIGHT, &gR2LightOnLbl, &gR2LightPowerLbl);
    gR2ACBtn = createHomeToggleRow(gRoom2Panel, "AC", CTRL_R2_AC, &gR2ACOnLbl, &gR2ACPowerLbl);

    lv_obj_add_flag(gRoom2Panel, LV_OBJ_FLAG_HIDDEN);
    setRoomTabVisual(gRoomSelBtn1, 1U);
    setRoomTabVisual(gRoomSelBtn2, 0U);
#endif
    memset(&gUiState, 0, sizeof(gUiState));
    memset(&gUiCmd, 0, sizeof(gUiCmd));
    return RET_OK;
}

VOID uiProcess(VOID)
{
#ifdef ENABLE_LVGL
    uint32_t i;
    uint32_t next;

    lv_port_linux_tick();
    /* Drain due timers (incl. indev) when handler returns 0 = call again immediately. */
    for (i = 0U; i < 16U; i++)
    {
        next = lv_timer_handler();
        if (next != 0U)
            break;
    }
#endif
}

VOID uiUpdateState(const UI_STATE *state)
{
    if (state == NULL)
        return;

    gUiState = *state;

#ifdef ENABLE_LVGL
    gUiSyncInProgress = TRUE;
    setSwitchState(gR1ModeSwitch, gUiState.isAutoModeRoom1);
    if (gR1ModeLabel)
        lv_label_set_text(gR1ModeLabel, gUiState.isAutoModeRoom1 ? "Auto" : "Manual");
    setSwitchState(gR2ModeSwitch, gUiState.isAutoModeRoom2);
    if (gR2ModeLabel)
        lv_label_set_text(gR2ModeLabel, gUiState.isAutoModeRoom2 ? "Auto" : "Manual");

    if (gStatusLineLabel)
    {
        const CHAR *mqttTxt = (gUiState.mqttConnected != 0U) ? "OK" : "Down";
        const CHAR *mbTxt = (gUiState.modbusConnectedCount == gUiState.modbusTotalDevices) ? "OK" : "Partial";
        lv_label_set_text_fmt(gStatusLineLabel, "MQTT: %s | Modbus: %s | Sensors: %u/%u",
                              mqttTxt, mbTxt,
                              (unsigned)gUiState.modbusConnectedCount,
                              (unsigned)gUiState.modbusTotalDevices);
    }

    if (gPir1ValueLabel)
        setStatusValue(gPir1ValueLabel, (gUiState.pirRoom1 != 0U), "Yes", "No");
    if (gPir2ValueLabel)
        setStatusValue(gPir2ValueLabel, (gUiState.pirRoom2 != 0U), "Yes", "No");

    setHomeBtnVisual(gR1LightBtn, gR1LightOnLbl, gUiState.room1Light);
    setHomeBtnVisual(gR1FanBtn, gR1FanOnLbl, gUiState.room1Fan);
    setHomeBtnVisual(gR2LightBtn, gR2LightOnLbl, gUiState.room2Light);
    setHomeBtnVisual(gR2ACBtn, gR2ACOnLbl, gUiState.room2AC);

    setTogglePowerLabel(gR1LightPowerLbl, gUiState.room1Light, gUiState.room1LightPowerW);
    setTogglePowerLabel(gR1FanPowerLbl, gUiState.room1Fan, gUiState.room1FanPowerW);
    setTogglePowerLabel(gR2LightPowerLbl, gUiState.room2Light, gUiState.room2LightPowerW);
    setTogglePowerLabel(gR2ACPowerLbl, gUiState.room2AC, gUiState.room2ACPowerW);

    if (gR2FridgePowerLbl)
        lv_label_set_text_fmt(gR2FridgePowerLbl, "Fridge %u W", (unsigned)gUiState.room2FridgePowerW);

    if (gUiState.isAutoModeRoom1)
    {
        lv_obj_add_state(gR1LightBtn, LV_STATE_DISABLED);
        lv_obj_add_state(gR1FanBtn, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_clear_state(gR1LightBtn, LV_STATE_DISABLED);
        lv_obj_clear_state(gR1FanBtn, LV_STATE_DISABLED);
    }

    if (gUiState.isAutoModeRoom2)
    {
        lv_obj_add_state(gR2LightBtn, LV_STATE_DISABLED);
        lv_obj_add_state(gR2ACBtn, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_clear_state(gR2LightBtn, LV_STATE_DISABLED);
        lv_obj_clear_state(gR2ACBtn, LV_STATE_DISABLED);
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

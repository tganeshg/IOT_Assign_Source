#include "lvgl_port_linux.h"

#ifdef ENABLE_LVGL

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <lvgl.h>

/* LVGL 9 removed lv_disp_drv_t / lv_indev_drv_t; use lv_display_t / lv_indev_t instead. */
#if defined(LVGL_VERSION_MAJOR) && (LVGL_VERSION_MAJOR >= 9)
#define LVGL_PORT_USE_V9 1
#else
#define LVGL_PORT_USE_V9 0
#endif

static int s_fbFd = -1;
static char *s_fbMem = NULL;
static uint32_t s_fbLineLen = 0;
static struct fb_var_screeninfo s_vinfo;
static struct fb_fix_screeninfo s_finfo;

static int s_touchFd = -1;
static int32_t s_touchX = 0;
static int32_t s_touchY = 0;
static int32_t s_touchXMin = 0;
static int32_t s_touchXMax = 4095;
static int32_t s_touchYMin = 0;
static int32_t s_touchYMax = 4095;
static UINT8 s_touchPressed = 0U;

static uint32_t s_lastTickMs = 0U;

#if LVGL_PORT_USE_V9
static lv_display_t *s_disp = NULL;
static lv_indev_t *s_indev = NULL;
#endif

static uint32_t monotonicMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((UINT64)ts.tv_sec * 1000ULL + (UINT64)ts.tv_nsec / 1000000ULL);
}

static void fb_write_pixel(int32_t x, int32_t y, uint16_t rgb565)
{
    long int loc = 0;

    loc = (x + (int32_t)s_vinfo.xoffset) * ((int32_t)s_vinfo.bits_per_pixel / 8) +
          (y + (int32_t)s_vinfo.yoffset) * (long int)s_fbLineLen;

    if (s_vinfo.bits_per_pixel == 16)
    {
        *((uint16_t *)(s_fbMem + loc)) = rgb565;
    }
    else if (s_vinfo.bits_per_pixel == 32)
    {
        uint32_t r = (uint32_t)((rgb565 >> 11) & 0x1FU);
        uint32_t g = (uint32_t)((rgb565 >> 5) & 0x3FU);
        uint32_t b = (uint32_t)(rgb565 & 0x1FU);
        r = (r * 255U) / 31U;
        g = (g * 255U) / 63U;
        b = (b * 255U) / 31U;
        *((uint32_t *)(s_fbMem + loc)) = (r << 16) | (g << 8) | b;
    }
    else
    {
        *((uint16_t *)(s_fbMem + loc)) = rgb565;
    }
}

static void fb_write_pixel_u32(int32_t x, int32_t y, uint32_t argb_or_xrgb)
{
    long int loc = 0;

    loc = (x + (int32_t)s_vinfo.xoffset) * ((int32_t)s_vinfo.bits_per_pixel / 8) +
          (y + (int32_t)s_vinfo.yoffset) * (long int)s_fbLineLen;

    if (s_vinfo.bits_per_pixel == 32)
    {
        *((uint32_t *)(s_fbMem + loc)) = argb_or_xrgb;
    }
    else if (s_vinfo.bits_per_pixel == 16)
    {
        uint32_t r = (argb_or_xrgb >> 16) & 0xFFU;
        uint32_t g = (argb_or_xrgb >> 8) & 0xFFU;
        uint32_t b = argb_or_xrgb & 0xFFU;
        uint16_t rgb565 = (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
        *((uint16_t *)(s_fbMem + loc)) = rgb565;
    }
}

#if LVGL_PORT_USE_V9

static void fbdev_flush_v9(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lv_color_format_t cf = lv_display_get_color_format(disp);
    uint32_t w = (uint32_t)lv_area_get_width(area);
    uint32_t h = (uint32_t)lv_area_get_height(area);
    uint32_t stride = lv_draw_buf_width_to_stride(w, cf);
    uint32_t bpp = (uint32_t)lv_color_format_get_bpp(cf);
    uint32_t x = 0U;
    uint32_t y = 0U;

    for (y = 0U; y < h; y++)
    {
        for (x = 0U; x < w; x++)
        {
            uint32_t src_off = y * stride + x * (bpp / 8U);
            int32_t dx = area->x1 + (int32_t)x;
            int32_t dy = area->y1 + (int32_t)y;

            if (cf == LV_COLOR_FORMAT_RGB565)
            {
                uint16_t px = *(uint16_t *)(px_map + src_off);
                fb_write_pixel(dx, dy, px);
            }
            else if (cf == LV_COLOR_FORMAT_RGB888)
            {
                uint8_t *p = px_map + src_off;
                uint32_t r = p[0];
                uint32_t g = p[1];
                uint32_t b = p[2];
                uint32_t v = (r << 16) | (g << 8) | b;
                fb_write_pixel_u32(dx, dy, v);
            }
            else if (cf == LV_COLOR_FORMAT_XRGB8888 || cf == LV_COLOR_FORMAT_ARGB8888)
            {
                uint32_t v = *(uint32_t *)(px_map + src_off);
                fb_write_pixel_u32(dx, dy, v);
            }
            else
            {
                uint16_t px = *(uint16_t *)(px_map + src_off);
                fb_write_pixel(dx, dy, px);
            }
        }
    }

    lv_display_flush_ready(disp);
}

#else

static void fbdev_flush_v8(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    int32_t x = 0;
    int32_t y = 0;

    (void)disp_drv;

    for (y = area->y1; y <= area->y2; y++)
    {
        for (x = area->x1; x <= area->x2; x++)
        {
            if (s_vinfo.bits_per_pixel == 16)
            {
                *((uint16_t *)(s_fbMem + ((x + s_vinfo.xoffset) * (s_vinfo.bits_per_pixel / 8) +
                                          (y + s_vinfo.yoffset) * (long int)s_fbLineLen))) =
                    lv_color_to_u16(*color_p);
            }
            else if (s_vinfo.bits_per_pixel == 32)
            {
                lv_color32_t c32 = lv_color_to_32(*color_p);
                uint32_t v = ((uint32_t)c32.ch.red << 16) | ((uint32_t)c32.ch.green << 8) | (uint32_t)c32.ch.blue;
                *((uint32_t *)(s_fbMem + ((x + s_vinfo.xoffset) * (s_vinfo.bits_per_pixel / 8) +
                                          (y + s_vinfo.yoffset) * (long int)s_fbLineLen))) = v;
            }
            else
            {
                *((uint16_t *)(s_fbMem + ((x + s_vinfo.xoffset) * (s_vinfo.bits_per_pixel / 8) +
                                          (y + s_vinfo.yoffset) * (long int)s_fbLineLen))) =
                    lv_color_to_u16(*color_p);
            }
            color_p++;
        }
    }

    lv_disp_flush_ready(disp_drv);
}

#endif

static void touch_poll_events(VOID)
{
    struct input_event ev;
    ssize_t n = 0;

    while ((n = read(s_touchFd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev))
    {
        if (ev.type == EV_ABS)
        {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X)
            {
                s_touchX = ev.value;
            }
            else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y)
            {
                s_touchY = ev.value;
            }
        }
        else if (ev.type == EV_KEY)
        {
            if (ev.code == BTN_TOUCH || ev.code == BTN_LEFT)
            {
                s_touchPressed = (ev.value != 0) ? 1U : 0U;
            }
        }
    }
}

static void touch_fill_pointer(lv_indev_data_t *data)
{
    if (s_touchXMax > s_touchXMin)
    {
        data->point.x = (lv_coord_t)((int32_t)s_vinfo.xres * (s_touchX - s_touchXMin)) / (s_touchXMax - s_touchXMin);
    }
    else
    {
        data->point.x = (lv_coord_t)s_touchX;
    }

    if (s_touchYMax > s_touchYMin)
    {
        data->point.y = (lv_coord_t)((int32_t)s_vinfo.yres * (s_touchY - s_touchYMin)) / (s_touchYMax - s_touchYMin);
    }
    else
    {
        data->point.y = (lv_coord_t)s_touchY;
    }

    if (data->point.x < 0)
    {
        data->point.x = 0;
    }
    if (data->point.y < 0)
    {
        data->point.y = 0;
    }
    if (data->point.x >= (lv_coord_t)s_vinfo.xres)
    {
        data->point.x = (lv_coord_t)s_vinfo.xres - 1;
    }
    if (data->point.y >= (lv_coord_t)s_vinfo.yres)
    {
        data->point.y = (lv_coord_t)s_vinfo.yres - 1;
    }

    data->state = (s_touchPressed != 0U) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#if LVGL_PORT_USE_V9

static void touch_read_v9(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    touch_poll_events();
    touch_fill_pointer(data);
}

#else

static void touch_read_v8(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    touch_poll_events();
    touch_fill_pointer(data);
}

#endif

ERROR_CODE lv_port_linux_init(const CHAR *fbdevPath, const CHAR *touchDevPath)
{
    lv_coord_t buf_lines = 40;
    uint32_t buf_pixels = 0U;

#if LVGL_PORT_USE_V9
    uint8_t *buf1 = NULL;
    uint32_t buf_bytes = 0U;
#else
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;
    lv_color_t *buf1 = NULL;
#endif

    if ((fbdevPath == NULL) || (touchDevPath == NULL))
    {
        fprintf(stderr, "LVGL: fbdev/touch path is NULL\n");
        return RET_FAILURE;
    }

    s_fbFd = open(fbdevPath, O_RDWR);
    if (s_fbFd < 0)
    {
        fprintf(stderr, "LVGL: cannot open framebuffer %s: %s\n", fbdevPath, strerror(errno));
        return RET_FAILURE;
    }

    if (ioctl(s_fbFd, FBIOGET_FSCREENINFO, &s_finfo) < 0)
    {
        fprintf(stderr, "LVGL: FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    if (ioctl(s_fbFd, FBIOGET_VSCREENINFO, &s_vinfo) < 0)
    {
        fprintf(stderr, "LVGL: FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    s_fbLineLen = s_finfo.line_length;
    s_fbMem = (char *)mmap(0, s_finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, s_fbFd, 0);
    if (s_fbMem == MAP_FAILED)
    {
        fprintf(stderr, "LVGL: framebuffer mmap failed: %s\n", strerror(errno));
        close(s_fbFd);
        s_fbFd = -1;
        s_fbMem = NULL;
        return RET_FAILURE;
    }

    buf_pixels = (uint32_t)s_vinfo.xres * (uint32_t)buf_lines;

#if LVGL_PORT_USE_V9
    s_disp = lv_display_create((int32_t)s_vinfo.xres, (int32_t)s_vinfo.yres);
    if (s_disp == NULL)
    {
        fprintf(stderr, "LVGL: lv_display_create failed\n");
        munmap(s_fbMem, s_finfo.smem_len);
        s_fbMem = NULL;
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    if (s_vinfo.bits_per_pixel == 16)
    {
        lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    }
    else if (s_vinfo.bits_per_pixel == 32)
    {
        lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_XRGB8888);
    }
    else
    {
        lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    }

    {
        uint32_t bppcf = (uint32_t)lv_color_format_get_bpp(lv_display_get_color_format(s_disp));
        buf_bytes = buf_pixels * ((bppcf + 7U) / 8U);
    }
    buf1 = (uint8_t *)malloc(buf_bytes);
    if (buf1 == NULL)
    {
        fprintf(stderr, "LVGL: draw buffer alloc failed\n");
        lv_display_delete(s_disp);
        s_disp = NULL;
        munmap(s_fbMem, s_finfo.smem_len);
        s_fbMem = NULL;
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    lv_display_set_buffers(s_disp, buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, fbdev_flush_v9);
#else
    buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_pixels);
    if (buf1 == NULL)
    {
        fprintf(stderr, "LVGL: draw buffer alloc failed\n");
        munmap(s_fbMem, s_finfo.smem_len);
        s_fbMem = NULL;
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, (uint32_t)(buf_pixels));

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = (lv_coord_t)s_vinfo.xres;
    disp_drv.ver_res = (lv_coord_t)s_vinfo.yres;
    disp_drv.flush_cb = fbdev_flush_v8;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
#endif

    s_touchFd = open(touchDevPath, O_RDONLY | O_NONBLOCK);
    if (s_touchFd < 0)
    {
        fprintf(stderr, "LVGL: cannot open touch device %s: %s\n", touchDevPath, strerror(errno));
#if LVGL_PORT_USE_V9
        free(buf1);
        lv_display_delete(s_disp);
        s_disp = NULL;
#else
        free(buf1);
#endif
        munmap(s_fbMem, s_finfo.smem_len);
        s_fbMem = NULL;
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }

    {
        struct input_absinfo absx;
        struct input_absinfo absy;
        if (ioctl(s_touchFd, EVIOCGABS(ABS_X), &absx) == 0)
        {
            s_touchXMin = absx.minimum;
            s_touchXMax = absx.maximum;
        }
        if (ioctl(s_touchFd, EVIOCGABS(ABS_Y), &absy) == 0)
        {
            s_touchYMin = absy.minimum;
            s_touchYMax = absy.maximum;
        }
    }

#if LVGL_PORT_USE_V9
    s_indev = lv_indev_create();
    if (s_indev == NULL)
    {
        fprintf(stderr, "LVGL: lv_indev_create failed\n");
        free(buf1);
        close(s_touchFd);
        s_touchFd = -1;
        lv_display_delete(s_disp);
        s_disp = NULL;
        munmap(s_fbMem, s_finfo.smem_len);
        s_fbMem = NULL;
        close(s_fbFd);
        s_fbFd = -1;
        return RET_FAILURE;
    }
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_indev, s_disp);
    lv_indev_set_read_cb(s_indev, touch_read_v9);
#else
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_v8;
    lv_indev_drv_register(&indev_drv);
#endif

    s_lastTickMs = monotonicMs();
    return RET_OK;
}

VOID lv_port_linux_tick(VOID)
{
    uint32_t now = monotonicMs();
    uint32_t elapsed = 0U;

    if (s_lastTickMs == 0U)
    {
        s_lastTickMs = now;
        return;
    }

    elapsed = now - s_lastTickMs;
    if (elapsed > 1000U)
    {
        elapsed = 1000U;
    }
    lv_tick_inc(elapsed);
    s_lastTickMs = now;
}

#else

ERROR_CODE lv_port_linux_init(const CHAR *fbdevPath, const CHAR *touchDevPath)
{
    (void)fbdevPath;
    (void)touchDevPath;
    return RET_OK;
}

VOID lv_port_linux_tick(VOID)
{
}

#endif

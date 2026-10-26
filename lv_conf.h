/**
 * @file lv_conf.h
 * @brief LVGL配置文件
 * 基于LVGL v8.x, 为Windows SDL2后端调整
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0
#define LV_COLOR_SCREEN_TRANSP  0

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE             (256U * 1024U)
#define LV_MEM_CUSTOM           0
#define LV_MEMCPY_MEMSET_STD    1

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM          0
#define LV_USE_PERF_MONITOR     0

/*====================
   DRAW SETTINGS
 *====================*/
#define LV_DRAW_COMPLEX         1
#define LV_SHADOW_CACHE_SIZE    0
#define LV_IMG_CACHE_DEF_SIZE   0

/*====================
   GPU
 *====================*/
#define LV_USE_GPU              0

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG              0

/*====================
   OTHER
 *====================*/
#define LV_USE_ASSERT_NULL      0
#define LV_USE_ASSERT_MALLOC    0

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0

/*====================
   WIDGET USAGE
 *====================*/
#define LV_USE_ARC              0
#define LV_USE_BAR              0
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        0
#define LV_USE_CANVAS           0
#define LV_USE_CHECKBOX         0
#define LV_USE_DROPDOWN         0
#define LV_USE_IMG              0
#define LV_USE_LABEL            1
#define LV_USE_LINE             0
#define LV_USE_ROLLER           0
#define LV_USE_SLIDER           0
#define LV_USE_SWITCH           0
#define LV_USE_TEXTAREA         0
#define LV_USE_TABLE            0
#define LV_USE_WIN              0

/*====================
   THEME
 *====================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_GROW   1

/*====================
   INPUT DEVICES
 *====================*/
#define LV_INDEV_DEF_READ_PERIOD    30
#define LV_INDEV_DEF_SCROLL_THROW   0

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX             1
#define LV_USE_GRID             0

#endif /* LV_CONF_H */

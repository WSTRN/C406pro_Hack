#pragma once

#include <lvgl.h>

#ifdef __cplusplus
#include "PageManager.h"
#endif

typedef enum {
	PAGE_NONE,
	PAGE_Info,
	PAGE_MAX,
} Page_Type;

#ifdef __cplusplus
extern PageManager page;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void ui_init();
void Pages_Init();
void AppWindow_Create();
lv_obj_t *AppWindow_GetCont(uint8_t pageID);
lv_coord_t AppWindow_GetHeight();
lv_coord_t AppWindow_GetWidth();

#ifdef __cplusplus
}
#endif

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>

#include "info_page.h"
#include "main_page.h"
#include "pages.h"
#include "status_bar.h"
#define PAGES_THREAD_STACK_SIZE 2048
#define PAGES_THREAD_PRIORITY 5
#define PAGE_WIDTH 128
#define PAGE_HEIGHT 144

K_THREAD_STACK_DEFINE(pages_thread_stack, PAGES_THREAD_STACK_SIZE);
static struct k_thread pages_thread;
static k_tid_t pages_thread_id;
static struct k_mutex pages_mutex;
PageManager page(PAGE_MAX);
static lv_obj_t *page_windows[PAGE_MAX];

extern const struct device *display_dev;

lv_obj_t *AppWindow_GetCont(uint8_t pageID)
{
	return pageID < PAGE_MAX ? page_windows[pageID] : NULL;
}

lv_coord_t AppWindow_GetHeight()
{
	return PAGE_HEIGHT;
}

lv_coord_t AppWindow_GetWidth()
{
	return PAGE_WIDTH;
}

void AppWindow_Create()
{
	for (uint8_t page_id = 0; page_id < PAGE_MAX; page_id++) {
		lv_obj_t *window = lv_obj_create(lv_scr_act());

		lv_obj_set_size(window, AppWindow_GetWidth(), AppWindow_GetHeight());
		lv_obj_align(window, LV_ALIGN_BOTTOM_MID, 0, 0);
		lv_obj_set_style_bg_color(window, lv_color_black(), LV_PART_MAIN);
		lv_obj_set_style_border_width(window, 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(window, 0, LV_PART_MAIN);
		lv_obj_clear_flag(window, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_add_flag(window, LV_OBJ_FLAG_HIDDEN);

		page_windows[page_id] = window;
	}
}

#define PAGE_REG(name)		       \
	do {			       \
		PageRegister_##name(PAGE_##name); \
	} while (0)

void Pages_Init()
{
	PAGE_REG(Main);
	PAGE_REG(Info);
	page.PagePush(PAGE_Main);
}

static void pages_thread_entry(void *, void *, void *)
{
	k_mutex_lock(&pages_mutex, K_FOREVER);
	AppWindow_Create();
	StatusBar_Init();
	Pages_Init();
	k_mutex_unlock(&pages_mutex);

	for (;;) {
		k_mutex_lock(&pages_mutex, K_FOREVER);
		page.Running();
		static uint32_t last_status_update;
		uint32_t now = k_uptime_get_32();

		if (now - last_status_update >= 500U) {
			StatusBar_Refresh();
			last_status_update = now;
		}
		lv_task_handler();
		k_mutex_unlock(&pages_mutex);
		k_msleep(20);
	}
}

void ui_init()
{
	k_mutex_init(&pages_mutex);
	pages_thread_id = k_thread_create(&pages_thread, pages_thread_stack,
				   K_THREAD_STACK_SIZEOF(pages_thread_stack),
				   pages_thread_entry,
				   NULL, NULL, NULL,
				   PAGES_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(pages_thread_id, "pages_thread");
	display_blanking_off(display_dev);
}

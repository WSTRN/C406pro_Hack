#include <lvgl.h>
#include <zephyr/sys/util.h>

#include "main_page.h"
#include "pages.h"

static lv_obj_t *main_window;

static void setup_main_page(int arg)
{
	lv_obj_t *label;

	ARG_UNUSED(arg);

	lv_obj_move_foreground(main_window);
	lv_obj_clear_flag(main_window, LV_OBJ_FLAG_HIDDEN);

	label = lv_label_create(main_window);
	lv_label_set_text(label, "Main Page");
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

static void loop_main_page(int arg)
{
	ARG_UNUSED(arg);
}

static void teardown_main_page(int arg)
{
	ARG_UNUSED(arg);

	lv_obj_clean(main_window);
	lv_obj_add_flag(main_window, LV_OBJ_FLAG_HIDDEN);
}

void PageRegister_Main(uint8_t pageID)
{
	main_window = AppWindow_GetCont(pageID);
	page.PageRegister(pageID, setup_main_page, loop_main_page,
			  teardown_main_page, NULL);
}

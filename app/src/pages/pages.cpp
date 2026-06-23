#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>

#include "info_page.h"
#include "pages.h"
#include "PageManager.h"

#define PAGES_THREAD_STACK_SIZE 2048
#define PAGES_THREAD_PRIORITY 5

enum {
	PAGE_ID_INFO,
	PAGE_ID_COUNT,
};

K_THREAD_STACK_DEFINE(pages_thread_stack, PAGES_THREAD_STACK_SIZE);
static struct k_thread pages_thread;
static k_tid_t pages_thread_id;
static struct k_mutex pages_mutex;
static PageManager page_manager(PAGE_ID_COUNT, 4);

extern const struct device *display_dev;

static void register_pages()
{
	info_page_register(&page_manager, PAGE_ID_INFO);
}

static void pages_thread_entry(void *, void *, void *)
{
	k_mutex_lock(&pages_mutex, K_FOREVER);
	register_pages();
	page_manager.Running();
	k_mutex_unlock(&pages_mutex);

	for (;;) {
		k_mutex_lock(&pages_mutex, K_FOREVER);
		page_manager.Running();
		lv_task_handler();
		k_mutex_unlock(&pages_mutex);
		k_msleep(20);
	}
}

void page_init()
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

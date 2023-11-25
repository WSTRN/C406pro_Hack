#include "button_event.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(button, LOG_LEVEL_INF);

#define BTN_STACK_SIZE 512
#define BTN_PRIORITY 4
K_THREAD_STACK_DEFINE(btn_stack_area, BTN_STACK_SIZE);
struct k_thread btn_thread_data;
k_tid_t btn_tid;

const struct gpio_dt_spec dev_btn0 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
const struct gpio_dt_spec dev_btn1 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);
const struct gpio_dt_spec dev_btn2 = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios);
extern struct device* ext_power;
extern struct device* display_dev;
extern struct device* pressure_dev;
extern struct device *gpio_0;
extern struct device *gpio_1;

class ButtonEvent btn0;
class ButtonEvent btn1;
class ButtonEvent btn2;

static void btn0_EventHandler(ButtonEvent* btn, int event)
{
    if(event == ButtonEvent::EVENT_ButtonLongPressed)
    {
        LOG_INF("btn0 longpressed");
        gpio_pin_interrupt_configure_dt(&dev_btn0,  GPIO_INT_LEVEL_ACTIVE);
        pm_device_action_run(display_dev, PM_DEVICE_ACTION_SUSPEND);
        pm_device_action_run(pressure_dev, PM_DEVICE_ACTION_SUSPEND);
        pm_device_action_run(ext_power, PM_DEVICE_ACTION_SUSPEND);
        sys_poweroff();
    }
    else if(event == ButtonEvent::EVENT_ButtonClick)
        LOG_INF("btn0 press");
        
}
static void btn1_EventHandler(ButtonEvent* btn, int event)
{
    if(event == ButtonEvent::EVENT_ButtonClick)
        LOG_INF("btn1 press");
}
static void btn2_EventHandler(ButtonEvent* btn, int event)
{
    int ret;
    if(event == ButtonEvent::EVENT_ButtonDoubleClick)
        LOG_INF("btn2 press");
    if(event == ButtonEvent::EVENT_ButtonLongPressed)
    {
        LOG_INF("btn2 longpressed");
        ret = gpio_pin_toggle(gpio_1, 10);
        LOG_INF("%d", ret);
    }
}

void Button_Update()
{
    btn0.EventMonitor(gpio_pin_get(dev_btn0.port, dev_btn0.pin));
    btn1.EventMonitor(gpio_pin_get(dev_btn1.port, dev_btn1.pin));
    btn2.EventMonitor(gpio_pin_get(dev_btn2.port, dev_btn2.pin));
}
void ButtonEventTask(void *, void *, void *)
{
    for(;;)
    {
        Button_Update();
        k_msleep(10);
    }
}

extern "C" void ButtonEvent_Init(void)
{
    gpio_pin_configure_dt(&dev_btn0, GPIO_INPUT);
    gpio_pin_configure_dt(&dev_btn1, GPIO_INPUT);
    gpio_pin_configure_dt(&dev_btn2, GPIO_INPUT);
    btn0.EventAttach(btn0_EventHandler);
    btn1.EventAttach(btn1_EventHandler);
    btn2.EventAttach(btn2_EventHandler);
    btn_tid = k_thread_create(  &btn_thread_data,
                                btn_stack_area,
                                K_THREAD_STACK_SIZEOF(btn_stack_area),
                                ButtonEventTask,
                                NULL, NULL, NULL,
                                BTN_PRIORITY, 0, K_NO_WAIT);
}
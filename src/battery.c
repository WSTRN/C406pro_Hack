#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),
/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

int battery_sensor_init()
{
    int err;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};
	if (!adc_is_ready_dt(&adc_channels[0])) {
		printk("ADC controller device %s not ready\n", adc_channels[0].dev->name);
		return 0;
	}
	err = adc_channel_setup_dt(&adc_channels[0]);
	if (err < 0) {
		printk("Could not setup channel #%d (%d)\n", 0, err);
		return 0;
	}
	printk("ADC reading[AIN5]:\n");
	printk("- %s, channel %d: ",
		       adc_channels[0].dev->name,
		       adc_channels[0].channel_id);
	int32_t val_mv;
	(void)adc_sequence_init_dt(&adc_channels[0], &sequence);

	err = adc_read_dt(&adc_channels[0], &sequence);
	if (err < 0) {
		printk("Could not read (%d)\n", err);
		return 0;
	}
	val_mv = (int32_t)buf;
	printk("%"PRId32, val_mv);
	err = adc_raw_to_millivolts_dt(&adc_channels[0],
				       &val_mv);
	/* conversion to mV may not be supported, skip if not */
	if (err < 0) {
		printk(" (value in mV not available)\n");
	} else {
		printk(" = %"PRId32" mV\n", 4*val_mv);
	}
	printk("\r\n");
    return 0;
}

int32_t battery_get_mv()
{
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};
	int32_t val_mv;
	(void)adc_sequence_init_dt(&adc_channels[0], &sequence);
	adc_read_dt(&adc_channels[0], &sequence);
	val_mv = (int32_t)buf;
	adc_raw_to_millivolts_dt(&adc_channels[0],
				       &val_mv);
	return 4*val_mv;
}
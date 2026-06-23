#ifndef GNSS_H
#define GNSS_H

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/drivers/gnss.h>

#define GNSS_SATELLITE_CACHE_SIZE 48U

void gnss_init(void);
bool gnss_get_info(struct gnss_data *data);
size_t gnss_get_satellites(struct gnss_satellite *satellites, size_t capacity);

#endif /* GNSS_H */

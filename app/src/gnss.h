#ifndef GNSS_H
#define GNSS_H

#include <stdbool.h>
#include <stdint.h>

struct gnss_info {
	bool has_fix;
	int32_t lat_e6;
	int32_t lon_e6;
	uint8_t quality;
	uint8_t satellites;
	uint16_t hdop_x10;
	uint16_t speed_kmh_x10;
	uint16_t course_deg_x10;
};

void gnss_init(void);
void gnss_get_info(struct gnss_info *info);

#endif // GNSS_H

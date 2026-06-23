#include <stdint.h>

#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include "gnss.h"

static const char *fix_status_str(enum gnss_fix_status status)
{
    switch (status)
    {
    case GNSS_FIX_STATUS_NO_FIX:
        return "NO_FIX";
    case GNSS_FIX_STATUS_GNSS_FIX:
        return "GNSS_FIX";
    case GNSS_FIX_STATUS_DGNSS_FIX:
        return "DGNSS_FIX";
    case GNSS_FIX_STATUS_ESTIMATED_FIX:
        return "ESTIMATED_FIX";
    default:
        return "UNKNOWN";
    }
}

static const char *fix_quality_str(enum gnss_fix_quality quality)
{
    switch (quality)
    {
    case GNSS_FIX_QUALITY_INVALID:
        return "INVALID";
    case GNSS_FIX_QUALITY_GNSS_SPS:
        return "GNSS_SPS";
    case GNSS_FIX_QUALITY_DGNSS:
        return "DGNSS";
    case GNSS_FIX_QUALITY_GNSS_PPS:
        return "GNSS_PPS";
    case GNSS_FIX_QUALITY_RTK:
        return "RTK";
    case GNSS_FIX_QUALITY_FLOAT_RTK:
        return "FLOAT_RTK";
    case GNSS_FIX_QUALITY_ESTIMATED:
        return "ESTIMATED";
    default:
        return "UNKNOWN";
    }
}

static const char *system_str(enum gnss_system system)
{
    switch (system)
    {
    case GNSS_SYSTEM_GPS:
        return "GPS";
    case GNSS_SYSTEM_GLONASS:
        return "GLONASS";
    case GNSS_SYSTEM_GALILEO:
        return "GALILEO";
    case GNSS_SYSTEM_BEIDOU:
        return "BEIDOU";
    case GNSS_SYSTEM_QZSS:
        return "QZSS";
    case GNSS_SYSTEM_IRNSS:
        return "IRNSS";
    case GNSS_SYSTEM_SBAS:
        return "SBAS";
    case GNSS_SYSTEM_IMES:
        return "IMES";
    default:
        return "UNKNOWN";
    }
}

static uint64_t abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static int cmd_gnss_data(const struct shell *sh, size_t argc, char **argv)
{
    struct gnss_data data;
    uint64_t latitude;
    uint64_t longitude;
    uint64_t altitude;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!gnss_get_info(&data))
    {
        shell_warn(sh, "No GNSS data received yet");
        return 0;
    }

    latitude = abs_i64(data.nav_data.latitude);
    longitude = abs_i64(data.nav_data.longitude);
    altitude = abs_i64(data.nav_data.altitude);

    shell_print(sh, "Fix status: %s", fix_status_str(data.info.fix_status));
    shell_print(sh, "Fix quality: %s", fix_quality_str(data.info.fix_quality));
    shell_print(sh, "Latitude: %llu.%09llu%s deg", latitude / 1000000000ULL, latitude % 1000000000ULL,
                data.nav_data.latitude < 0 ? "S" : "N");
    shell_print(sh, "Longitude: %llu.%09llu%s deg", longitude / 1000000000ULL, longitude % 1000000000ULL,
                data.nav_data.longitude < 0 ? "W" : "E");
    shell_print(sh, "Altitude: %s%llu.%03llu m", data.nav_data.altitude < 0 ? "-" : "", altitude / 1000ULL,
                altitude % 1000ULL);
    shell_print(sh, "Speed: %u.%03u m/s", data.nav_data.speed / 1000U, data.nav_data.speed % 1000U);
    shell_print(sh, "Bearing: %u.%03u deg", data.nav_data.bearing / 1000U, data.nav_data.bearing % 1000U);
    shell_print(sh, "Satellites tracked: %u", data.info.satellites_cnt);
    shell_print(sh, "HDOP: %u.%03u", data.info.hdop / 1000U, data.info.hdop % 1000U);
    shell_print(sh, "UTC: %02u-%02u-%02u %02u:%02u:%02u.%03u", data.utc.century_year, data.utc.month,
                data.utc.month_day, data.utc.hour, data.utc.minute, data.utc.millisecond / 1000U,
                data.utc.millisecond % 1000U);

    return 0;
}

static int cmd_gnss_satellites(const struct shell *sh, size_t argc, char **argv)
{
    struct gnss_satellite satellites[GNSS_SATELLITE_CACHE_SIZE];
    size_t satellites_count;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    satellites_count = gnss_get_satellites(satellites, ARRAY_SIZE(satellites));
    if (satellites_count == 0U)
    {
        shell_warn(sh, "No satellite data received yet");
        return 0;
    }

    shell_print(sh, "Satellite details (%u):", (uint32_t)satellites_count);
    for (size_t i = 0; i < satellites_count; i++)
    {
        const struct gnss_satellite *sat = &satellites[i];

        shell_print(sh, "  PRN:%u SYS:%s SNR:%u dB EL:%u deg AZ:%u deg SIGNAL:%s", sat->prn, system_str(sat->system),
                    sat->snr, sat->elevation, sat->azimuth, sat->is_tracked ? "yes" : "no");
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss, SHELL_CMD(data, NULL, "Show GNSS position data", cmd_gnss_data),
                               SHELL_CMD(satellites, NULL, "Show each visible satellite", cmd_gnss_satellites),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(gnss, &sub_gnss, "GNSS information", NULL);

/*
 * Copyright (c) 2023 Taisheng WANG <wstrn66@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ST75256_DISPLAY_DRIVER_H__
#define ST75256_DISPLAY_DRIVER_H__

#include <zephyr/kernel.h>

#define ST75256_CMD_00_COMMANDS        0x30
#define ST75256_CMD_01_COMMANDS        0x31

#define ST75256_CMD_SLEEP_OUT          0x94
#define ST75256_CMD_SLEEP_IN           0x95
#define ST75256_CMD_DISPLAY_OFF        0xAE
#define ST75256_CMD_DISPLAY_ON         0xAF

#define ST75256_CMD_AUTO_READ          0XD7
// #define ST75256_DAT_AUTO_READ_DISABLE  0X9F

#define ST75256_CMD_ANALOG_CIRCUIT     0X32
// #define ST75256_DAT_ANALOG_CIRCUIT_1   0X00
// #define ST75256_DAT_ANALOG_CIRCUIT_2   0X01
// #define ST75256_DAT_ANALOG_CIRCUIT_3   0X00

#define ST75256_CMD_GRAY_LEVEL         0X20
// #define ST75256_DAT_GRAY_LEVEL_1       0X01
// #define ST75256_DAT_GRAY_LEVEL_2       0X03
// #define ST75256_DAT_GRAY_LEVEL_3       0X05
// #define ST75256_DAT_GRAY_LEVEL_4       0X07
// #define ST75256_DAT_GRAY_LEVEL_5       0X09
// #define ST75256_DAT_GRAY_LEVEL_6       0X0B
// #define ST75256_DAT_GRAY_LEVEL_7       0X0D
// #define ST75256_DAT_GRAY_LEVEL_8       0X10
// #define ST75256_DAT_GRAY_LEVEL_9       0X11
// #define ST75256_DAT_GRAY_LEVEL_10      0X13
// #define ST75256_DAT_GRAY_LEVEL_11      0X15
// #define ST75256_DAT_GRAY_LEVEL_12      0X17
// #define ST75256_DAT_GRAY_LEVEL_13      0X19
// #define ST75256_DAT_GRAY_LEVEL_14      0X1B
// #define ST75256_DAT_GRAY_LEVEL_15      0X1D
// #define ST75256_DAT_GRAY_LEVEL_16      0X1F

#define ST75256_CMD_ROW_RANGE          0X75
#define ST75256_CMD_COL_RANGE          0X15

#define ST75256_CMD_DATA_SCAN          0XBC
// #define ST75256_DAT_DATA_SCAN          0X02

#define ST75256_CMD_DATA_FORMAT_LSB    0X0C

#define ST75256_CMD_DISPLAY_CONTROL    0XCA
// #define ST75256_DAT_DISPLAY_CONTROL_1  0X00
// #define ST75256_DAT_DISPLAY_CONTROL_2  159
// #define ST75256_DAT_DISPLAY_CONTROL_3  0X20

#define ST75256_CMD_COLOR_MODE         0XF0

#define ST75256_CMD_VOLUME_CONTROL     0X81
// #define ST75256_DAT_VOLUME_CONTROL_1   0X18
// #define ST75256_DAT_VOLUME_CONTROL_2   0X05

#define ST75256_CMD_POWER_CONTROL      0X20
// #define ST75256_DAT_POWER_CONTROL      0X0B

#define ST75256_CMD_WRITE_DATA         0X5C




#define ST75256_DAT_UNKNOWN            0XA6

#endif  /* ST75256_DISPLAY_DRIVER_H__ */

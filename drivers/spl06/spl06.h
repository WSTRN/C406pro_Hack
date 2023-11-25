/*
 * Copyright (c) 2023 Taisheng WANG <wstrn66@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SPL06_SENSOR_DRIVER_H__
#define SPL06_SENSOR_DRIVER_H__


#define SPL06_REG_PRS_B2 0x00
#define SPL06_REG_PRS_B1 0x01
#define SPL06_REG_PRS_B0 0x02
#define SPL06_REG_TMP_B2 0x03
#define SPL06_REG_TMP_B1 0x04
#define SPL06_REG_TMP_B0 0x05

#define SPL06_REG_PRS_CFG 0x06
#define SPL06_REG_TMP_CFG 0x07
#define SPL06_REG_MEAS_CFG 0x08
#define SPL06_REG_CFG_REG 0x09

#define SPL06_REG_C0        0x10
#define SPL06_REG_C0C1      0x11
#define SPL06_REG_C1        0x12
#define SPL06_REG_C00_1     0x13
#define SPL06_REG_C00_2     0x14
#define SPL06_REG_C00C10    0x15
#define SPL06_REG_C10_1     0x16
#define SPL06_REG_C10_2     0x17
#define SPL06_REG_C01_1     0x18
#define SPL06_REG_C01_2     0x19
#define SPL06_REG_C11_1     0x1a
#define SPL06_REG_C11_2     0x1b
#define SPL06_REG_C20_1     0x1c
#define SPL06_REG_C20_2     0x1d
#define SPL06_REG_C21_1     0x1e
#define SPL06_REG_C21_2     0x1f
#define SPL06_REG_C30_1     0x20
#define SPL06_REG_C30_2     0x21


#define k_SPS1 524288.0
#define k_SPS2 1572864.0
#define k_SPS4 3670016.0
#define k_SPS8 7864320.0
#define k_SPS16 253952.0
#define k_SPS32 516096.0
#define k_SPS64 1040384.0
#define k_SPS128 2088960.0

#define Total_Number_24 16777216.0
#define Total_Number_20 1048576.0
#define Total_Number_16 65536.0
#define Total_Number_12 4096.0


#endif  /* SPL06_SENSOR_DRIVER_H__ */
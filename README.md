# C406pro_Hack
C406pro is a bike computer from Magene. This repository includes a customized firmware and hardware information for this device.

## Hardware
### Components
- Nordic nRF52840 BLE SoC
- Sitronix st75256 128x160 monochrome display
- Goertek spl06 pressure sensor
- Unicore uc6226 Multi-GNSS Positioning SoC
- GigaDevice gd25q256 32MB flash memory

### Pinmap
| Pin | Function |
| --- | --- |
| P0.26 | Display SDA |
| P0.04 | Display SCL |
| P1.07 | Display RST |
| P1.04 | Display DC |
| P1.06 | Display CS |
| P1.10 | Display BL |
| P0.08 | Power mos |
| P0.09 | Beep |
| P0.20 | Flash Hold |
| P0.22 | Flash CLK |
| P0.24 | Flash MOSI |
| P0.15 | Flash WP |
| P0.14 | Flash MISO |
| P0.13 | Flash CS |
| P0.06 | UC6226 RST |
| P1.11 | UC6226 RXD |
| P1.13 | UC6226 TXD |
| P0.11 | Button 1 |
| P0.12 | Button 2 |
| P1.15 | Button 3 |
| P0.25 | SPL06 SCK |
| P1.00 | SPL06 SDA |
| P0.29 | Battery ADC X4|
| P0.31 | Charge detection|

## Firmware
### Features
- Zephyr RTOS
- LVGL GUI
- Display driver
- Pressure sensor driver
- Battery voltage monitoring
- Pressure and temperature monitoring
- Altitude estimation
- Button handling
- Power management

### Building
#### Clone this repository as a standalone Zephyr application
```bash
git clone git@github.com:WSTRN/C406pro_Hack.git
```
#### Go to the project directory
```bash
cd C406pro_Hack
```
#### Initialize the application
```bash
west init -l app
```
#### Update to fetch modules
```bash
west update
```
#### Export Zephyr CMake package
```bash
west zephyr-export
```
#### Install Zephyr Python Dependencies
```bash
pip3 install -r zephyr/scripts/requirements.txt
```
#### Go to the application directory
```bash
cd app
```
#### Build the application
```bash
west build -p=auto
```

### TODO
- [ ] Add BLE support
- [ ] Add GPS support
- [ ] Add Flash storage support
- [ ] Add offline maps support
- [ ] Add track recording support
- [ ] Add power saving mode
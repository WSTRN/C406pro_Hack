cmake_minimum_required(VERSION 3.20.0)

set(BOARD C406pro)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Add our custom Zephyr module for drivers, etc.
set(EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/module)

find_package(Zephyr)
project(C406pro_Hack)

target_include_directories(app PRIVATE
    src
    src/pages
)
target_sources(app PRIVATE
    src/main.c
    src/ble/ble.c
    src/ble/transfer_ble.c
    src/ble/shell_ble.c
    src/pages/pages.cpp
    src/pages/main_page.cpp
    src/pages/info_page.cpp
    src/pages/PageManager.cpp
    src/status_bar.cpp
    src/battery.c
    src/cmd/test_fs.c
    src/cmd/test_flash.c
    src/cmd/gnss.c
    src/button.cpp
    src/button_event.cpp
	src/gnss.c
)

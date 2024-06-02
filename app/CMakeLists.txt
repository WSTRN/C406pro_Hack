cmake_minimum_required(VERSION 3.20.0)

set(BOARD C406pro)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Add our custom Zephyr module for drivers, etc.
set(EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/module)

find_package(Zephyr)
project(C406pro_Hack)

target_include_directories(app PRIVATE
    src
)
target_sources(app PRIVATE
    src/main.c
    src/main_page.c
    src/battery.c
    src/button.cpp
    src/button_event.cpp
	src/gnss.c
)


cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sht41_esp32)

# Enable SHT41 sensor driver
set(CONFIG_SHT4X y)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
target_include_directories(app PRIVATE include)
target_sources(app PRIVATE src/main.cpp)

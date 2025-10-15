#!/bin/bash

ZEPHYR_TOOLCHAIN_VARIANT="zephyr" west build -p always -b esp32_devkitc_wrover/esp32/procpu ./  -- -DCMAKE_EXPORT_COMPILE_COMMANDS=1

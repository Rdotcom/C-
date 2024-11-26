# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set board type because we are building for Pico W
set(PICO_BOARD pico_w)
# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set name of project and C/C++ standards
project(blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()


# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME}
        main.c

)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link with necessary libraries
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_pwm
        hardware_gpio
        hardware_i2c
        hardware_uart
)

# Disable USB output, enable UART output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

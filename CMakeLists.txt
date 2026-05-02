cmake_minimum_required(VERSION 3.10)
project(sdr-msi-driver C)

set(CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

########################################################################
# Find dependencies
########################################################################
find_package(PkgConfig REQUIRED)

find_package(LibUSB)
if(NOT LIBUSB_FOUND)
    message(FATAL_ERROR "LibUSB 1.0 required")
endif()

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FFTW3F REQUIRED fftw3f)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

########################################################################
# Kernel driver detach option
########################################################################
option(DETACH_KERNEL_DRIVER "Detach kernel driver if loaded" OFF)
if(DETACH_KERNEL_DRIVER)
    message(STATUS "Building with kernel driver detaching enabled")
    add_definitions(-DDETACH_KERNEL_DRIVER=1)
endif()

########################################################################
# libmsisdr (static library)
########################################################################
add_library(msisdr STATIC
    driver/src/libmsisdr.c
)

target_include_directories(msisdr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/include
    ${LIBUSB_INCLUDE_DIR}
)

target_link_libraries(msisdr
    ${LIBUSB_LIBRARIES}
)

if(CMAKE_COMPILER_IS_GNUCC AND NOT WIN32)
    target_compile_options(msisdr PRIVATE -Wall -Wextra -Wno-unused -Wsign-compare)
endif()

########################################################################
# SDR MSi Driver executable
########################################################################
add_executable(sdr-msi-driver
    app/main.c
    app/device.c
    app/dsp.c
    app/audio.c
    app/ui.c
)

target_include_directories(sdr-msi-driver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/include
    ${LIBUSB_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
)

target_link_directories(sdr-msi-driver PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${FFTW3F_LIBRARY_DIRS}
)

target_link_libraries(sdr-msi-driver
    msisdr
    ${LIBUSB_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${FFTW3F_LIBRARIES}
    OpenGL::GL
    Threads::Threads
    m
)

target_compile_definitions(sdr-msi-driver PRIVATE
    GL_SILENCE_DEPRECATION
)

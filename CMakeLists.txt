# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.5)

set(PROJECT_EXTRA_COMPONENTS "${PROJECT_SOURCE_DIR}/bootloader_components")
if(EXISTS ${PROJECT_EXTRA_COMPONENTS})
    list(APPEND EXTRA_COMPONENT_DIRS "${PROJECT_EXTRA_COMPONENTS}")
endif()

# Consider each directory in project's bootloader_components as a component to be compiled
file(GLOB proj_components RELATIVE ${PROJECT_EXTRA_COMPONENTS} ${PROJECT_EXTRA_COMPONENTS}/*)
foreach(component ${proj_components})
    if(IS_DIRECTORY ${PROJECT_EXTRA_COMPONENTS}/${component})
        list(APPEND COMPONENTS ${component})
    endif()
endforeach()

# Setup ESP-IDF before using any SDKCONFIG variables
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Version string
set(PROJECT_VER "704_23")

# === Detect correct sdkconfig path ===
if(DEFINED SDKCONFIG)
    set(SDKCONFIG_PATH "${SDKCONFIG}")
else()
    set(SDKCONFIG_PATH "${CMAKE_SOURCE_DIR}/sdkconfig")
endif()
message(STATUS "Using sdkconfig path: ${SDKCONFIG_PATH}")
# Default suffix
set(PROJECT_NAME_SUFFIX "GenericFirmware_")

# === Parse sdkconfig to select firmware type ===
if(EXISTS "${SDKCONFIG_PATH}")
    file(READ "${SDKCONFIG_PATH}" SDKCONFIG_CONTENT)

string(REGEX MATCH "CONFIG_HARDWARE_B480[ \t]*=[ \t]*y" MATCH_B480 "${SDKCONFIG_CONTENT}")
string(REGEX MATCH "CONFIG_HARDWARE_B553[ \t]*=[ \t]*y" MATCH_B553 "${SDKCONFIG_CONTENT}")
string(REGEX MATCH "CONFIG_HARDWARE_B543[ \t]*=[ \t]*y" MATCH_B543 "${SDKCONFIG_CONTENT}")
string(REGEX MATCH "CONFIG_HARDWARE_B542[ \t]*=[ \t]*y" MATCH_B542 "${SDKCONFIG_CONTENT}")
string(REGEX MATCH "CONFIG_HARDWARE_B394[ \t]*=[ \t]*y" MATCH_B394 "${SDKCONFIG_CONTENT}")
string(REGEX MATCH "CONFIG_HARDWARE_B527[ \t]*=[ \t]*y" MATCH_B527 "${SDKCONFIG_CONTENT}")

    if(MATCH_B480)
        message(STATUS "Detected CONFIG_HARDWARE_B480")
        set(PROJECT_NAME_SUFFIX "X-Series_Firmware_")
        add_definitions(-DHARDWARE_B480)
        add_compile_definitions(B480)
    elseif(MATCH_B553)
        message(STATUS "Detected CONFIG_HARDWARE_B553")
        set(PROJECT_NAME_SUFFIX "X-Mini_Firmware_")
        add_definitions(-DHARDWARE_B553)
        add_compile_definitions(B553)
    elseif(MATCH_B543)
        message(STATUS "Detected CONFIG_HARDWARE_B543")
        set(PROJECT_NAME_SUFFIX "X-POE_Firmware_")
        add_definitions(-DHARDWARE_B543)
         add_compile_definitions(B543)
    elseif(MATCH_B542)
        message(STATUS "Detected CONFIG_HARDWARE_B542")
        set(PROJECT_NAME_SUFFIX "Q-POE_Firmware_")
        add_definitions(-DHARDWARE_B542)
         add_compile_definitions(B542)
	elseif(MATCH_B394)
        message(STATUS "Detected CONFIG_HARDWARE_B394")
        set(PROJECT_NAME_SUFFIX "Auto_Link_Firmware_")
        add_definitions(-DHARDWARE_B394)
        add_compile_definitions(B394)
	elseif(MATCH_B527)
        message(STATUS "Detected CONFIG_HARDWARE_B527")
        set(PROJECT_NAME_SUFFIX "SmartOutlet_Firmware_")
        add_definitions(-DHARDWARE_B527)
        add_compile_definitions(B527)
    else()
        message(WARNING "No known hardware type enabled in sdkconfig.")
    endif()
else()
    message(WARNING "sdkconfig file not found at: ${SDKCONFIG_PATH}")
endif()

# Set final project name
set(PROJECT_NAME "${PROJECT_NAME_SUFFIX}${PROJECT_VER}")

if(NOT CMAKE_BUILD_TYPE)  #This added to enable debugging
    set(CMAKE_BUILD_TYPE Debug)
endif() 

project(${PROJECT_NAME})

# === Optional Secure Element Fetch Logic ===
if(CONFIG_ESP_TLS_USE_SECURE_ELEMENT)
    if(CONFIG_MBEDTLS_ATCA_HW_ECDSA_SIGN AND CONFIG_MBEDTLS_ATCA_HW_ECDSA_VERIFY)
        include(FetchContent)
        if(NOT esp-cryptoauthlib_POPULATED)
            FetchContent_Populate(esp-cryptoauthlib
                GIT_REPOSITORY https://github.com/espressif/esp-cryptoauthlib
                GIT_TAG        985ea960f83f67f66881e2dae57097e2a07b92a0
                SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/components/esp-cryptoauthlib"
            )
        endif()
    else()
        message(FATAL_ERROR "To use the Microchip ATECC608 secure element, refer to README_ATECC608_Support.md")
    endif()
endif()

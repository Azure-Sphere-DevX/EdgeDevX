CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(edge_devx C)

# AZURE_IOT_SUPPORT Variable can be set to ON or OFF by inheriting CMakeLists.txt
set(AZURE_IOT_SUPPORT ON CACHE BOOL "Enable Azure Cloud support for the project" )

################################################################################
# Source groups
################################################################################
set(Source
    "./src/dx_async.c"
    "./src/dx_json_serializer.c"
    "./src/dx_mqtt.c"
    "./src/dx_terminate.c"
    "./src/dx_timer.c"
    "./src/dx_utilities.c"
    "./src/log.c"
    "./src/parson.c"
    "./src/dx_openai_functions.c"

)
source_group("Source" FILES ${Source})

if(AZURE_IOT_SUPPORT)
    message(STATUS "Azure IoT Central/Hub Support Enabled")

    set(Cloud 
        "./src/dx_avnet_iot_connect.c"
        "./src/dx_azure_iot.c"
        "./src/dx_config.c"   
        "./src/dx_device_twins.c"
        "./src/dx_direct_methods.c"
    )
    source_group("Cloud" FILES ${Cloud})

endif()

# Support for gpio on Linux
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   message(STATUS "EdgeDevX Linux Peripherals Enabled")

   set(LinuxPeripherals
       "./src/dx_gpio.c"
   )
   source_group("LinuxPeripherals" FILES ${LinuxPeripherals})
endif()

set(ALL_FILES
    ${Source}
    ${LinuxPeripherals}
    ${Cloud}
)

################################################################################
# MQTT-C Library Configuration
################################################################################
# Configure MQTT-C build options
set(MQTT_C_EXAMPLES OFF CACHE BOOL "Disable MQTT-C examples")
set(MQTT_C_TESTS OFF CACHE BOOL "Disable MQTT-C tests")

# Add MQTT-C as a subdirectory
add_subdirectory(MQTT-C)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC MQTT-C/include)

# set(ROOT_NAMESPACE rpi_libs)
# set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_KEYWORD "EdgeDevX")

if(AZURE_IOT_SUPPORT)

    # Set options for Azure IoT SDK C lib
    set(use_amqp OFF CACHE  BOOL "Set amqp off" FORCE )
    set(use_http OFF CACHE  BOOL "Set http off" FORCE )
    set(use_mqtt ON CACHE  BOOL "Set http off" FORCE )
    set(skip_samples ON CACHE  BOOL "Skip samples" FORCE )
    set(use_prov_client ON CACHE  BOOL "Use prov client" FORCE )

    # Add Azure IoT SDK C
    add_subdirectory(azure-iot-sdk-c out)

    target_include_directories(${PROJECT_NAME} PUBLIC azure-iot-sdk-c/iothub_client/inc)
    target_include_directories(${PROJECT_NAME} PUBLIC azure-iot-sdk-c/provisioning_client/inc)
    target_include_directories(${PROJECT_NAME} PUBLIC azure-iot-sdk-c/c-utility/inc)
    target_include_directories(${PROJECT_NAME} PUBLIC azure-iot-sdk-c/deps/umock-c/inc)
    target_include_directories(${PROJECT_NAME} PUBLIC azure-iot-sdk-c/deps/azure-macro-utils-c/inc)

    target_link_libraries (${PROJECT_NAME} 
        aziotsharedutil
        hsm_security_client
        iothub_client
        parson
        prov_auth_client
        prov_device_ll_client
        prov_mqtt_transport
        umqtt
        )

endif()

#
# The following config was required for macOS homebrew installed UV, UUID and SSL libs
# Install brew
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
#
# Install required libraries
# brew install mosquitto libuv openssl ossp-uuid
#
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # macOS on Apple silicon (arm64)
    if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "arm64")
        target_include_directories(${PROJECT_NAME} PUBLIC /opt/homebrew/include)
        target_link_directories(${PROJECT_NAME} PUBLIC /opt/homebrew/lib)
    else()
    # macOS on Intel
        target_include_directories(${PROJECT_NAME} PUBLIC /usr/local/include)
        target_link_directories(${PROJECT_NAME} PUBLIC /usr/local/lib)
    endif()
endif()

# required for dx_gpio on linux support
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Use libgpiod 2.x from /usr/local
    target_include_directories(${PROJECT_NAME} PUBLIC /usr/local/include)
    target_link_directories(${PROJECT_NAME} PUBLIC /usr/local/lib)
    target_link_libraries (${PROJECT_NAME} gpiod)
endif()

target_link_libraries (${PROJECT_NAME}
    curl
    m
    mqttc
    pthread
    uuid
    uv
)

cmake_minimum_required(VERSION 3.10)

# Project name and version
project(waggle LANGUAGES C)

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -fPIC")
set(CMAKE_SHARED_LINKER_FLAGS "-shared")

# Option to enable debug printing
option(ENABLE_DEBUG "Enable debug prints" OFF)
if(ENABLE_DEBUG)
    add_definitions(-DDEBUG)
    message(STATUS "Debug prints enabled (DEBUG defined).")
endif()

# User-defined options for external libraries
option(RABBITMQ_DIR "Path to RabbitMQ installation directory" "")
option(CJSON_DIR "Path to cJSON installation directory" "")

# Include directories
include_directories(include)

# Add user-specified include paths
if(RABBITMQ_DIR)
    include_directories(${RABBITMQ_DIR}/include)
    link_directories(${RABBITMQ_DIR}/lib)
endif()

if(CJSON_DIR)
    include_directories(${CJSON_DIR}/include)
    link_directories(${CJSON_DIR}/lib)
endif()

# Source files
set(SRCS
    src/waggle/plugin/plugin.c
    src/waggle/plugin/config.c
    src/waggle/plugin/rabbitmq.c
    src/waggle/plugin/uploader.c
    src/waggle/plugin/filepublisher.c
    src/waggle/data/timeutil.c
    src/waggle/data/wagglemsg.c
)

# Target library
add_library(waggle SHARED ${SRCS})

# Link libraries
target_link_libraries(waggle cjson rabbitmq pthread)

# Install the library
install(TARGETS waggle
    LIBRARY DESTINATION lib
)

# Install include files
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Custom target for cleaning (renamed to "clean_custom")
add_custom_target(clean_custom
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/libwaggle.so
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/*.o
    COMMENT "Custom clean target"
)

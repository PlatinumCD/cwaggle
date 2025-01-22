cmake_minimum_required(VERSION 3.10)

# Project name and version
project(waggle LANGUAGES C)

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -fPIC")
set(CMAKE_SHARED_LINKER_FLAGS "-shared")

# Include directories
include_directories(include)

# Source files
set(SRCS
    src/plugin/plugin.c
    src/plugin/config.c
    src/plugin/rabbitmq.c
    src/plugin/uploader.c
    src/timeutil.c
    src/wagglemsg.c
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

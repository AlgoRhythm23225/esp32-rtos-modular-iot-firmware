# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22...3.28)
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/driver"
    "${CMAKE_CURRENT_SOURCE_DIR}/driver/sensor_drivers"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Project)

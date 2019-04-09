cmake_minimum_required(VERSION 2.8.4)
project(cpp-mods.coreutils)

set(SOURCE_FILES log.cpp)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(utilsincfiles SOURCES ${IncFiles})

add_library(coreutils ${SOURCE_FILES})
target_link_libraries(coreutils PUBLIC fmt)
target_include_directories(coreutils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../)
set_property(TARGET coreutils PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET coreutils PROPERTY CXX_STANDARD 14)
set_property(TARGET coreutils PROPERTY CXX_STANDARD_REQUIRED ON)


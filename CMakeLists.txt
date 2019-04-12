
cmake_minimum_required(VERSION 3.12)
project(dox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

execute_process(COMMAND llvm-config --includedir OUTPUT_VARIABLE LIBCLANG_INCLUDE)
execute_process(COMMAND llvm-config --libdir OUTPUT_VARIABLE LIBCLANG_LIBDIR)
string(REGEX REPLACE "\n$" "" LIBCLANG_LIBDIR "${LIBCLANG_LIBDIR}")
string(REGEX REPLACE "\n$" "" LIBCLANG_INCLUDE "${LIBCLANG_INCLUDE}")
link_directories(${LIBCLANG_LIBDIR})


add_subdirectory(external/coreutils)
add_subdirectory(external/lua)
add_subdirectory(external/sol2)
add_subdirectory(external/fmt)
add_subdirectory(external/CLI11)
add_subdirectory(external/cppast)

add_executable(dox main.cpp)
target_include_directories(dox PRIVATE ${LIBCLANG_INCLUDE})
target_link_libraries(dox PRIVATE pthread cppast clang sol coreutils)

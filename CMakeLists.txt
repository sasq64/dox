
cmake_minimum_required(VERSION 3.12)
project(dox)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

execute_process(COMMAND llvm-config --includedir OUTPUT_VARIABLE LIBCLANG_INCLUDE)
execute_process(COMMAND llvm-config --libdir OUTPUT_VARIABLE LIBCLANG_LIBDIR)
execute_process(COMMAND llvm-config --libnames OUTPUT_VARIABLE LIBCLANG_LIBNAMES)
execute_process(COMMAND llvm-config --libs OUTPUT_VARIABLE LIBCLANG_LIBS)
set(LIBCLANG_LIBDIR "/opt/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04/lib")
link_directories(${LIBCLANG_LIBDIR})

add_executable(dox main.cpp)
target_include_directories(dox PRIVATE ${LIBCLANG_INCLUDE})
#target_link_options(dox PRIVATE ${LIBCLANG_LIBS})
target_link_libraries(dox PRIVATE pthread clang)

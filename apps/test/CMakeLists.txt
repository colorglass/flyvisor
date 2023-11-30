cmake_minimum_required(VERSION 3.7.2)

set(RPI4_MEMORY 4096 CACHE STRING "Memory size in MB")

# include extern cmake project
set(seL4_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../seL4)
add_subdirectory(${seL4_dir}/projects/sel4test ${CMAKE_CURRENT_BINARY_DIR}/sel4test)

project(test)
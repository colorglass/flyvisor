cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(crypto_comm C CXX ASM)

# set up camkes tool and vm environment
find_package(camkes-tool REQUIRED)
find_package(global-components REQUIRED)
find_package(sel4_projects_libs REQUIRED)

camkes_tool_setup_camkes_build_environment()
sel4_projects_libs_import_libraries()
global_components_import_project()

include(cpio)
file(GLOB public_keys "${CMAKE_CURRENT_SOURCE_DIR}/public_keys/*")
MakeCPIO(cpio_keys.o "${public_keys}")
add_library(cpio_archive STATIC cpio_keys.o)
set_target_properties(cpio_archive PROPERTIES LINKER_LANGUAGE C)
DeclareCAmkESComponent(FileServer LIBS cpio_archive)

add_subdirectory(${libs_dir}/GmSSL libgmssl)


DeclareCAmkESComponent(Rx 
    SOURCES components/rx/rx.c 
    INCLUDES include
)
DeclareCAmkESComponent(Encrypt 
    SOURCES components/encrypt/encrypt.c 
    INCLUDES include 
    LIBS gmssl
)
DeclareCAmkESComponent(Decrypt 
    SOURCES components/decrypt/decrypt.c 
    INCLUDES include ${libs_dir}
    LIBS gmssl FileServer-client
)
DeclareCAmkESComponent(HWRNG
    SOURCES components/hwrng/hwrng.c 
    INCLUDES include
)

DeclareCAmkESRootserver(crypto_comm.camkes CPP_INCLUDES include)

GenerateCAmkESRootserver()

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

# create cpio archive file and file server, in which the public keys are stored
include(cpio)
file(GLOB public_keys "${root_dir}/resources/public_keys/*")
MakeCPIO(cpio_keys.o "${public_keys}")
add_library(cpio_archive STATIC cpio_keys.o)
set_target_properties(cpio_archive PROPERTIES LINKER_LANGUAGE C)
DeclareCAmkESComponent(FileServer LIBS cpio_archive)

add_subdirectory(${libs_dir}/GmSSL libgmssl)

CAmkESAddImportPath(interfaces components)

set(extra_flags)
if(DebugServer)
    list(APPEND extra_flags -DDEBUG_SERVER -DMAVLINK_USE_MESSAGE_INFO)
endif()

DeclareCAmkESComponent(Rx 
    SOURCES components/rx/rx.c 
    INCLUDES include
)
DeclareCAmkESComponent(Server
    SOURCES components/server/server.c driver/rpi4-dma.c
    INCLUDES include ${libs_dir} driver
    LIBS sel4allocman FileServer-client gmssl
    C_FLAGS ${extra_flags}
)
DeclareCAmkESComponent(HWRNG
    SOURCES components/hwrng/hwrng.c 
    INCLUDES include
)

DeclareCAmkESRootserver(crypto_comm.camkes CPP_INCLUDES include)

GenerateCAmkESRootserver()

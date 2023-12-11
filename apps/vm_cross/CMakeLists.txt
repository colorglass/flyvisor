cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(vm_example C CXX ASM)

# set up camkes tool and vm environment
find_package(camkes-tool REQUIRED)
find_package(camkes-vm REQUIRED)
find_package(camkes-arm-vm REQUIRED)
find_package(global-components REQUIRED)
find_package(sel4_projects_libs REQUIRED)

camkes_tool_setup_camkes_build_environment()
sel4_projects_libs_import_libraries()
global_components_import_project()
camkes_arm_vm_import_project()

# configure the linux vm

# add related image to FileServer
set(IMAGE_DIR "${root_dir}/resources/${PLATFORM}/linux")

set(dts_file "${IMAGE_DIR}/vm_linux.dts")
set(dtb_file "${CMAKE_CURRENT_BINARY_DIR}/vm_linux.dtb")

find_program(DTC_TOOL dtc)

if("${DTC_TOOL}" STREQUAL "DTC_TOOL-NOTFOUND")
    message(FATAL_ERROR "Cannot find 'dtc' program.")
endif()

execute_process(
    # COMMAND
    #     ${CPP_TOOL} -nostdinc -I${IMAGE_DIR}/dts -I${IMAGE_DIR}/dts/include -undef -x assembler-with-cpp -o ${dts_pp_file} ${dts_file}
    COMMAND
        ${DTC_TOOL} -q -I dts -O dtb -o ${dtb_file} ${dts_file}
    RESULT_VARIABLE error
)

AddToFileServer("linux" "${IMAGE_DIR}/Image")
# AddToFileServer("linux-initrd" "${IMAGE_DIR}/rootfs.cpio.gz")
AddToFileServer("linux-dtb" "${dtb_file}")

DefineCAmkESVMFileServer()

CAmkESAddImportPath("conf/${PLATFORM}/")

DeclareCAmkESComponent(VM SOURCES modules/cross_vm_connections.c)
DeclareCAmkESComponent(CrossvmInit SOURCES components/CrossvmInit/crossvm_init.c)

DeclareCAmkESRootserver(vm_cross.camkes
    CPP_INCLUDES
    ${CAMKES_VM_DIR}/components/VM_Arm
)

GenerateCAmkESRootserver()

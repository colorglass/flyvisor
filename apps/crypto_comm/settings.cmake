
# include all the dependcies for the seL4 project
set(root_dir "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(seL4_dir "${root_dir}/seL4")
set(libs_dir "${root_dir}/libs")
file(GLOB project_modules "${seL4_dir}/projects/*")
list(
    APPEND CMAKE_MODULE_PATH
    ${seL4_dir}/kernel
    ${seL4_dir}/tools/seL4/cmake-tool/helpers
    ${seL4_dir}/tools/seL4/elfloader-tool
    ${project_modules}
)

# print CMAKE_MODULE_PATH
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# import default options
include(${CMAKE_CURRENT_LIST_DIR}/options.cmake)

# configure the platform related configurations
include(application_settings)
correct_platform_strings()
ApplyCommonReleaseVerificationSettings(${RELEASE} FALSE)

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()

ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
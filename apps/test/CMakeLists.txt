cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(test C CXX ASM)

# set up camkes tool and vm environment
find_package(camkes-tool REQUIRED)
find_package(global-components REQUIRED)
find_package(sel4_projects_libs REQUIRED)

camkes_tool_setup_camkes_build_environment()
sel4_projects_libs_import_libraries()
global_components_import_project()

DeclareCAmkESComponent(Test
    SOURCES test.c
)

DeclareCAmkESRootserver(test.camkes)

GenerateCAmkESRootserver()

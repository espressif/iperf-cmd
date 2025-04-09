# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# idf < 5.0
if(${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR} VERSION_LESS 5.0)
    add_compile_options("-Wno-format")
endif()

if(COMMAND idf_build_set_property)
    idf_build_set_property(MINIMAL_BUILD ON)
endif()

project(loopback)

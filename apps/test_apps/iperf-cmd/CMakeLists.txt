cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(COMMAND idf_build_set_property)
    idf_build_set_property(MINIMAL_BUILD ON)
endif()

project(iperf)

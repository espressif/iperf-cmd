include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(srcs  "iperf.c")

set(priv_requires freertos esp_netif esp_timer)

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11.0)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
endif()

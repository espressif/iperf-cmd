include($ENV{IDF_PATH}/tools/cmake/version.cmake)

idf_build_get_property(target IDF_TARGET)

set(srcs  "iperf.c" "iperf_report.c")
set(priv_requires freertos esp_timer)

if(${target} STREQUAL "linux")
    list(APPEND srcs "linux_port/esp_timer_stub.c")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires}
                    REQUIRES esp_netif)
if(COMMAND idf_component_optional_requires)
    idf_component_optional_requires(PUBLIC lwip)
endif()


if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11.0)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
endif()

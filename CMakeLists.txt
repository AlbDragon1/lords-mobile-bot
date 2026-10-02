cmake_minimum_required(VERSION 3.10)

project(LordsMobileBot VERSION 1.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(client
    src/main.c
    src/connection.c
    src/log.c
    src/protocol.c
    src/des.c
    src/map_point.c
    src/command.c
    src/config.c
)

target_compile_definitions(client PRIVATE
    VERSION="${PROJECT_VERSION}"
)

target_include_directories(client PRIVATE include)

if(WIN32)
    target_link_libraries(client PRIVATE ws2_32)
endif()

# Unit tests (run with: ctest --test-dir build --output-on-failure)
option(LMBOT_BUILD_TESTS "Build unit tests" ON)

if(LMBOT_BUILD_TESTS)
    enable_testing()

    add_executable(test_config
        tests/test_config.c
        src/config.c
    )
    target_include_directories(test_config PRIVATE include)

    if(WIN32)
        target_link_libraries(test_config PRIVATE ws2_32)
    endif()

    add_test(NAME config COMMAND test_config)
endif()

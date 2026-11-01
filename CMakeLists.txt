cmake_minimum_required(VERSION 3.16)
project(unified-engineering-workbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and compiler warning flags
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc /O2)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
    if(WIN32)
        add_link_options(-static -static-libgcc -static-libstdc++)
    endif()
endif()

# Core static library
add_library(workbench_core STATIC
    src/core/fea/thermal_fea.cpp
    src/core/parsers/stl_parser.cpp
    src/core/parsers/ansys_csv_parser.cpp
    src/core/multicore/batch_solver.cpp
    src/core/embedded/mock_adapters.cpp
    src/core/backprop/backprop_engine.cpp
    src/server/http_server.cpp
)

target_include_directories(workbench_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(workbench_core PUBLIC ws2_32)
endif()

# Automated Unit Tests
add_executable(unit_tests
    tests/unit_tests.cpp
)
target_link_libraries(unit_tests PRIVATE workbench_core)

# Headless CLI Runner
add_executable(workbench_cli
    src/cli/main.cpp
)
target_link_libraries(workbench_cli PRIVATE workbench_core)

# Standalone Application Server (GUI backend)
add_executable(workbench_server
    src/server/main.cpp
)
target_link_libraries(workbench_server PRIVATE workbench_core)

# Enable CTest
enable_testing()
add_test(NAME AllUnitTests COMMAND unit_tests)

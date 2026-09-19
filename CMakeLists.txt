cmake_minimum_required(VERSION 3.24)
project(ArenaTES3JSON VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(ARENATES3JSON_BUILD_GUI "Build Qt Widgets GUI" ON)
option(ARENATES3JSON_BUILD_CLI "Build command-line converter" ON)
option(ARENATES3JSON_BUILD_TESTS "Build self tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(ArenaTES3JSONCore STATIC
    src/core/cp1251.cpp
    src/core/cp1251.h
    src/core/tes3plugin.cpp
    src/core/tes3plugin.h
    src/core/converter.cpp
    src/core/converter.h
)
target_link_libraries(ArenaTES3JSONCore PUBLIC Qt6::Core)
target_include_directories(ArenaTES3JSONCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ArenaTES3JSONCore PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(MSVC)
    target_compile_options(ArenaTES3JSONCore PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ArenaTES3JSONCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ARENATES3JSON_BUILD_CLI)
    add_executable(ArenaTES3JSON-cli src/cli/main.cpp)
    target_link_libraries(ArenaTES3JSON-cli PRIVATE ArenaTES3JSONCore Qt6::Core)
    if(MSVC)
        target_compile_options(ArenaTES3JSON-cli PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

if(ARENATES3JSON_BUILD_GUI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    add_executable(ArenaTES3JSON
        src/gui/main.cpp
        src/gui/mainwindow.cpp
        src/gui/mainwindow.h
        resources/resources.qrc
    )
    target_link_libraries(ArenaTES3JSON PRIVATE ArenaTES3JSONCore Qt6::Widgets)
    if(WIN32)
        set_target_properties(ArenaTES3JSON PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    if(MSVC)
        target_compile_options(ArenaTES3JSON PRIVATE /W4 /permissive- /utf-8)
    endif()
endif()

if(ARENATES3JSON_BUILD_TESTS)
    enable_testing()
    add_executable(ArenaTES3JSONSelfTest tests/selftest.cpp)
    target_link_libraries(ArenaTES3JSONSelfTest PRIVATE ArenaTES3JSONCore Qt6::Core)
    if(MSVC)
        target_compile_options(ArenaTES3JSONSelfTest PRIVATE /W4 /permissive- /utf-8)
    endif()
    add_test(NAME ArenaTES3JSONSelfTest COMMAND ArenaTES3JSONSelfTest)
endif()

install(TARGETS ArenaTES3JSONCore ARCHIVE DESTINATION lib)
if(TARGET ArenaTES3JSON)
    install(TARGETS ArenaTES3JSON RUNTIME DESTINATION bin)
endif()
if(TARGET ArenaTES3JSON-cli)
    install(TARGETS ArenaTES3JSON-cli RUNTIME DESTINATION bin)
endif()

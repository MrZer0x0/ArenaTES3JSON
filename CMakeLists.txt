cmake_minimum_required(VERSION 3.24)
project(ArenaTES3JSON VERSION 0.3.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(ARENATES3JSON_BUILD_GUI "Build Qt Widgets GUI" ON)
option(ARENATES3JSON_BUILD_CLI "Build Qt command-line frontend" ON)
option(ARENATES3JSON_BUILD_TESTS "Build tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_program(CARGO_EXECUTABLE cargo REQUIRED)

if(WIN32)
    enable_language(RC)
    set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/ArenaTES3JSON.ico")
    file(TO_CMAKE_PATH "${APP_ICON_ICO}" APP_ICON_ICO_RC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ArenaTES3JSON_icon.rc"
        @ONLY
    )
endif()

if(WIN32)
    set(RUST_CORE_EXE "ArenaTES3JSON-core.exe")
else()
    set(RUST_CORE_EXE "ArenaTES3JSON-core")
endif()

set(RUST_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/core-rs/Cargo.toml")
set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target")
set(RUST_CORE_BUILT "${RUST_TARGET_DIR}/release/${RUST_CORE_EXE}")
set(RUST_CORE_STAGE "${CMAKE_BINARY_DIR}/rust-bin/${RUST_CORE_EXE}")

file(GLOB_RECURSE RUST_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core-rs/src/*.rs"
    "${CMAKE_CURRENT_SOURCE_DIR}/core-rs/Cargo.toml"
    "${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml")

add_custom_command(
    OUTPUT "${RUST_CORE_STAGE}"
    COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
            "${CARGO_EXECUTABLE}" build --manifest-path "${RUST_MANIFEST}" --release
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/rust-bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${RUST_CORE_BUILT}" "${RUST_CORE_STAGE}"
    DEPENDS ${RUST_CORE_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/core-rs"
    VERBATIM
)
add_custom_target(ArenaTES3JSONRustCore ALL DEPENDS "${RUST_CORE_STAGE}")

add_library(ArenaTES3JSONFrontend STATIC
    src/core/converter.cpp
    src/core/converter.h
    src/core/localization.h
)
target_link_libraries(ArenaTES3JSONFrontend PUBLIC Qt6::Core)
target_include_directories(ArenaTES3JSONFrontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ArenaTES3JSONFrontend PRIVATE ARENATES3JSON_RUST_CORE_NAME="${RUST_CORE_EXE}")

if(MSVC)
    target_compile_options(ArenaTES3JSONFrontend PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ArenaTES3JSONFrontend PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(stage_rust_core target_name)
    add_dependencies(${target_name} ArenaTES3JSONRustCore)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RUST_CORE_STAGE}" "$<TARGET_FILE_DIR:${target_name}>/${RUST_CORE_EXE}"
        VERBATIM)
endfunction()

if(ARENATES3JSON_BUILD_CLI)
    add_executable(ArenaTES3JSON-cli src/cli/main.cpp)
    target_link_libraries(ArenaTES3JSON-cli PRIVATE ArenaTES3JSONFrontend Qt6::Core)
    stage_rust_core(ArenaTES3JSON-cli)
endif()

if(ARENATES3JSON_BUILD_GUI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    set(ARENATES3JSON_GUI_SOURCES
        src/gui/main.cpp
        src/gui/mainwindow.cpp
        src/gui/mainwindow.h
        resources/resources.qrc
    )
    if(WIN32)
        list(APPEND ARENATES3JSON_GUI_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/ArenaTES3JSON_icon.rc")
    endif()
    add_executable(ArenaTES3JSON ${ARENATES3JSON_GUI_SOURCES})
    target_link_libraries(ArenaTES3JSON PRIVATE ArenaTES3JSONFrontend Qt6::Widgets)
    stage_rust_core(ArenaTES3JSON)
    if(WIN32)
        set_target_properties(ArenaTES3JSON PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

if(ARENATES3JSON_BUILD_TESTS)
    enable_testing()
    add_executable(ArenaTES3JSONSelfTest tests/selftest.cpp)
    target_link_libraries(ArenaTES3JSONSelfTest PRIVATE ArenaTES3JSONFrontend Qt6::Core)
    stage_rust_core(ArenaTES3JSONSelfTest)
    add_test(NAME ArenaTES3JSONFrontendSelfTest COMMAND ArenaTES3JSONSelfTest)
    add_test(NAME ArenaTES3JSONRustHelp COMMAND "${RUST_CORE_STAGE}" --help)
endif()

install(TARGETS ArenaTES3JSONFrontend ARCHIVE DESTINATION lib)
if(TARGET ArenaTES3JSON)
    install(TARGETS ArenaTES3JSON RUNTIME DESTINATION bin)
endif()
if(TARGET ArenaTES3JSON-cli)
    install(TARGETS ArenaTES3JSON-cli RUNTIME DESTINATION bin)
endif()
install(PROGRAMS "${RUST_CORE_STAGE}" DESTINATION bin)

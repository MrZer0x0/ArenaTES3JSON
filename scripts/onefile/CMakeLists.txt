cmake_minimum_required(VERSION 3.24)
project(ArenaTES3JSONOneFile LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "The one-file launcher is Windows-only")
endif()
if(NOT DEFINED PAYLOAD_ZIP OR NOT EXISTS "${PAYLOAD_ZIP}")
    message(FATAL_ERROR "PAYLOAD_ZIP must point to the generated runtime ZIP")
endif()
if(NOT DEFINED PAYLOAD_ID)
    message(FATAL_ERROR "PAYLOAD_ID is required")
endif()
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "dev")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(TO_CMAKE_PATH "${PAYLOAD_ZIP}" PAYLOAD_ZIP_RC)
configure_file(payload.rc.in "${CMAKE_CURRENT_BINARY_DIR}/payload.rc" @ONLY)

add_executable(ArenaTES3JSON WIN32
    launcher.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/payload.rc"
)
target_compile_definitions(ArenaTES3JSON PRIVATE
    ARENA_APP_VERSION="${APP_VERSION}"
    ARENA_PAYLOAD_ID="${PAYLOAD_ID}"
)
target_compile_options(ArenaTES3JSON PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(ArenaTES3JSON PRIVATE shell32 user32)

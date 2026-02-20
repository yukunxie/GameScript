cmake_minimum_required(VERSION 3.20)
project(GameScript LANGUAGES CXX)

if(WIN32)
    if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio (18 2026|17 2022)$")
        message(FATAL_ERROR "GameScript requires Visual Studio generator: prefer 'Visual Studio 18 2026', fallback 'Visual Studio 17 2022'. Current generator: ${CMAKE_GENERATOR}")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GS_BUILD_TOOLS "Build script compiler tool" ON)
option(GS_BUILD_DEMO "Build demo runtime app" ON)

set(GS_LIB_SOURCES
    src/binding.cpp
    src/global.cpp
    src/type_system.cpp
    src/tokenizer.cpp
    src/parser.cpp
    src/compiler.cpp
    src/thread_pool.cpp
    src/task_system.cpp
    src/vm.cpp
    src/runtime.cpp
)

set(GS_LIB_HEADERS
    include/gs/binding.hpp
    include/gs/global.hpp
    include/gs/bytecode.hpp
    include/gs/compiler.hpp
    include/gs/parser.hpp
    include/gs/runtime.hpp
    include/gs/task_system.hpp
    include/gs/thread_pool.hpp
    include/gs/tokenizer.hpp
    include/gs/type_system.hpp
    include/gs/vm.hpp
)

add_library(gamescript STATIC)
target_sources(gamescript PRIVATE ${GS_LIB_SOURCES} ${GS_LIB_HEADERS})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GS_LIB_SOURCES} ${GS_LIB_HEADERS})

target_include_directories(gamescript
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(gamescript
    PRIVATE
        GS_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(GS_BUILD_TOOLS)
    add_executable(gsc tools/gsc.cpp)
    target_link_libraries(gsc PRIVATE gamescript)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES tools/gsc.cpp)
endif()

if(GS_BUILD_DEMO)
    add_executable(gs_demo app/main.cpp)
    target_link_libraries(gs_demo PRIVATE gamescript)
    target_compile_definitions(gs_demo PRIVATE GS_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES app/main.cpp)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gs_demo)
endif()

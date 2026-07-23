cmake_minimum_required(VERSION 3.16)

# ---- cli — CLI frontend ----
# 依赖 ../liblpclcore（解耦的纯 C++ 核心库）

project(lpcl-cli VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Git version injection
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(GIT_DESCRIBE STREQUAL "")
    set(GIT_DESCRIBE "v${PROJECT_VERSION}-unknown")
endif()
if(GIT_COMMIT_HASH STREQUAL "")
    set(GIT_COMMIT_HASH "0000000")
endif()
message(STATUS "Build version: ${GIT_DESCRIBE} (${GIT_COMMIT_HASH})")

# Qt 路径 — 如果从父项目继承则跳过，否则尝试常见路径
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.1/gcc_64"
        CACHE PATH "Qt 6 installation prefix")
    message(STATUS "CMAKE_PREFIX_PATH not set, trying: ${CMAKE_PREFIX_PATH}")
endif()

# liblpclcore 可能已由父项目构建，也可能需要独立构建
if(NOT TARGET lpclcore)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../sdk
                     ${CMAKE_CURRENT_BINARY_DIR}/sdk)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(lpcl-cli main.cpp test.cpp tui_select.cpp)

target_include_directories(lpcl-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include
)

target_link_libraries(lpcl-cli PRIVATE lpclcore Qt6::Core)

target_compile_definitions(lpcl-cli PRIVATE
    GIT_DESCRIBE="${GIT_DESCRIBE}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

target_compile_features(lpcl-cli PRIVATE cxx_std_20)

# rpath: 优先在同目录找 liblpclcore.so（打包后 lpcl-cli 和 .so 放一起）
set_target_properties(lpcl-cli PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

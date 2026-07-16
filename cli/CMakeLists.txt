cmake_minimum_required(VERSION 3.16)

# ---- cli — CLI frontend + liblpclcore SDK ----
# 可作为 LPCL/ 的子目录，也可在 CLion/Qt Creator 中独立打开。
# 独立打开时需要设置 CMAKE_PREFIX_PATH 指向 Qt 安装目录。

project(lpcl-cli VERSION 0.1 LANGUAGES CXX)

# Qt 路径 — 如果从父项目继承则跳过，否则尝试常见路径
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.11.1/gcc_64"
        CACHE PATH "Qt 6 installation prefix")
    message(STATUS "CMAKE_PREFIX_PATH not set, trying: ${CMAKE_PREFIX_PATH}")
endif()

# 先构建核心库
add_subdirectory(liblpclcore)

# 再构建 CLI 可执行文件
find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(lpcl-cli main.cpp)

target_include_directories(lpcl-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/liblpclcore/include
    ${CMAKE_CURRENT_SOURCE_DIR}/liblpclcore/include/lpclcore
)

target_link_libraries(lpcl-cli PRIVATE lpclcore Qt6::Core)

target_compile_features(lpcl-cli PRIVATE cxx_std_20)

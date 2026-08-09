cmake_minimum_required(VERSION 3.16)

# ---- liblpclcore — Pure C++ Minecraft launcher core library ----
# Dependencies: Qt6::Core, Qt6::Network, nlohmann-json, ZLIB
# Intentionally does NOT depend on Qt Quick / QML / GUI.

project(lpclcore VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(ZLIB REQUIRED)

# nlohmann-json (header-only, fallback to FetchContent)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.11 QUIET)
    if(NOT nlohmann_json_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()

# Core sources (no GUI, no QML)
set(LPCLCORE_SOURCES
    src/lpcl.cpp
    src/modpack/modpack.cpp
    src/modpack/common.cpp
    src/modpack/detect.cpp
    src/modpack/installers.cpp
    src/modpack/pipeline.cpp
    src/settings.cpp
    src/javamanager.cpp
    src/versionmanager.cpp
    src/launchbuilder.cpp
    src/launcher.cpp
    src/installer.cpp
    src/download/downloadmanager.cpp
    src/download/assetdownloader.cpp
    src/download/modplatform.cpp
    src/auth/offlineauth.cpp
    src/auth/msauth.cpp
    src/auth/authlibauth.cpp
    src/util/file_utils.cpp
    src/util/arg_utils.cpp
    src/util/crypto_utils.cpp
)

set(LPCLCORE_HEADERS
    include/lpcl.h
    include/modpack.h
    include/core/types.h
    include/core/settings.h
    include/core/javamanager.h
    include/core/versionmanager.h
    include/core/launchbuilder.h
    include/core/launcher.h
    include/core/installer.h
    include/core/lpclcore_export.h
    include/auth/authbase.h
    include/auth/offlineauth.h
    include/auth/msauth.h
    include/auth/authlibauth.h
    include/download/downloadmanager.h
    include/download/assetdownloader.h
    include/download/modplatform.h
    include/util/arg_utils.h
    include/util/crypto_utils.h
    include/util/file_utils.h
    include/util/platform_utils.h
)

# 打包 CLI 时用 -DLPCL_STATIC_CORE=ON：静态链接进 lpcl，发布单文件免带 .so
option(LPCL_STATIC_CORE "Static-link lpclcore into dependents" OFF)
if(LPCL_STATIC_CORE)
    add_library(lpclcore STATIC ${LPCLCORE_SOURCES} ${LPCLCORE_HEADERS})
    # Qt 要求使用它的目标都是 PIC；静态库要链接进 PIE 可执行文件也必须 PIC
    set_target_properties(lpclcore PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    add_library(lpclcore SHARED ${LPCLCORE_SOURCES} ${LPCLCORE_HEADERS})
endif()

# CurseForge API key：开发构建从 LPCL/.env 嵌入（发布构建 -DLPCL_EMBED_CF_KEY=OFF 跳过）
option(LPCL_EMBED_CF_KEY "Embed CurseForge API key from .env (dev builds)" ON)
set(LPCL_CF_KEY "")
if(LPCL_EMBED_CF_KEY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.env")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../.env" _lpcl_env_lines)
    foreach(_line IN LISTS _lpcl_env_lines)
        # 兼容 LPCL_CURSEFORGE_API_KEY= 和 API_KEY= 两种写法
        if(_line MATCHES "^(LPCL_CURSEFORGE_API_KEY|API_KEY)=(.*)$")
            set(LPCL_CF_KEY "${CMAKE_MATCH_2}")
        endif()
    endforeach()
    # 去掉可能的首尾引号和 C 字符串转义
    string(REGEX REPLACE "^[\"'](.*)[\"']$" "\\1" LPCL_CF_KEY "${LPCL_CF_KEY}")
    string(REPLACE "\\" "\\\\" LPCL_CF_KEY "${LPCL_CF_KEY}")
    string(REPLACE "\"" "\\\"" LPCL_CF_KEY "${LPCL_CF_KEY}")
endif()
# 始终生成头文件（禁用时为空串），源码无需条件编译
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cf_key_embedded.h"
     "#pragma once\n#define LPCL_CF_API_KEY_EMBEDDED \"${LPCL_CF_KEY}\"\n")
target_include_directories(lpclcore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(LPCL_CF_KEY)
    message(STATUS "LPCL: CurseForge API key embedded from .env")
endif()

# Mark this build as the library itself (for export/import macros)
target_compile_definitions(lpclcore PRIVATE LPCLCORE_LIBRARY)

# Default to hidden visibility, only export marked symbols
set_target_properties(lpclcore PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(lpclcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Propagate nlohmann-json include path from parent build
if(TARGET nlohmann_json::nlohmann_json)
    get_target_property(NLOHMANN_INC nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
    if(NLOHMANN_INC)
        target_include_directories(lpclcore PUBLIC ${NLOHMANN_INC})
    endif()
endif()

target_link_libraries(lpclcore PUBLIC
    Qt6::Core
    Qt6::Network
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

# file_utils.cpp 的 iconv（zip 中文文件名解码）：Linux 在 glibc 内，
# macOS 需显式链接系统 libiconv；Windows 走另一实现（Q_OS_WIN 不用 iconv）
if(APPLE)
    target_link_libraries(lpclcore PUBLIC iconv)
endif()

# .tar.xz 解压（自更新包）：liblzma 系统库，macOS/Linux 发行版自带
find_library(LZMA_LIBRARY lzma)
if(LZMA_LIBRARY)
    target_link_libraries(lpclcore PUBLIC ${LZMA_LIBRARY})
else()
    message(WARNING "liblzma 未找到——.tar.xz 解压不可用（update 会解包失败）")
endif()

# C++20
target_compile_features(lpclcore PUBLIC cxx_std_20)

# Version info (mirrors top-level CMakeLists.txt git injection)
target_compile_definitions(lpclcore PRIVATE
    GIT_DESCRIBE="${GIT_DESCRIBE}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

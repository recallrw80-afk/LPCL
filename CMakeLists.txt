cmake_minimum_required(VERSION 3.16)

project(LPCL VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

# Qt 6.8+ 安装路径。
# QtCreator 会通过 kit 变量 %{Qt:QT_INSTALL_PREFIX} 注入 CMAKE_PREFIX_PATH，
# 若 kit 未正确配置则可能解析为空，导致 find_package 失败。
# 这里检测到空值或未定义时自动回退到正确的 Qt 6.8.3 路径。
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.8.3/gcc_64")
endif()
message(STATUS "Using Qt prefix: ${CMAKE_PREFIX_PATH}")

find_package(Qt6 REQUIRED COMPONENTS Quick Network Concurrent)
find_package(ZLIB REQUIRED)  # file_utils.cpp uses zlib for raw DEFLATE (jar/native extraction)

qt_standard_project_setup(REQUIRES 6.8)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

# nlohmann-json header-only library (system package preferred)
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

# ---- Git version info (mirrors Windows PCL's CommitHash injection) ----
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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

# Source files
set(CORE_SOURCES
    src/core/settings.cpp
    src/core/javamanager.cpp
    src/core/versionmanager.cpp
    src/core/launchbuilder.cpp
    src/core/launcher.cpp
    src/core/download/downloadmanager.cpp
    src/core/download/assetdownloader.cpp
    src/core/auth/offlineauth.cpp
    src/core/auth/msauth.cpp
    src/core/auth/authlibauth.cpp
    src/core/installer.cpp
    src/core/download/modplatform.cpp
    src/util/file_utils.cpp
    src/util/arg_utils.cpp
    src/util/crypto_utils.cpp
)

set(CORE_HEADERS
    include/core/types.h
    include/core/settings.h
    include/core/javamanager.h
    include/core/versionmanager.h
    include/core/launchbuilder.h
    include/core/launcher.h
    include/download/downloadmanager.h
    include/download/assetdownloader.h
    include/auth/authbase.h
    include/auth/offlineauth.h
    include/auth/msauth.h
    include/auth/authlibauth.h
    include/util/platform_utils.h
    include/util/file_utils.h
    include/util/arg_utils.h
    include/util/crypto_utils.h
    include/download/modplatform.h
    include/core/installer.h
)

qt_add_executable(LiunxPlainCraftLauncher
    main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Lucide icons — download on demand from GitHub
# Add icon names to LUCIDE_ICONS list (e.g. "play" "settings" "x")
# SVGs are downloaded at configure time and cached in assets/icons/lucide/
set(LUCIDE_ICONS
    "play"
    "bolt"
    "arrow-down-to-line"
    "layout-grid"
    "x"
    "minus"
    "shield-check"
    "unlink"
)
set(LUCIDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/lucide")
file(MAKE_DIRECTORY "${LUCIDE_DIR}")

function(fetch_lucide_icon icon_name)
    set(out_path "${LUCIDE_DIR}/${icon_name}.svg")
    if(NOT EXISTS "${out_path}")
        set(lucide_url "https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/${icon_name}.svg")
        message(STATUS "Downloading Lucide icon: ${icon_name}")
        file(DOWNLOAD "${lucide_url}" "${out_path}" STATUS dl_status TIMEOUT 10)
        list(GET dl_status 0 status_code)
        if(NOT status_code EQUAL 0)
            message(WARNING "Failed to download lucide:${icon_name} (HTTP ${status_code})")
            file(REMOVE "${out_path}")
            return()
        endif()
        # Rewrite stroke="currentColor" → stroke="#ffffff" for ColorOverlay tinting
        file(READ "${out_path}" svg_content)
        string(REPLACE "stroke=\"currentColor\"" "stroke=\"#ffffff\"" svg_content "${svg_content}")
        file(WRITE "${out_path}" "${svg_content}")
    endif()
endfunction()

foreach(icon ${LUCIDE_ICONS})
    fetch_lucide_icon(${icon})
endforeach()

# Icon assets — auto-collect all SVGs (relative paths required by Qt resources)
file(GLOB_RECURSE ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} assets/icons/*.svg)
list(APPEND ICON_FILES assets/icons/icon.ico)
qt_add_resources(LiunxPlainCraftLauncher "assets"
    PREFIX "/"
    FILES ${ICON_FILES}
)

target_include_directories(LiunxPlainCraftLauncher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/util
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)

target_link_libraries(LiunxPlainCraftLauncher
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Network
    PRIVATE Qt6::Concurrent
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE ZLIB::ZLIB
)

# ---- Version defines (injected at compile time, mirrors Windows PCL CI injection) ----
target_compile_definitions(LiunxPlainCraftLauncher PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    GIT_DESCRIBE="${GIT_DESCRIBE}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

# Mark Theme.qml as singleton — pragma Singleton in the file is not enough;
# CMake must be told explicitly so the generated qmldir includes "singleton Theme"
set_source_files_properties(src/ui/styles/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(LiunxPlainCraftLauncher
    URI LPCL
    VERSION 1.0
    QML_FILES
        Main.qml
        src/ui/pages/PageLaunch.qml
        src/ui/pages/PageDownload.qml
        src/ui/pages/PageSettings.qml
        src/ui/pages/PageMore.qml
        src/ui/pages/PageSelect.qml
        src/ui/pages/LoginPage.qml
        src/ui/components/LPCLButton.qml
        src/ui/components/LPCLIcon.qml
        src/ui/components/LPCLComboBox.qml
        src/ui/components/LPCLProgressBar.qml
        src/ui/components/LPCLTextBox.qml
        src/ui/components/LPCLScrollBar.qml
        src/ui/styles/Theme.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(LiunxPlainCraftLauncher PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.LPCL
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS LiunxPlainCraftLauncher
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

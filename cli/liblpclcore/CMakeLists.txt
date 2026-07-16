cmake_minimum_required(VERSION 3.16)

# ---- liblpclcore — Pure C++ Minecraft launcher core library ----
# Dependencies: Qt6::Core, Qt6::Network, nlohmann-json, ZLIB
# Intentionally does NOT depend on Qt Quick / QML / GUI.

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
    include/lpclcore/types.h
    include/lpclcore/settings.h
    include/lpclcore/javamanager.h
    include/lpclcore/versionmanager.h
    include/lpclcore/launchbuilder.h
    include/lpclcore/launcher.h
    include/lpclcore/installer.h
    include/lpclcore/authbase.h
    include/lpclcore/offlineauth.h
    include/lpclcore/msauth.h
    include/lpclcore/authlibauth.h
    include/lpclcore/downloadmanager.h
    include/lpclcore/assetdownloader.h
    include/lpclcore/modplatform.h
    include/lpclcore/arg_utils.h
    include/lpclcore/crypto_utils.h
    include/lpclcore/file_utils.h
    include/lpclcore/platform_utils.h
)

add_library(lpclcore SHARED ${LPCLCORE_SOURCES} ${LPCLCORE_HEADERS})

# Mark this build as the library itself (for export/import macros)
target_compile_definitions(lpclcore PRIVATE LPCLCORE_LIBRARY)

# Default to hidden visibility, only export marked symbols
set_target_properties(lpclcore PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(lpclcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lpclcore
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

# C++20
target_compile_features(lpclcore PUBLIC cxx_std_20)

# Version info (mirrors top-level CMakeLists.txt git injection)
target_compile_definitions(lpclcore PRIVATE
    GIT_DESCRIBE="${GIT_DESCRIBE}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

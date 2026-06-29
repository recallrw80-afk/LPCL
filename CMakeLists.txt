cmake_minimum_required(VERSION 3.16)

project(PCL_LIUNX VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network Concurrent)

qt_standard_project_setup(REQUIRES 6.8)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

# nlohmann-json header-only library
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

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
)

set(CORE_HEADERS
    src/core/types.h
    src/core/settings.h
    src/core/javamanager.h
    src/core/versionmanager.h
    src/core/launchbuilder.h
    src/core/launcher.h
    src/core/download/downloadmanager.h
    src/core/download/assetdownloader.h
    src/core/auth/authbase.h
    src/core/auth/offlineauth.h
    src/core/auth/msauth.h
    src/core/auth/authlibauth.h
)

qt_add_executable(appPCL_LIUNX
    main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(appPCL_LIUNX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
)

target_link_libraries(appPCL_LIUNX
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Network
    PRIVATE Qt6::Concurrent
    PRIVATE nlohmann_json::nlohmann_json
)

qt_add_qml_module(appPCL_LIUNX
    URI PCL_LIUNX
    QML_FILES
        Main.qml
        src/ui/pages/PageLaunchLeft.qml
        src/ui/pages/PageLaunchRight.qml
        src/ui/pages/PageSelectLeft.qml
        src/ui/pages/LoginPage.qml
        src/ui/pages/SettingsPage.qml
        src/ui/components/MyButton.qml
        src/ui/components/MyComboBox.qml
        src/ui/components/MyProgressBar.qml
        src/ui/components/MyTextBox.qml
        src/ui/components/MyScrollBar.qml
        src/ui/styles/Theme.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appPCL_LIUNX PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appPCL_LIUNX
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS appPCL_LIUNX
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

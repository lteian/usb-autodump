cmake_minimum_required(VERSION 3.16)
project(usb-autodump LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Sql)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Output to dist/YYYY-MM-DD/ folder
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist/${BUILD_DATE}")
file(MAKE_DIRECTORY "${DIST_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DIST_DIR}")

# Main executable
set(APP_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/usb_card.cpp
    src/log_panel.cpp
    src/settings_dialog.cpp
    src/password_dialog.cpp
    src/usb_monitor.cpp
    src/disk_tool.cpp
    src/file_record.cpp
    src/crypto.cpp
    src/config.cpp
    src/upload_queue.cpp
    src/dump_process.cpp
    src/ftp_process.cpp
    src/ftp_pool.cpp
    src/disk_ring.cpp
    src/disk_space_widget.cpp
)

if(WIN32)
    list(APPEND APP_SOURCES src/win32_icon.rc)
endif()

if(WIN32)
    add_executable(usb-autodump WIN32 ${APP_SOURCES})
else()
    add_executable(usb-autodump ${APP_SOURCES})
endif()

target_link_libraries(usb-autodump
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
)

if(WIN32 AND MSVC)
    target_link_libraries(usb-autodump bcrypt)
endif()

set_target_properties(usb-autodump PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

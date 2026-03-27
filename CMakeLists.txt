cmake_minimum_required(VERSION 3.16)
project(usb-autodump LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Sql)

include_directories(${CMAKE_SOURCE_DIR}/src)

# Main executable
set(MAIN_SOURCES
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
)

add_executable(usb-autodump WIN32
    ${MAIN_SOURCES}
)

target_link_libraries(usb-autodump
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
)

# Windows-specific: link against BCrypt
if(WIN32)
    target_link_libraries(usb-autodump bcrypt)
endif()

set_target_properties(usb-autodump PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

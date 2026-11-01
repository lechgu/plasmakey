cmake_minimum_required(VERSION 3.22)
project(plasmakey VERSION 0.1.0 LANGUAGES CXX)

# The top-level Makefile is hand-written; an in-source CMake build would overwrite it.
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Run `make` (builds in ./build) or `cmake -B build`.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GNUInstallDirs)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Test)
qt_policy(SET QTP0001 NEW) # QML modules under :/qt/qml/ (Qt 6.5+ default)

# XCB + XCB-XKB (libxcb-xkb-dev). Found directly, so pkg-config is not needed.
find_path(XCB_XKB_INCLUDE_DIR xcb/xkb.h REQUIRED)
find_library(XCB_LIBRARY xcb REQUIRED)
find_library(XCB_XKB_LIBRARY xcb-xkb REQUIRED)

# The plasmoid id has one source of truth: package/metadata.json.
file(READ ${PROJECT_SOURCE_DIR}/package/metadata.json PLASMAKEY_METADATA)
string(JSON PLASMOID_ID GET "${PLASMAKEY_METADATA}" KPlugin Id)

set(PLASMAKEY_QMLDIR "${CMAKE_INSTALL_LIBDIR}/qml" CACHE PATH "Where the plasmakey.xkb QML module is installed")
set(PLASMAKEY_PLASMOIDDIR "${CMAKE_INSTALL_DATADIR}/plasma/plasmoids" CACHE PATH "Where the plasmoid package is installed")
option(PLASMAKEY_BUILD_TESTS "Build unit and live tests" ON)

add_subdirectory(src)

install(DIRECTORY package/ DESTINATION ${PLASMAKEY_PLASMOIDDIR}/${PLASMOID_ID})

if(PLASMAKEY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Plasmoid id:        ${PLASMOID_ID}")
message(STATUS "QML module dir:     ${PLASMAKEY_QMLDIR}")
message(STATUS "Plasmoid dir:       ${PLASMAKEY_PLASMOIDDIR}")

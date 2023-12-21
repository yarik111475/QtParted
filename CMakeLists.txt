cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME QtParted)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PARTED_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/parted/include)
set(PARTED_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/parted/lib)

file(GLOB_RECURSE SOURCES CMAKE_CONFIGURE_DEPENDS
    "*.h"
    "*.cpp"
)

find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PARTED_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    #${PARTED_LIBRARY_DIR}/libparted.so.2.0.1
    #${PARTED_LIBRARY_DIR}/libparted-fs-resize.so.0.0.1
    parted
    blkid
    ext2fs
    -static-libgcc
    -static-libstdc++
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

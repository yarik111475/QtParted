cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME QtParted)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

find_package(Qt5 COMPONENTS Core REQUIRED)

add_executable(${PROJECT_NAME}
  ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PARTED_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    parted
    blkid
    ext2fs
    -static-libgcc
    -static-libstdc++
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

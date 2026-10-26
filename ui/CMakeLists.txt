cmake_minimum_required(VERSION 3.16)

project(CodeVaultUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ../src/common/types.h
    ../src/problem/problem_library.cpp
    ../src/problem/problem_library.h
    ../src/compiler/compiler_checker.cpp
    ../src/compiler/compiler_checker.h
)

qt_add_executable(CodeVaultUI
    ${PROJECT_SOURCES}
)

target_include_directories(CodeVaultUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(CodeVaultUI PRIVATE Qt6::Widgets)

set_target_properties(CodeVaultUI PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CodeVaultUI
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

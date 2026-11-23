cmake_minimum_required(VERSION 3.16)
project(SessionIsolationRegression LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(REPO_ABSOLUTE "${REPO}")
add_executable(SessionIsolationCheck ../session_test.cpp
    ${REPO}/ui/mainwindow.cpp ${REPO}/ui/mainwindow.h ${REPO}/ui/mainwindow.ui
    ${REPO}/ui/logindialog.cpp ${REPO}/ui/logindialog.h ${REPO}/ui/logindialog.ui ${REPO}/ui/login.qrc
    ${REPO}/ui/filterdialog.cpp ${REPO}/ui/filterdialog.h ${REPO}/ui/filterdialog.ui
    ${REPO}/src/account/account_manager.cpp ${REPO}/src/progress/progress_store.cpp
    ${REPO}/src/problem/problem_library.cpp ${REPO}/src/compiler/compiler_checker.cpp
    ${REPO}/src/judge/judge_engine.cpp ${REPO}/src/judge/output_judge.cpp)
target_include_directories(SessionIsolationCheck PRIVATE ${REPO}/src ${REPO}/ui)
target_compile_definitions(SessionIsolationCheck PRIVATE CODEVAULT_SOURCE_ROOT="${REPO_ABSOLUTE}")
target_link_libraries(SessionIsolationCheck PRIVATE Qt6::Widgets Qt6::Network Qt6::Test)
set_target_properties(SessionIsolationCheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/session-regression")

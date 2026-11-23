cmake_minimum_required(VERSION 3.16)
project(ProblemLibraryCheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core)
set(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "CodeVault source root")
get_filename_component(REPO "${REPO}" ABSOLUTE)
add_executable(ProblemLibraryCheck test.cpp
    ${REPO}/src/problem/problem_library.cpp
    ${REPO}/src/progress/progress_store.cpp)
target_include_directories(ProblemLibraryCheck PRIVATE ${REPO}/src)
target_link_libraries(ProblemLibraryCheck PRIVATE Qt6::Core)
enable_testing()
add_test(NAME ProblemLibraryCheck COMMAND ProblemLibraryCheck)
find_package(Qt6 REQUIRED COMPONENTS Test Network)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_executable(ProblemJudgeIntegration integration.cpp
    ${REPO}/src/account/account_manager.cpp
    ${REPO}/src/problem/problem_library.cpp
    ${REPO}/src/progress/progress_store.cpp
    ${REPO}/src/judge/judge_engine.cpp
    ${REPO}/src/judge/output_judge.cpp)
target_include_directories(ProblemJudgeIntegration PRIVATE ${REPO}/src)
get_filename_component(REPO_ABSOLUTE "${REPO}" ABSOLUTE)
target_compile_definitions(ProblemJudgeIntegration PRIVATE CODEVAULT_SOURCE_ROOT="${REPO_ABSOLUTE}")
target_link_libraries(ProblemJudgeIntegration PRIVATE Qt6::Core Qt6::Test Qt6::Network)

find_package(Qt6 REQUIRED COMPONENTS Network Widgets)
add_executable(AccountCheck account_test.cpp ${REPO}/src/account/account_manager.cpp ${REPO}/src/progress/progress_store.cpp)
target_include_directories(AccountCheck PRIVATE ${REPO}/src)
target_link_libraries(AccountCheck PRIVATE Qt6::Core Qt6::Network)
add_test(NAME AccountCheck COMMAND AccountCheck)
set(CMAKE_AUTOUIC ON)
add_executable(LoginDialogCheck login_test.cpp ${REPO}/ui/logindialog.cpp ${REPO}/ui/logindialog.h ${REPO}/ui/logindialog.ui ${REPO}/ui/login.qrc)
target_include_directories(LoginDialogCheck PRIVATE ${REPO}/ui)
target_link_libraries(LoginDialogCheck PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME LoginDialogCheck COMMAND LoginDialogCheck)
set_tests_properties(LoginDialogCheck PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

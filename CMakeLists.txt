cmake_minimum_required(VERSION 3.16)
project(TaskPlannerWeb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# PostgreSQL
find_package(PostgreSQL REQUIRED)

# OpenSSL с компонентами SSL и Crypto
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

# Исполняемый файл
add_executable(task_planner_web
    src/main_web.cpp
    src/Database.cpp
    src/Task.cpp
    src/TaskManager.cpp
    src/UserManager.cpp
    src/JwtUtils.cpp
)

# Подключаем заголовочные файлы
target_include_directories(task_planner_web PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/jwt-cpp/include
)

# Линкуем библиотеки (используем цели OpenSSL)
target_link_libraries(task_planner_web
    ${PostgreSQL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)
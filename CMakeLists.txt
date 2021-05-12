cmake_minimum_required(VERSION 3.19)
project(absmServer)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/include/mysql)

add_executable(absmServer main.cpp config.h config.cpp webserver.cpp webserver.h threadpool/threadpool.h http/http_conn.cpp http/http_conn.h lock/locker.h CGImysql/sql_connection_pool.cpp CGImysql/sql_connection_pool.h timer/list_timer.cpp timer/list_timer.h)


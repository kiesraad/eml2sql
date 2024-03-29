cmake_minimum_required(VERSION 3.1)

project(emlconv VERSION 1.0
                  DESCRIPTION "convert eml to sqlite"
                  LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "-Wall -g -Wextra -O3")
include_directories(ext/pugixml-1.13/src/ ext/cpp-httplib-0.12.6/ ext/nlohmann/)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(emlconv emlconv.cc sqlwriter.cc ext/pugixml-1.13/src/pugixml.cpp)
target_link_libraries(emlconv pthread  dl sqlite3 )

add_executable(emlserv emlserv.cc sqlwriter.cc  ext/pugixml-1.13/src/pugixml.cpp jsonhelper.cc)
target_link_libraries(emlserv pthread dl sqlite3 z )


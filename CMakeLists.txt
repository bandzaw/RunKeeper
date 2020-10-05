cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(RunKeeper VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#find_package(Boost COMPONENTS system REQUIRED)


add_subdirectory(external)
add_subdirectory(libs)
add_subdirectory(apps)
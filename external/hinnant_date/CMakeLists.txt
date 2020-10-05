add_library(date tz.cpp)
target_include_directories(date INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>)

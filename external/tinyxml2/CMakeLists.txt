add_library(tinyxml2 tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>)

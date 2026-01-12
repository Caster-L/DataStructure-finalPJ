cmake_minimum_required(VERSION 3.31)
#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR})
project(DS_PJ)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(ext/SFML)

add_executable(DS_PJ main.cpp)

file(GLOB_RECURSE SOURCES src/*.cpp)
target_sources(DS_PJ PRIVATE ${SOURCES})

target_include_directories(DS_PJ PRIVATE 
    ${CMAKE_SOURCE_DIR}/ext/SFML/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ext/json/single_include
)

target_link_libraries(DS_PJ PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)
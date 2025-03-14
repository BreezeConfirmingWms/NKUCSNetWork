cmake_minimum_required(VERSION 3.21)
project(ChatAppServer)

set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)


#add_library(ChatAppServer src/TcpServer.cpp)
aux_source_directory(./src DIR_SRCS)
add_executable(ChatAppServer ${DIR_SRCS})
#target_sources(ChatAppServer PRIVATE TcpServer.cpp)

target_link_libraries(ChatAppServer wsock32 ws2_32)

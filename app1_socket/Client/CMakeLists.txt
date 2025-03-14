cmake_minimum_required(VERSION 3.21)
project(ChatAppClient)

set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)



aux_source_directory(./src DIR_SRCS)
add_executable(ChatAppClient ${DIR_SRCS})


target_link_libraries(ChatAppClient wsock32 ws2_32)

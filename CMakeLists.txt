cmake_minimum_required(VERSION 4.1)
project(MintText)

set(CMAKE_CXX_STANDARD 17)

add_executable(MintText
        common.h
        server.cpp
        client.cpp)

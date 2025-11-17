cmake_minimum_required(VERSION 3.13)

project(dns-server-starter-cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

# Remove client.cpp from the list
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/client.cpp")
list(REMOVE_ITEM SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/reference.cpp")

add_executable(dns-server ${SOURCE_FILES})

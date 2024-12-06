# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.0)

# Set the project name
project(LowLatencyApp)

# Set CMake to compile in cpp20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_VERBOSE_MAKEFILE on)

# List all source (.cpp) files in a file called SOURCES
file(GLOB SOURCES "*.cpp")

# For include (.h) files search in ${PROJECT_SOURCE_DIR}
include_directories(${PROJECT_SOURCE_DIR})

# Create a library called libcommon from the list of files in SOURCES
add_library(libcommon STATIC ${SOURCES})

# Append that library to LIBS
list(APPEND LIBS libcommon)

# Append an existing library for threads (already in the system) called pthreads to LIBS
list(APPEND LIBS pthread)

# Link source file to executable with the same name
add_executable(thread_example thread_example.cpp)

# Link executable to libraries
target_link_libraries(thread_example PUBLIC ${LIBS})
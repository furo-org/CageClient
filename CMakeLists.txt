cmake_minimum_required(VERSION 3.10)
project(CageClient)

set(BUILD_CAGE_EXAMPLES OFF CACHE BOOL "Build example codes")
set(BUILD_CAGE_CLI OFF CACHE BOOL "Build simconsole tool")

set(ZMQ_LIBRARY zmq)
if (MSVC)
find_package(zeromq)
set(ZMQ_INCLUDE_DIR $ENV{ZMQ_INCLUDE_DIR})
set(ZMQ_LIBRARY_DIR $ENV{ZMQ_LIBRARY_DIR})
set(ZMQ_LIBRARY libzmq)
endif(MSVC)

# link this interface library with application code
add_library(cageClientIF INTERFACE)
target_include_directories(cageClientIF INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/json/include"
  ${ZMQ_INCLUDE_DIR}
)

target_link_libraries(cageClientIF INTERFACE
    ${ZMQ_LIBRARY}
)

if(BUILD_CAGE_CLI)
add_subdirectory(srcs)
endif()

if(BUILD_CAGE_EXAMPLES)
add_subdirectory(examples)
endif()

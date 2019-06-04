project(sim_ue)

find_package(Boost COMPONENTS system thread date_time serialization filesystem program_options REQUIRED)
find_package(SDL2)
set(ZMQ_LIBRARY zmq)
if (MSVC)
find_package(MsgPack)
find_package(zeromq)
set(ZMQ_INCLUDE_DIR $ENV{ZMQ_INCLUDE_DIR})
set(ZMQ_LIBRARY_DIR $ENV{ZMQ_LIBRARY_DIR})
set(ZMQ_LIBRARY libzmq)
endif(MSVC)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
    ${MSGPACK_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/json/include
    ${ZMQ_INCLUDE_DIR}
)

link_directories(
    ${Boost_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIR}
    ${ZMQ_LIBRARY_DIR}
    ${SDL2_LIBDIR}
)

if(MSVC)
set(SHM_LIBRARIES
    ${Boost_LIBRARIES}
    libzmq
)
else(MSVC)
set(SHM_LIBRARIES
    ${Boost_LIBRARIES}
    zmq
)
endif(MSVC)

add_executable(simconsole simConsole.cpp)
add_executable(sampleSubscriber sampleSubscriber.cc)
add_executable(sampleRun sampleRun.cc)

target_link_libraries(simconsole Boost::program_options ${ZMQ_LIBRARY})
target_link_libraries(sampleSubscriber Boost::program_options ${ZMQ_LIBRARY})
target_link_libraries(sampleRun ${ZMQ_LIBRARY})

add_library(simIF INTERFACE)
target_include_directories(simIF INTERFACE
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/json/include"
)

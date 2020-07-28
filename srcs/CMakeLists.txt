project(CageClient_cui)

find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(simconsole simConsole.cpp)

target_link_libraries(simconsole Boost::program_options cageClientIF)

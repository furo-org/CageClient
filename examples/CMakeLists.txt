project(CageClient_examples)

find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(sampleSubscriber sampleSubscriber.cc)
add_executable(sampleRun sampleRun.cc)

target_link_libraries(sampleSubscriber Boost::program_options cageClientIF)
target_link_libraries(sampleRun cageClientIF)

cmake_minimum_required(VERSION 3.12)
project(xarpd)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package (Threads REQUIRED)


set(CMAKE_CXX_STANDARD 14)

add_executable(xarpd src/xarpd.cpp src/arp_table.cpp inc/arp_table.h src/interface_worker.cpp inc/interface_worker.h inc/types.h inc/utils.h src/utils.cpp)
add_executable(xarp src/xarp.cpp inc/utils.h src/utils.cpp)
add_executable(xifconfig src/xifconfig.cpp inc/utils.h src/utils.cpp)

target_link_libraries(xarpd Threads::Threads)
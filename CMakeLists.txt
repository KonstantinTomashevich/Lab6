cmake_minimum_required(VERSION 3.13)
project(Lab6 C)
add_subdirectory(CUtilsLocalCopy)

set(CMAKE_C_STANDARD 11)
add_executable(Lab6Server server.c server_db_cache.c server_db_cache.h)
add_executable(Lab6Client client.c)

add_dependencies(Lab6Server Lab6Client)
target_link_libraries(Lab6Server CContainers)
include_directories(CUtilsLocalCopy)

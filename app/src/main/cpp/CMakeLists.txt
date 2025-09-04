cmake_minimum_required(VERSION 3.22.1)

project(unlimitedphotos)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp local_cxa_atexit_finalize_impl/atexit.cpp)

add_subdirectory(Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)

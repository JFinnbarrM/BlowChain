
cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mylib_guipipe)

FILE(GLOB mylib_sources src/*.c)
target_sources(app PRIVATE ${mylib_sources})

target_include_directories(app PRIVATE include)
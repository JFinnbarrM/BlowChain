
set(ENV{EXTRA_ZEPHYR_MODULES}
    "/home/niall/csse4011/repo/mycode/p2/t1/mylib_clock"
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mylib_clock-tapp)

FILE(GLOB tapp_sources src/*.c)
target_sources(app PRIVATE ${tapp_sources})

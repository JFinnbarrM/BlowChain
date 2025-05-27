
set(ENV{EXTRA_ZEPHYR_MODULES}
    "/home/niall/csse4011/repo/mycode/p2/t2/mylib_sensors;/home/niall/csse4011/repo/mycode/p2/t2/mylib_sensors"
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mylib_shell-tapp)

FILE(GLOB tapp_sources src/*.c)
target_sources(app PRIVATE ${tapp_sources})

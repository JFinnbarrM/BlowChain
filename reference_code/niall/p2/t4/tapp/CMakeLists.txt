
set(ENV{EXTRA_ZEPHYR_MODULES}
    "/home/niall/csse4011/repo/mycode/p2/t1/mylib_clock;/home/niall/csse4011/repo/mycode/p2/t2/mylib_sensors;/home/niall/csse4011/repo/mycode/p2/t3/mylib_shell;/home/niall/csse4011/repo/mycode/p2/t4/mylib_sampling;/home/niall/csse4011/repo/mycode/p2/t5/mylib_logfiles;/home/niall/csse4011/repo/mycode/p2/t7/mylib_guipipe"
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mylib_shell-tapp)

FILE(GLOB tapp_sources src/*.c)
target_sources(app PRIVATE ${tapp_sources})

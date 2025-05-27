# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(base)

target_sources(app PRIVATE
    src/main.c
    src/broadcaster.c
    src/controller.c
    #src/observer.c # Only remove for disco board which cannot simultaneous broadcast and observe
    src/shell.c
)

target_include_directories(app PRIVATE include)
cmake_minimum_required(VERSION 4.3)
project(vkc_bootstrap C)

set(CMAKE_C_STANDARD 17)

add_library(vkc_bootstrap STATIC library.c)

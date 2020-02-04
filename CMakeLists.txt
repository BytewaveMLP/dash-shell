cmake_minimum_required(VERSION 3.12)
project(Project1 C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "-Wall -Wpedantic -Wextra -Werror")

add_executable(Project1 main.c)
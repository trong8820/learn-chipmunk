cmake_minimum_required(VERSION 3.21)

project(learn-chipmunk VERSION 1.0.0)

set(BUILD_SHARED OFF)
set(BUILD_STATIC ON)
set(INSTALL_STATIC OFF)

add_subdirectory(src)

add_subdirectory(demo)

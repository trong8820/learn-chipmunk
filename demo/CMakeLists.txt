cmake_minimum_required(VERSION 3.21)

project(demo VERSION 1.0.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 3rdparty include
include_directories (
	${CMAKE_SOURCE_DIR}/3rdparty/glad-0.1.36/include
	${CMAKE_SOURCE_DIR}/3rdparty/glfw-3.3.8/include
)

# 3rdparty libs
link_directories(
	${CMAKE_SOURCE_DIR}/3rdparty/glad-0.1.36/lib
	${CMAKE_SOURCE_DIR}/3rdparty/glfw-3.3.8/lib
)

# 3rdparty link
link_libraries(glad glfw3 chipmunk_static)

# Projects
include_directories(
	 ${CMAKE_SOURCE_DIR}/include
)

add_executable(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

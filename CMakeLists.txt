cmake_minimum_required(VERSION 3.10)
project(Py_ParallelGC)

option(DEBUG on)
if(DEBUG)
    message("DEBUG ON COMPILE MODE")

    # add_definitions(-DPy_BUILD_CORE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fPIC -D Py_BUILD_CORE -D DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O0 -fPIC")
else()
    message("DEBUG OFF COMPILE MODE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3 -fPIC -D Py_BUILD_CORE ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O3 -fPIC")
endif(DEBUG)

# python
include_directories(/usr/local/python3.7/include/python3.7m)
link_directories(/usr/local/python3.7/lib)

#glib
find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})



file(GLOB CPP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
        )



add_library(Py_ParallelGC SHARED ${CPP_SOURCES})
target_link_libraries(Py_ParallelGC libpython3.7m.a)
target_link_libraries(Py_ParallelGC ${GLIB_LIBRARIES})

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

cmake_minimum_required(VERSION 3.5)
project(longhair)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 11)

set(LIB_SOURCE_FILES
        cauchy_256.cpp
        cauchy_256.h
        gf256.cpp
        gf256.h
        )

set(UNIT_TEST_SOURCE_FILES
        tests/cauchy_256_tests.cpp
        tests/SiameseTools.cpp
        tests/SiameseTools.h
        )

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
else()
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -march=native")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
endif()

include_directories(.)

add_library(longhair STATIC ${LIB_SOURCE_FILES})

add_executable(longhair_test ${UNIT_TEST_SOURCE_FILES})
target_link_libraries(longhair_test longhair)

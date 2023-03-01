cmake_minimum_required(VERSION 3.16)

#
# Build code into library to verify builds
#
set(SOURCES main.cpp) 
set(TARGET timestamp_rdtsc.tsk)
add_executable(${TARGET} ${SOURCES})
target_include_directories(${TARGET} PUBLIC .)

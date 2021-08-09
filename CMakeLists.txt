cmake_minimum_required(VERSION 3.5)

project(simper)

############################################################
# Create a library
############################################################

#Generate the shared library from the library sources
add_library(simper SHARED 
    src/simper.c
    src/uni.c
    src/fts5_unicode2.c
)

target_include_directories(simper
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)

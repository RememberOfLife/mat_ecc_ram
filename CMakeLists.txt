cmake_minimum_required(VERSION 3.18)
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)

project(ecc_memory C CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(INCLUDES
    lib
    
    src
)

set(SOURCES
    lib/bch_codec/bch_codec.c

    src/ecc/bch.cpp
    src/ecc/hamming.cpp
    src/ecc/hsiao.cpp

    src/util/noise.c

    src/main.cpp
)

add_executable(ecc_memory "${SOURCES}")

target_compile_options(ecc_memory PRIVATE
    # "-Wall"
    # "-Wextra"
    # "-Werror" # warnings as errors
    "-Wfatal-errors" # stop after first error
)

target_include_directories(ecc_memory PRIVATE ${INCLUDES})

target_link_libraries(ecc_memory Threads::Threads)

set_target_properties(ecc_memory PROPERTIES EXPORT_COMPILE_COMMANDS true)

cmake_minimum_required(VERSION 3.16)
project(QuantumBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -funroll-loops -DNDEBUG")

add_executable(quantumbook
    main.cpp
    OrderBook.cpp
)

target_compile_options(quantumbook PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-g -fsanitize=thread>   # catch data races in dev
)

target_link_options(quantumbook PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=thread>
)
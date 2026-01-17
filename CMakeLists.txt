cmake_minimum_required(VERSION 3.16)
project(concurrent_market_data_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(market_data_engine
    src/main.cpp
    src/metrics.cpp
)

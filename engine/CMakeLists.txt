cmake_minimum_required(VERSION 3.10)
project(MarketDataEngine)

set(CMAKE_CXX_STANDARD 20)

# Tell CMake to look in our include folder
include_directories(include)

# Automatically find all .cpp files in the src directory
file(GLOB SOURCES "src/*.cpp")

# Create the executable using all found sources
add_executable(market_data_engine ${SOURCES})

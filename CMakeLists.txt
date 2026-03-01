cmake_minimum_required(VERSION 3.10)

project(OrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# The profiling build (Fast, but keeps breadcrumbs for Flamegraphs)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer")

set(SOURCES
    main.cpp
    orderbook.cpp
)

# Create the executable first
add_executable(engine ${SOURCES})

# --- DEPENDENCY LINKING ---

# 1. Find the system thread library (Required for Boost.Asio/Threads)
find_package(Threads REQUIRED)

# 2. Find Boost and the specific compiled components
find_package(Boost REQUIRED COMPONENTS system thread)

# 3. Link them privately to your engine target
target_link_libraries(engine 
    PRIVATE 
    Boost::system 
    Boost::thread 
    Threads::Threads
    atomic
)

include(FetchContent)
FetchContent_Declare(
   googletest
   GIT_REPOSITORY https://github.com/google/googletest.git
   GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(run_tests test_orderbook.cpp orderbook.cpp)
target_link_libraries(run_tests gtest_main Threads::Threads atomic)

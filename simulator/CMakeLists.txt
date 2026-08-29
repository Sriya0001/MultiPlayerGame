cmake_minimum_required(VERSION 3.16)
project(PlayerSimulator LANGUAGES CXX)

set(SIMULATOR_SOURCES
    src/main.cpp
    src/Simulator.cpp
    src/PlayerBot.cpp
    src/MetricsCollector.cpp
    ${CMAKE_SOURCE_DIR}/server/src/MessageFramer.cpp
)

add_executable(player_simulator ${SIMULATOR_SOURCES})

target_include_directories(player_simulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/server/include
    ${CMAKE_SOURCE_DIR}/server/third_party
)

target_compile_options(player_simulator PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

find_package(Threads REQUIRED)
target_link_libraries(player_simulator PRIVATE Threads::Threads)

install(TARGETS player_simulator DESTINATION bin)

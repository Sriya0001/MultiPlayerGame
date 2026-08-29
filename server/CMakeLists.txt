cmake_minimum_required(VERSION 3.16)
project(GameServer LANGUAGES C CXX)

set(SERVER_SOURCES
    src/main.cpp
    src/Server.cpp
    src/Logger.cpp
    src/ConnectionManager.cpp
    src/MessageFramer.cpp
    src/RequestHandler.cpp
    src/GameState.cpp
    src/ThreadPool.cpp
    src/Metrics.cpp
    src/RedisClient.cpp
    src/CacheManager.cpp
    src/DatabaseManager.cpp
    src/HttpMetricsServer.cpp
)

add_executable(game_server ${SERVER_SOURCES})

target_include_directories(game_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    /usr/include/mysql
)

target_compile_options(game_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

find_package(Threads REQUIRED)
target_link_libraries(game_server PRIVATE Threads::Threads ${CMAKE_DL_LIBS} mysqlclient)

install(TARGETS game_server DESTINATION bin)

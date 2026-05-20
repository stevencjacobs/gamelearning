cmake_minimum_required(VERSION 3.10.0)
project(TextARPG VERSION 0.1.0 LANGUAGES C CXX)

find_package(SQLite3 REQUIRED)
find_path(SODIUM_INCLUDE_DIR sodium.h)
find_library(SODIUM_LIBRARY sodium)

#if (NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
#    message(FATAL_ERROR "Could not find libsodium. Install libsodium-dev or set SODIUM_INCLUDE_DIR/SODIUM_LIBRARY.")
#endif()

add_executable(TextARPG 
main.cpp
player.cpp
attackable_entity.cpp
combat.cpp
game_server.cpp
session.cpp
player_manager.cpp
room.cpp
player_store.cpp
)

target_include_directories(TextARPG PRIVATE ${SODIUM_INCLUDE_DIR})
target_link_libraries(TextARPG PRIVATE SQLite::SQLite3 ${SODIUM_LIBRARY})


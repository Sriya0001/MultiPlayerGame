#pragma once

#include <string>
#include <cstdint>

struct SimulatorConfig {
    std::string host = "127.0.0.1";
    uint16_t    port = 7777;
    int         playerCount = 50;
    int         durationSeconds = 10;
    int         basePlayerId = 1000;
    int         actionIntervalMs = 50; // sleep between actions per player
    int         seed = 42;
    bool        debug = false;
};

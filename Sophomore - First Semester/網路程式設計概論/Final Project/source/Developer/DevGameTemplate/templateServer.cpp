#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include "EasonGS_Server.hpp"
using namespace std;

int main(int argc, char** argv) {
    GameServerConfig cfg;
    cfg.port = stol(argv[1]);
    cfg.tick_rate = 30;

    GameServerCallbacks cbs;

    cbs.on_player_join_room = [&](uint32_t player_id, uint32_t room_id, string player_name) {
    };

    cbs.on_player_create_room = [&](uint32_t player_id, uint32_t room_id, string player_name) {
    };

    cbs.on_player_leave = [&](uint32_t player_id) {
    };

    cbs.on_message = [&](uint32_t player_id, const vector<uint8_t>& msg) {
    };

    cbs.on_tick = [&]() {
    };

    return run_game_server(cfg, cbs);
}

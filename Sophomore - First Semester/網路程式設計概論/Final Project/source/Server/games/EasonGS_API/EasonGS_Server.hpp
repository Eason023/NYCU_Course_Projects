// #pragma once
#include <functional>
#include <bits/stdc++.h>
using std::string;
using std::vector;
using std::uint32_t;
using std::uint8_t;

struct GameServerConfig {
    uint16_t port;
    int tick_rate; // tps
};

struct GameServerCallbacks {
    std::function<void(uint32_t player_id, uint32_t room_id, string player_name)> on_player_join_room;
    std::function<void(uint32_t player_id, uint32_t room_id, string player_name)> on_player_create_room;
    std::function<void(uint32_t player_id)> on_player_leave;
    std::function<void(uint32_t player_id, const vector<uint8_t>& msg)> on_message;
    std::function<void()> on_tick;
};

void send_to_player(uint32_t player_id, const vector<uint8_t>& data);
int run_game_server(const GameServerConfig& cfg, const GameServerCallbacks& cbs);
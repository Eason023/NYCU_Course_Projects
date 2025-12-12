#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include "EasonGS_Client.hpp"
using namespace std;

int main(int argc, char** argv) {
    client_port = stol(argv[1]);

    on_message = [&](const vector<uint8_t>& msg) {
    };

    thread game = thread([&] { run_game_player(); });

    game.join();
    return 0;
}

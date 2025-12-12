// A simple number guessing game server using the provided EasonGS API.
//
// This server illustrates how to build a minimal CLI style game on top of
// the provided multiplayer game server API.  Players join or create a room
// via the hosting platform; once inside a room each player can send
// messages that are interpreted as guesses.  The server selects a random
// target number between 1 and 100 for each room.  When a player submits a
// guess the server compares it to the target and responds with a hint.  If
// the guess is correct the server notifies all players in the room and
// selects a new number.

#include "EasonGS_Server.hpp"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Maintain simple state for each game room: the current target number.
struct RoomState {
    int target{};
};

int main(int argc, char** argv) {
    auto trim = [](std::string& s) {
        // 找第一個不是空白的
        auto first = s.find_first_not_of(" \r\n\t");
        if (first == std::string::npos) {
            s.clear(); // 全部是空白 → 直接變成空字串
            return;
        }
        // 找最後一個不是空白的
        auto last = s.find_last_not_of(" \r\n\t");
        s = s.substr(first, last - first + 1);
    };

    // Seed the random number generator.  Using time here is sufficient for
    // demonstration purposes.
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Configure the game server.  The hosting environment will supply the
    // listening port as the first command line argument.  We do not
    // specify a fallback here because the platform is responsible for
    // passing a valid port.  The tick rate is fixed at 30 ticks per
    // second to satisfy the platform's requirements.
    GameServerConfig cfg{};
    cfg.tick_rate = 30;
    if (argc > 1) {
        // Convert the argument to a 16‑bit port number.  Use stol to
        // mirror the reference implementation; a bad argument will cause
        // a runtime error which is acceptable during development.
        cfg.port = static_cast<uint16_t>(std::stol(argv[1]));
    }
    else {
        // Without a provided port the server cannot start.  Exit early
        // with a non‑zero return code to indicate misconfiguration.
        return 1;
    }

    // Data structures to track room state and player metadata.  We need to
    // know which room each player belongs to and their display name so we
    // can broadcast messages.
    std::map<uint32_t, RoomState> rooms;
    std::unordered_map<uint32_t, uint32_t> player_to_room;
    std::unordered_map<uint32_t, std::string> player_name;

    // Helper lambda to broadcast a string message to all players in a room.
    auto broadcast_to_room = [&](uint32_t room_id, const std::string& message) {
        std::vector<uint8_t> bytes(message.begin(), message.end());
        for (const auto& pr : player_to_room) {
            if (pr.second == room_id) {
                send_to_player(pr.first, bytes);
            }
        }
    };

    GameServerCallbacks cbs{};

    // When a player creates a room the platform will invoke this callback.
    cbs.on_player_create_room = [&](uint32_t player_id, uint32_t room_id, std::string name) {
        // Store the player's room association and name.
        player_to_room[player_id] = room_id;
        player_name[player_id] = name;
        // Initialise the room's target number if this is the first time.
        RoomState& state = rooms[room_id];
        if (state.target <= 0) {
            state.target = std::rand() % 100 + 1;
        }
        // Greet the creator and instruct them how to play.
        std::string greet = "You created room " + std::to_string(room_id) + ". Guess a number between 1 and 100.";
        send_to_player(player_id, std::vector<uint8_t>(greet.begin(), greet.end()));
    };

    // When a player joins an existing room.
    cbs.on_player_join_room = [&](uint32_t player_id, uint32_t room_id, std::string name) {
        player_to_room[player_id] = room_id;
        player_name[player_id] = name;
        // If the room doesn't have a target number yet, set one.
        RoomState& state = rooms[room_id];
        if (state.target <= 0) {
            state.target = std::rand() % 100 + 1;
        }
        // Inform the new player and the rest of the room of the arrival.
        std::string greet = name + " joined the room. Guess a number between 1 and 100.";
        broadcast_to_room(room_id, greet);
    };

    // Remove player associations on leave; if the room becomes empty its
    // state persists so that new players can continue the game.
    cbs.on_player_leave = [&](uint32_t player_id) {
        auto it = player_to_room.find(player_id);
        if (it != player_to_room.end()) {
            uint32_t room_id = it->second;
            std::string name = player_name[player_id];
            broadcast_to_room(room_id, name + " left the room.");
            player_to_room.erase(it);
            player_name.erase(player_id);
        }
    };

    // Process incoming guesses.  Each message from a player is expected to
    // contain an ASCII representation of an integer guess.  Non-numeric
    // messages are ignored.
    cbs.on_message = [&](uint32_t player_id, const std::vector<uint8_t>& msg) {
        try {
            // Look up which room the player is in.
            auto it_room = player_to_room.find(player_id);
            if (it_room == player_to_room.end()) {
                return; // Unknown player or not in a room
            }
            uint32_t room_id = it_room->second;

            // Convert the message to a string.
            std::string text(msg.begin(), msg.end());

            // Trim whitespace safely
            trim(text);
            if (text.empty()) {
                // 全空白就當作沒輸入
                return;
            }

            // Attempt to parse an integer guess.
            int guess = std::stoi(text);

            RoomState& state = rooms[room_id];
            if (state.target <= 0) {
                state.target = std::rand() % 100 + 1;
            }
            std::string name = player_name[player_id];
            if (guess < state.target) {
                broadcast_to_room(
                    room_id,
                    name + " guessed " + std::to_string(guess) + ": too low."
                );
            }
            else if (guess > state.target) {
                broadcast_to_room(
                    room_id,
                    name + " guessed " + std::to_string(guess) + ": too high."
                );
            }
            else {
                broadcast_to_room(
                    room_id,
                    name + " guessed " + std::to_string(guess) + ": correct! Starting a new round."
                );
                // Reset target for a new round
                state.target = std::rand() % 100 + 1;
            }
        }
        catch (const std::exception&) {
            // 非整數或其它錯誤就忽略，不讓 exception 往外炸
            return;
        }
        catch (...) {
            // 保險一點，避免任何未知錯誤把整個 server 弄掛
            return;
        }
    };

    // We do not need to perform any logic on each tick for this simple game.
    cbs.on_tick = []() {};

    // Start the game server.  run_game_server blocks and will run until
    // the host platform signals it to stop.
    return run_game_server(cfg, cbs);
}
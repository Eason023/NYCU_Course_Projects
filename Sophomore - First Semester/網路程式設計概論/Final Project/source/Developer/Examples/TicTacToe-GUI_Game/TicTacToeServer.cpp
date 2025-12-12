#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>

#include "EasonGS_Server.hpp"

using namespace std;

//
// Text protocol (ASCII):
//   Client -> Server:
//     SYNC
//     MOVE <0..8>
//     RESET
//   Server -> Client:
//     ROLE <X|O|S>
//     STATE <9chars> <turn:X|O> <winner:X|O|D|N> <full:0|1> <start_in:int>
//       - board uses '-' for empty
//       - winner uses '-' for none
//       - start_in is seconds until next round auto-start (only meaningful after a game ends; otherwise -1)
//

namespace {
    vector<uint8_t> to_bytes(const string& s) {
        return vector<uint8_t>(s.begin(), s.end());
    }

    string trim(string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
        return s.substr(i);
    }

    struct RoomState {
        array<char, 9> board{};
        char turn = 'X';   // whose turn
        char winner = 'N'; // N=none, X/O winner, D=draw

        // IMPORTANT: Do NOT use 0 as "empty seat" sentinel. Some main servers may assign player_id=0.
        std::optional<uint32_t> x;
        std::optional<uint32_t> o;
        unordered_set<uint32_t> spectators;

        // Auto-rematch countdown (server-driven UX)
        bool rematch_counting = false;
        uint64_t rematch_start_tick = 0; // inclusive

        // Short warmup window after join/create: re-send ROLE/STATE a few times so "first packet" losses don't
        // leave a player stuck in Connecting.
        uint64_t warmup_until_tick = 0;

        bool dirty = true;

        void reset() {
            board.fill(' ');
            turn = 'X';
            winner = 'N';
            rematch_counting = false;
            rematch_start_tick = 0;
            warmup_until_tick = 0;
            dirty = true;
        }

        bool full() const { return x.has_value() && o.has_value(); }

        char role_of(uint32_t pid) const {
            if (x && pid == *x) return 'X';
            if (o && pid == *o) return 'O';
            if (spectators.count(pid)) return 'S';
            return '?';
        }

        vector<uint32_t> all_members() const {
            vector<uint32_t> v;
            if (x) v.push_back(*x);
            if (o) v.push_back(*o);
            for (auto id : spectators) v.push_back(id);
            return v;
        }

        void remove(uint32_t pid) {
            if (x && pid == *x) x.reset();
            if (o && pid == *o) o.reset();
            spectators.erase(pid);
            dirty = true;
        }

        char check_winner() const {
            const int w[8][3] = {
                {0,1,2},{3,4,5},{6,7,8},
                {0,3,6},{1,4,7},{2,5,8},
                {0,4,8},{2,4,6}
            };
            for (auto& ln : w) {
                char a = board[ln[0]];
                if (a != ' ' && a == board[ln[1]] && a == board[ln[2]]) return a;
            }
            for (char c : board) if (c == ' ') return 'N';
            return 'D';
        }
    };

    unordered_map<uint32_t, RoomState> rooms;                // room_id -> state
    unordered_map<uint32_t, uint32_t> player_to_room;        // player_id -> room_id
    uint64_t tick_counter = 0;
    int g_tick_rate = 30;

    void send_role(uint32_t pid, char role) {
        string msg;
        msg += "ROLE ";
        msg += role;
        msg += "\n";
        send_to_player(pid, to_bytes(msg));
    }

    int rematch_seconds_left(const RoomState& r) {
        if (!r.rematch_counting) return -1;
        if (tick_counter >= r.rematch_start_tick) return 0;
        uint64_t left_ticks = r.rematch_start_tick - tick_counter;
        // ceil(left_ticks / g_tick_rate)
        return (int)((left_ticks + (uint64_t)g_tick_rate - 1) / (uint64_t)g_tick_rate);
    }

    void send_state_to(uint32_t pid, const RoomState& r) {
        // Important: board must be a single token (no spaces), because the client parses with stringstream.
        string b;
        b.reserve(9);
        for (char c : r.board) b.push_back(c == ' ' ? '-' : c);

        char winner_enc = r.winner;
        if (winner_enc == 'N' || winner_enc == ' ') winner_enc = '-';
        // winner_enc: '-'=none, 'D'=draw, 'X'/'O' otherwise

        int start_in = rematch_seconds_left(r);
        string msg = "STATE " + b + " " + string(1, r.turn) + " " + string(1, winner_enc) + " " + (r.full() ? "1" : "0") + " " + to_string(start_in) + "\n";
        send_to_player(pid, to_bytes(msg));
    }

    void broadcast_room(uint32_t rid, bool force_role, bool force_state) {
        auto it = rooms.find(rid);
        if (it == rooms.end()) return;
        RoomState& r = it->second;
        for (uint32_t pid : r.all_members()) {
            char role = r.role_of(pid);
            if (force_role) send_role(pid, role);
            if (force_state) send_state_to(pid, r);
        }
        r.dirty = false;
    }

    uint32_t get_room_id(uint32_t pid) {
        auto it = player_to_room.find(pid);
        if (it != player_to_room.end()) return it->second;
        return 0;
    }
}

int main(int argc, char** argv) {
    GameServerConfig cfg;
    cfg.port = (argc >= 2 ? (uint16_t)stoul(argv[1]) : 0);
    cfg.tick_rate = 30;

    GameServerCallbacks cbs;

    auto handle_join_any = [&](uint32_t pid, uint32_t rid, const string& /*name*/) {
        RoomState& r = rooms[rid];
        // assign seat or spectator (optional-aware)
        if (r.x && pid == *r.x) {
            // already X
        }
        else if (r.o && pid == *r.o) {
            // already O
        }
        else if (!r.x) {
            r.x = pid;
            r.spectators.erase(pid);
        }
        else if (!r.o && pid != *r.x) {
            r.o = pid;
            r.spectators.erase(pid);
        }
        else {
            // room is full (or pid equals X already handled above)
            r.spectators.insert(pid);
        }

        player_to_room[pid] = rid;
        r.dirty = true;

        // Warmup re-sync for a couple seconds (covers rare race where the very first ROLE packet is lost)
        r.warmup_until_tick = tick_counter + (uint64_t)g_tick_rate * 2ULL;

        // Immediate sync (in case the client is already ready)
        send_role(pid, r.role_of(pid));
        send_state_to(pid, r);
        broadcast_room(rid, /*force_role*/true, /*force_state*/true);
    };

    cbs.on_player_create_room = [&](uint32_t pid, uint32_t rid, string name) {
        // Make sure a freshly created room starts with a clean member list.
        rooms[rid] = RoomState();
        rooms[rid].reset();
        handle_join_any(pid, rid, name);
    };

    cbs.on_player_join_room = [&](uint32_t pid, uint32_t rid, string name) {
        // If room doesn't exist yet, create a default one.
        if (rooms.find(rid) == rooms.end()) {
            rooms[rid] = RoomState();
            rooms[rid].reset();
        }
        handle_join_any(pid, rid, name);
    };

    cbs.on_player_leave = [&](uint32_t pid) {
        uint32_t rid = get_room_id(pid);
        if (rid == 0) {
            // fallback scan
            for (auto& [room_id, r] : rooms) {
                if (r.role_of(pid) != '?') { rid = room_id; break; }
            }
        }
        if (rid == 0) return;
        auto it = rooms.find(rid);
        if (it == rooms.end()) return;
        it->second.remove(pid);
        player_to_room.erase(pid);

        // If empty, reset room state (keep room object; main server may manage room list separately)
        if (!it->second.x && !it->second.o && it->second.spectators.empty()) {
            it->second.reset();
        }

        broadcast_room(rid, /*force_role*/true, /*force_state*/true);
    };

    cbs.on_message = [&](uint32_t pid, const vector<uint8_t>& msg) {
        string s(msg.begin(), msg.end());
        s = trim(s);
        if (s.empty()) return;

        uint32_t rid = get_room_id(pid);
        if (rid == 0) {
            // not in any room yet: ignore except SYNC (cannot know where to sync)
            return;
        }
        RoomState& r = rooms[rid];

        if (s == "SYNC") {
            send_role(pid, r.role_of(pid));
            send_state_to(pid, r);
            return;
        }
        // RESET is intentionally ignored in the new UX.
        // The server will auto-start next round after a short countdown when a game ends.
        if (s == "RESET") return;

        if (s.rfind("MOVE", 0) == 0) {
            // only X/O can move
            char role = r.role_of(pid);
            if (role != 'X' && role != 'O') {
                // spectator move ignored
                send_role(pid, role);
                send_state_to(pid, r);
                return;
            }
            if (!r.full()) {
                // wait for 2nd player
                send_role(pid, role);
                send_state_to(pid, r);
                return;
            }
            if (r.winner != 'N') {
                send_state_to(pid, r);
                return;
            }

            // parse index
            int idx = -1;
            {
                istringstream iss(s);
                string cmd;
                iss >> cmd >> idx;
            }
            if (idx < 0 || idx >= 9) return;
            if (r.turn != role) {
                // not your turn
                send_state_to(pid, r);
                return;
            }
            if (r.board[idx] != ' ') {
                send_state_to(pid, r);
                return;
            }

            r.board[idx] = role;
            r.winner = r.check_winner();
            if (r.winner == 'N') r.turn = (r.turn == 'X' ? 'O' : 'X');

            // If game just ended, schedule auto-rematch.
            if (r.winner != 'N') {
                r.rematch_counting = true;
                r.rematch_start_tick = tick_counter + (uint64_t)g_tick_rate * 5ULL; // 5 seconds
            }

            r.dirty = true;
            broadcast_room(rid, /*force_role*/false, /*force_state*/true);
            return;
        }
    };

    cbs.on_tick = [&]() {
        tick_counter++;

        for (auto& [rid, r] : rooms) {
            // Auto-start next round
            if (r.rematch_counting && tick_counter >= r.rematch_start_tick) {
                r.reset();
                broadcast_room(rid, /*force_role*/true, /*force_state*/true);
                continue;
            }
        }

        // Once per second:
        //  - drive countdown UI (start_in changes)
        //  - short warmup re-sync after join/create
        if (tick_counter % (uint64_t)g_tick_rate == 0) {
            for (auto& [rid, r] : rooms) {
                if (r.all_members().empty()) continue;
                if (r.rematch_counting || tick_counter <= r.warmup_until_tick) {
                    broadcast_room(rid, /*force_role*/true, /*force_state*/true);
                }
            }
        }
    };

    g_tick_rate = cfg.tick_rate;
    return run_game_server(cfg, cbs);
}

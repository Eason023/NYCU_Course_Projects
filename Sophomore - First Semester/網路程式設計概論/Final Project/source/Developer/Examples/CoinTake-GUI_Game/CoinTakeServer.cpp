#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>

#include "EasonGS_Server.hpp"

using namespace std;

//
// Coin Take (Multiplayer, 2~4 players)
// --------------------------------
// Simple turn-based multiplayer game designed to be easy to debug.
//
// Rules:
//   - Up to 4 players (P1..P4). Extra players become spectators (S).
//   - When there are at least 2 active players, a round starts.
//   - A shared pile starts with 21 coins.
//   - On your turn, TAKE 1/2/3 coins (cannot exceed remaining).
//   - Whoever takes the last coin wins the round.
//   - Server auto-starts the next round after a 5s countdown.
//   - Winner's score increments and persists while the room exists.
//
// Text protocol (ASCII lines, '\n' terminated):
//   Client -> Server:
//     SYNC
//     TAKE <1|2|3>
//
//   Server -> Client:
//     ROLE <P1|P2|P3|P4|S>
//     STATE <phase:int> <coins:int> <turn:int> <winner:int> <players:int> <start_in:int> <seats_mask:int> <s1:int> <s2:int> <s3:int> <s4:int>
//       phase: 0=WAITING (need >=2 players), 1=PLAYING, 2=ROUND_OVER
//       turn:  1..4, 0 if none
//       winner: 1..4, 0 if none
//       seats_mask bit i indicates Pi occupied (i=0..3)
//       start_in: seconds until next round starts, -1 if not counting
//

namespace {
    static constexpr int kMaxPlayers = 4;
    static constexpr int kMinPlayers = 2;
    static constexpr int kStartCoins = 21;
    static constexpr int kRematchSec = 5;

    vector<uint8_t> to_bytes(const string& s) {
        return vector<uint8_t>(s.begin(), s.end());
    }

    string trim(string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
        return s.substr(i);
    }

    enum Phase : int { WAITING = 0, PLAYING = 1, ROUND_OVER = 2 };

    struct RoomState {
        array<optional<uint32_t>, kMaxPlayers> seat; // P1..P4
        unordered_set<uint32_t> spectators;

        array<int, kMaxPlayers> score{};

        Phase phase = WAITING;
        int coins = kStartCoins;
        int turn = 0;       // seat index 0..3
        int winner = -1;     // seat index 0..3

        // Auto-rematch
        bool rematch_counting = false;
        uint64_t rematch_start_tick = 0;
        int last_winner = -1;

        // Warmup re-sync after join/create
        uint64_t warmup_until_tick = 0;

        bool dirty = true;

        int active_count() const {
            int c = 0;
            for (auto& s : seat) if (s.has_value()) c++;
            return c;
        }

        int seats_mask() const {
            int m = 0;
            for (int i = 0; i < kMaxPlayers; i++) if (seat[i]) m |= (1 << i);
            return m;
        }

        int seat_of(uint32_t pid) const {
            for (int i = 0; i < kMaxPlayers; i++) {
                if (seat[i] && *seat[i] == pid) return i;
            }
            return -1;
        }

        string role_of(uint32_t pid) const {
            int idx = seat_of(pid);
            if (idx >= 0) return string("P") + to_string(idx + 1);
            if (spectators.count(pid)) return "S";
            return "?";
        }

        vector<uint32_t> all_members() const {
            vector<uint32_t> v;
            for (int i = 0; i < kMaxPlayers; i++) if (seat[i]) v.push_back(*seat[i]);
            for (auto id : spectators) v.push_back(id);
            return v;
        }

        void reset_round_keep_scores() {
            phase = (active_count() >= kMinPlayers ? PLAYING : WAITING);
            coins = kStartCoins;
            winner = -1;
            rematch_counting = false;
            rematch_start_tick = 0;

            // Choose next turn:
            // - if we have a winner from last round, start from seat after winner
            // - otherwise first occupied
            int start = 0;
            if (last_winner >= 0) start = (last_winner + 1) % kMaxPlayers;
            turn = next_occupied_from(start);
            dirty = true;
        }

        int next_occupied_from(int start) const {
            for (int step = 0; step < kMaxPlayers; step++) {
                int i = (start + step) % kMaxPlayers;
                if (seat[i]) return i;
            }
            return 0;
        }

        void remove(uint32_t pid) {
            int idx = seat_of(pid);
            if (idx >= 0) seat[idx].reset();
            spectators.erase(pid);
            dirty = true;

            // If turn player left, advance turn.
            if (idx >= 0 && phase == PLAYING) {
                turn = next_occupied_from((idx + 1) % kMaxPlayers);
            }

            // If not enough players, go back to waiting.
            if (active_count() < kMinPlayers) {
                phase = WAITING;
                coins = kStartCoins;
                winner = -1;
                rematch_counting = false;
                rematch_start_tick = 0;
            }
        }

        bool empty() const {
            if (active_count() > 0) return false;
            return spectators.empty();
        }

        void ensure_phase_consistency() {
            int n = active_count();
            if (n < kMinPlayers) {
                phase = WAITING;
                coins = kStartCoins;
                winner = -1;
                rematch_counting = false;
                rematch_start_tick = 0;
                turn = next_occupied_from(0);
                return;
            }
            if (phase == WAITING) {
                // Start a new round automatically when enough players join.
                last_winner = -1;
                reset_round_keep_scores();
            }
        }

        // Assign seat if possible; otherwise spectator.
        void assign_or_spectate(uint32_t pid) {
            int existing = seat_of(pid);
            if (existing >= 0) {
                // already seated
                spectators.erase(pid);
                return;
            }
            if (spectators.count(pid)) {
                // already spectating
                return;
            }
            for (int i = 0; i < kMaxPlayers; i++) {
                if (!seat[i]) {
                    seat[i] = pid;
                    spectators.erase(pid);
                    dirty = true;
                    ensure_phase_consistency();
                    return;
                }
            }
            spectators.insert(pid);
            dirty = true;
        }
    };

    unordered_map<uint32_t, RoomState> rooms;         // room_id -> room state
    unordered_map<uint32_t, uint32_t> player_room;   // player_id -> room_id

    uint64_t tick_counter = 0;
    int g_tick_rate = 30;

    void send_role(uint32_t pid, const RoomState& r) {
        string role = r.role_of(pid);
        if (role == "?") role = "S"; // be permissive
        string msg = "ROLE " + role + "\n";
        send_to_player(pid, to_bytes(msg));
    }

    int rematch_seconds_left(const RoomState& r) {
        if (!r.rematch_counting) return -1;
        if (tick_counter >= r.rematch_start_tick) return 0;
        uint64_t left_ticks = r.rematch_start_tick - tick_counter;
        return (int)((left_ticks + (uint64_t)g_tick_rate - 1) / (uint64_t)g_tick_rate);
    }

    void send_state_to(uint32_t pid, const RoomState& r) {
        const int players = r.active_count();
        const int start_in = rematch_seconds_left(r);
        const int turn_out = (r.phase == PLAYING ? (r.turn + 1) : 0);
        const int win_out = (r.winner >= 0 ? (r.winner + 1) : 0);
        string msg = "STATE " +
            to_string((int)r.phase) + " " +
            to_string(r.coins) + " " +
            to_string(turn_out) + " " +
            to_string(win_out) + " " +
            to_string(players) + " " +
            to_string(start_in) + " " +
            to_string(r.seats_mask()) + " " +
            to_string(r.score[0]) + " " +
            to_string(r.score[1]) + " " +
            to_string(r.score[2]) + " " +
            to_string(r.score[3]) +
            "\n";
        send_to_player(pid, to_bytes(msg));
    }

    void broadcast_room(uint32_t rid, bool force_role, bool force_state) {
        auto it = rooms.find(rid);
        if (it == rooms.end()) return;
        RoomState& r = it->second;
        for (uint32_t pid : r.all_members()) {
            if (force_role)  send_role(pid, r);
            if (force_state) send_state_to(pid, r);
        }
        r.dirty = false;
    }

    uint32_t get_room(uint32_t pid) {
        auto it = player_room.find(pid);
        return (it == player_room.end() ? 0u : it->second);
    }
}

int main(int argc, char** argv) {
    GameServerConfig cfg;
    cfg.port = (argc >= 2 ? (uint16_t)stoul(argv[1]) : 0);
    cfg.tick_rate = 30;

    GameServerCallbacks cbs;

    auto join_common = [&](uint32_t pid, uint32_t rid, const string&) {
        RoomState& r = rooms[rid];
        r.assign_or_spectate(pid);
        player_room[pid] = rid;

        // Warmup re-sync for 2 seconds to prevent "first join" UI being stuck.
        r.warmup_until_tick = tick_counter + (uint64_t)g_tick_rate * 2ULL;

        // Immediate sync.
        send_role(pid, r);
        send_state_to(pid, r);
        broadcast_room(rid, /*force_role*/true, /*force_state*/true);
    };

    cbs.on_player_create_room = [&](uint32_t pid, uint32_t rid, string name) {
        rooms[rid] = RoomState();
        rooms[rid].reset_round_keep_scores();
        join_common(pid, rid, name);
    };

    cbs.on_player_join_room = [&](uint32_t pid, uint32_t rid, string name) {
        if (rooms.find(rid) == rooms.end()) {
            rooms[rid] = RoomState();
            rooms[rid].reset_round_keep_scores();
        }
        join_common(pid, rid, name);
    };

    cbs.on_player_leave = [&](uint32_t pid) {
        uint32_t rid = get_room(pid);
        if (rid == 0) {
            // fallback scan
            for (auto& [room_id, r] : rooms) {
                if (r.role_of(pid) != "?") { rid = room_id; break; }
            }
        }
        if (rid == 0) return;
        auto it = rooms.find(rid);
        if (it == rooms.end()) return;

        it->second.remove(pid);
        player_room.erase(pid);

        // If room empty, reset everything (including scores) to keep behavior predictable.
        if (it->second.empty()) {
            rooms[rid] = RoomState();
        }

        broadcast_room(rid, /*force_role*/true, /*force_state*/true);
    };

    cbs.on_message = [&](uint32_t pid, const vector<uint8_t>& msg) {
        string s(msg.begin(), msg.end());
        s = trim(s);
        if (s.empty()) return;

        uint32_t rid = get_room(pid);
        if (rid == 0) return;
        RoomState& r = rooms[rid];

        if (s == "SYNC") {
            r.ensure_phase_consistency();
            send_role(pid, r);
            send_state_to(pid, r);
            return;
        }

        if (s.rfind("TAKE", 0) == 0) {
            int n = 0;
            {
                istringstream iss(s);
                string cmd;
                iss >> cmd >> n;
            }
            if (n < 1 || n > 3) return;

            r.ensure_phase_consistency();

            int my_seat = r.seat_of(pid);
            if (my_seat < 0) {
                // spectator
                send_role(pid, r);
                send_state_to(pid, r);
                return;
            }

            if (r.phase != PLAYING) {
                send_state_to(pid, r);
                return;
            }

            if (my_seat != r.turn) {
                send_state_to(pid, r);
                return;
            }

            if (r.coins <= 0) {
                send_state_to(pid, r);
                return;
            }

            n = min(n, r.coins);
            r.coins -= n;

            if (r.coins <= 0) {
                r.phase = ROUND_OVER;
                r.winner = my_seat;
                r.last_winner = my_seat;
                r.score[my_seat] += 1;

                r.rematch_counting = true;
                r.rematch_start_tick = tick_counter + (uint64_t)g_tick_rate * (uint64_t)kRematchSec;
            }
            else {
                // advance turn
                r.turn = r.next_occupied_from((r.turn + 1) % kMaxPlayers);
            }

            r.dirty = true;
            broadcast_room(rid, /*force_role*/false, /*force_state*/true);
            return;
        }
    };

    cbs.on_tick = [&]() {
        tick_counter++;

        // Auto-start next round
        for (auto& [rid, r] : rooms) {
            if (r.rematch_counting && tick_counter >= r.rematch_start_tick) {
                r.rematch_counting = false;
                r.rematch_start_tick = 0;
                r.reset_round_keep_scores();
                broadcast_room(rid, /*force_role*/true, /*force_state*/true);
            }
        }

        // Once per second:
        //   - drive countdown UI (start_in changes)
        //   - warmup re-sync after join/create
        if (tick_counter % (uint64_t)g_tick_rate == 0) {
            for (auto& [rid, r] : rooms) {
                if (r.all_members().empty()) continue;
                r.ensure_phase_consistency();

                if (r.rematch_counting || tick_counter <= r.warmup_until_tick) {
                    broadcast_room(rid, /*force_role*/true, /*force_state*/true);
                }
            }
        }
    };

    g_tick_rate = cfg.tick_rate;
    return run_game_server(cfg, cbs);
}

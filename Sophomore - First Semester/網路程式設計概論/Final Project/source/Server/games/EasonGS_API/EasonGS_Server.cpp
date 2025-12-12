#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include "asio/asio.hpp"
#include "EasonGS_Server.hpp"
using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

/*
Data Type Definition:
1 JOIN_ROOM_REQUEST
2 CREATE_ROOM_REQUEST
3 LEAVE_ROOM_REQUEST
4 GAME_MESSAGE
*/

namespace {
    asio::io_context ioc;
    asio::steady_timer tick_timer(ioc);
    asio::ip::tcp::socket my_socket(ioc);

    uint32_t read_player_id;
    uint32_t read_buffer_len;
    vector<uint8_t> read_buffer;

    queue<vector<uint8_t>> write_queue;
    void do_write() {
        asio::async_write(my_socket, asio::buffer(write_queue.front().data(), write_queue.front().size()), [&](std::error_code ec, std::size_t) {
            if (!ec) {
                write_queue.pop();
                if (!write_queue.empty()) {
                    do_write();
                }
            }
        });
    }

    vector<uint8_t> uint32_to_bytes(uint32_t val) {
        vector<uint8_t> bytes(4);
        bytes[0] = (val >> 24) & 0xFF;
        bytes[1] = (val >> 16) & 0xFF;
        bytes[2] = (val >> 8) & 0xFF;
        bytes[3] = val & 0xFF;
        return bytes;
    }

    uint32_t bytes_to_uint32(const vector<uint8_t>& bytes) {
        return (static_cast<uint32_t>(bytes[0]) << 24) |
            (static_cast<uint32_t>(bytes[1]) << 16) |
            (static_cast<uint32_t>(bytes[2]) << 8) |
            static_cast<uint32_t>(bytes[3]);
    }
}

void send_to_player(uint32_t player_id, const vector<uint8_t>& data) {
    vector<uint8_t> data_to_sent(data), header(uint32_to_bytes(player_id)), header2(uint32_to_bytes(static_cast<uint32_t>(data.size())));
    header.insert(header.end(), header2.begin(), header2.end());
    data_to_sent.insert(data_to_sent.begin(), header.begin(), header.end());
    if (write_queue.empty()) {
        write_queue.push(data_to_sent);
        do_write();
    }
    else
        write_queue.push(data_to_sent);
}

int run_game_server(const GameServerConfig& cfg, const GameServerCallbacks& cbs) {
    tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), cfg.port);

    my_socket.connect(server_ep);

    function<void()> schedule_tick = [&]() {
        tick_timer.expires_after(chrono::milliseconds(1000 / cfg.tick_rate));
        tick_timer.async_wait([&](error_code ec) {
            if (!ec) {
                if (cbs.on_tick)
                    cbs.on_tick();
                schedule_tick();
            }
        });
    };

    read_buffer.resize(262144);

    function<void()> start_receive = [&]() {
        asio::async_read(my_socket, asio::buffer(read_buffer.data(), 4), [&](error_code ec, size_t) {
            if (!ec) {
                read_buffer_len = bytes_to_uint32(read_buffer);
                asio::async_read(my_socket, asio::buffer(read_buffer.data(), read_buffer_len), [&](error_code ec, size_t) {
                    if (!ec) {
                        uint8_t data_type = read_buffer[0];
                        read_player_id = bytes_to_uint32({ read_buffer[1], read_buffer[2], read_buffer[3], read_buffer[4] });
                        if (data_type == 1) {
                            if (cbs.on_player_join_room)
                                cbs.on_player_join_room(read_player_id, bytes_to_uint32({ read_buffer[5], read_buffer[6], read_buffer[7], read_buffer[8] }), string(read_buffer.begin() + 9, read_buffer.begin() + read_buffer_len));
                            start_receive();
                        }
                        else if (data_type == 2) {
                            if (cbs.on_player_create_room)
                                cbs.on_player_create_room(read_player_id, bytes_to_uint32({ read_buffer[5], read_buffer[6], read_buffer[7], read_buffer[8] }), string(read_buffer.begin() + 9, read_buffer.begin() + read_buffer_len));
                            start_receive();
                        }
                        else if (data_type == 3) {
                            if (cbs.on_player_leave)
                                cbs.on_player_leave(read_player_id);
                            start_receive();
                        }
                        else if (data_type == 4) {
                            if (cbs.on_message)
                                cbs.on_message(read_player_id, vector<uint8_t>(read_buffer.begin() + 5, read_buffer.begin() + read_buffer_len));
                            start_receive();
                        }
                    }
                });
            }
        });
    };

    schedule_tick();
    start_receive();

    ioc.run();
    return 0;
}
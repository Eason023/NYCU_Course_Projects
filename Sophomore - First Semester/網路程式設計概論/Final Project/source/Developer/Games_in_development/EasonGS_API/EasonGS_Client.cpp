#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include "asio/asio.hpp"
#include "EasonGS_Client.hpp"
using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

namespace {
    asio::io_context ioc;
    asio::ip::tcp::socket my_socket(ioc);

    uint32_t read_buffer_len;
    vector<uint8_t> read_buffer;

    mutex write_lock_;

    queue<vector<uint8_t>> write_queue;
    void do_write() {
        asio::async_write(my_socket, asio::buffer(write_queue.front().data(), write_queue.front().size()), [](std::error_code ec, std::size_t) {
            if (!ec) {
                unique_lock<mutex> lock(write_lock_);
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

uint16_t client_port;
std::function<void(const std::vector<uint8_t>&)> on_message;

void send_to_server(const vector<uint8_t>& data) {
    vector<uint8_t> data_to_send(data), header = uint32_to_bytes(static_cast<uint32_t>(data.size()));
    data_to_send.insert(data_to_send.begin(), header.begin(), header.end());
    unique_lock<mutex> lock(write_lock_);
    write_queue.push(data_to_send);
    if (write_queue.size() == 1)
        do_write();
}

int run_game_player() {
    tcp::endpoint client_ep(asio::ip::make_address("127.0.0.1"), client_port);

    my_socket.connect(client_ep);

    read_buffer.resize(262144);

    function<void()> start_receive = [&]() {
        asio::async_read(my_socket, asio::buffer(read_buffer.data(), 4), [&](std::error_code ec, size_t) {
            if (!ec) {
                read_buffer_len = bytes_to_uint32(read_buffer);
                asio::async_read(my_socket, asio::buffer(read_buffer.data(), read_buffer_len), [&](std::error_code ec, size_t) {
                    if (!ec) {
                        std::vector<uint8_t> msg(read_buffer.begin(), read_buffer.begin() + read_buffer_len);
                        if (on_message)
                            on_message(msg);
                        start_receive();
                    }
                });
            }
        });
    };

    start_receive();

    ioc.run();
    return 0;
}
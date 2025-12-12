#ifndef EASONGS_CLIENT_HPP
#define EASONGS_CLIENT_HPP
#include <vector>
#include <functional>
#include <cstdint> // 為了 uint16_t, uint8_t

extern uint16_t client_port;

extern std::function<void(const std::vector<uint8_t>&)> on_message;

void send_to_server(const std::vector<uint8_t>& data);
int run_game_player(void);

#endif
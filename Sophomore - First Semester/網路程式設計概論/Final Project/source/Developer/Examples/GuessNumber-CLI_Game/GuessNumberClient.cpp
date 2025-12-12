#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#endif

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <chrono>
#include <exception>
#include "EasonGS_Client.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    std::cout.clear();
    std::cin.clear();
#endif

    try {
        if (argc <= 1) {
            std::cerr << "[Client] Missing port argument.\n";
            std::this_thread::sleep_for(std::chrono::seconds(3));
            return 1;
        }

        // 1. 讀取 GameStore 傳進來的本機轉接 port
        client_port = static_cast<uint16_t>(std::stol(argv[1]));
        std::cout << "[Client] Starting GuessNumber client on port "
            << client_port << " ..." << std::endl;

        // 2. 處理從遊戲伺服器來的訊息
        on_message = [](const std::vector<uint8_t>& data) {
            std::string text(data.begin(), data.end());
            std::cout << text << std::endl;
        };

        // 3. 開一個 thread 跑 EasonGS 的 network loop
        std::thread game_thread([]() {
            try {
                run_game_player();
            }
            catch (const std::exception& e) {
                std::cerr << "[Client] run_game_player() exception: "
                    << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "[Client] run_game_player() unknown exception.\n";
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "\n";
        std::cout << "==================================================" << std::endl;
        std::cout << "       Welcome to the Guess Number Game!          " << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "  1. System has picked a number between 1-100.    " << std::endl;
        std::cout << "  2. Type your guess and press [Enter].           " << std::endl;
        std::cout << "  3. Wait for the hint (Too High / Too Low).      " << std::endl;
        std::cout << "  4. Have fun!                                    " << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "\n[System] Ready for input > " << std::flush;

        // 4. 主 thread 嘗試讀 stdin，有就當成猜測送出去
        std::string line;
        while (true) {
            if (!std::getline(std::cin, line)) {
                // 在 GameStore 啟動時很可能一開始就拿到 EOF
                // 不要直接結束程式，清掉錯誤狀態，稍微睡一下再重試
                if (!std::cin.good()) {
                    std::cin.clear();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            if (!line.empty()) {
                std::vector<uint8_t> bytes(line.begin(), line.end());
                send_to_server(bytes);
            }
        }

        // 理論上不會到這裡，不過保險起見
        if (game_thread.joinable()) {
            game_thread.join();
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[Client] Fatal exception in main: "
            << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }
    catch (...) {
        std::cerr << "[Client] Unknown fatal error in main.\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }
}

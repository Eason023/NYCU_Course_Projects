#include "EasonGS_Client.hpp" // 你的 Client API

// ImGui & GLFW
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "glfw/include/GLFW/glfw3.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace {
    using Clock = std::chrono::steady_clock;

    double now_sec() {
        static const auto t0 = Clock::now();
        return std::chrono::duration<double>(Clock::now() - t0).count();
    }

    vector<uint8_t> to_bytes(const string& s) {
        return vector<uint8_t>(s.begin(), s.end());
    }

    struct GameState {
        char board[9];
        char turn;          // X or O
        char winner;        // ' ' (none), X/O, D
        char my_role;       // ?, X, O, S
        bool opponent_joined;
        int  start_in;      // seconds until auto-rematch starts, -1 if not in countdown
        double last_rx;     // seconds since start, -1 = never

        std::mutex mtx;

        GameState() {
            for (int i = 0; i < 9; i++) board[i] = ' ';
            turn = 'X';
            winner = ' ';
            my_role = '?';
            opponent_joined = false;
            start_in = -1;
            last_rx = -1.0;
        }
    } g;

    void send_line(const string& s) {
        // Server side accepts raw text without \n too, but adding \n makes debugging easier.
        string msg = s;
        if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
        send_to_server(to_bytes(msg));
    }
}

static void network_thread_func() {
    try {
        run_game_player();
    }
    catch (...) {
        // ignore
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) return 1;
    client_port = static_cast<uint16_t>(stoul(argv[1]));

    // 收到遊戲 Server 訊息
    on_message = [](const vector<uint8_t>& data) {
        // Robust line reassembly: the upstream forwarder may split or coalesce payloads.
        static string rx_buf;

        lock_guard<mutex> lock(g.mtx);
        g.last_rx = now_sec();

        rx_buf.append(reinterpret_cast<const char*>(data.data()), data.size());
        if (rx_buf.size() > (1u << 20)) rx_buf.clear(); // safety

        while (true) {
            size_t pos = rx_buf.find('\n');
            if (pos == string::npos) break;
            string line = rx_buf.substr(0, pos);
            rx_buf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            stringstream ss(line);
            string cmd;
            ss >> cmd;
            if (cmd == "ROLE") {
                char r = '?';
                ss >> r;
                if (r == 'X' || r == 'O' || r == 'S') g.my_role = r;
            }
            else if (cmd == "STATE") {
                string boardStr;
                char turn_char = 'X';
                char winner_char = '-';
                char full_char = '0';
                int start_in = -1;
                ss >> boardStr >> turn_char >> winner_char >> full_char;
                if (!(ss >> start_in)) start_in = -1;

                if (boardStr.size() >= 9) {
                    for (int i = 0; i < 9; i++) {
                        g.board[i] = (boardStr[i] == '-' ? ' ' : boardStr[i]);
                    }
                }

                g.turn = (turn_char == 'O' ? 'O' : 'X');
                if (winner_char == '-' || winner_char == 'N' || winner_char == ' ') g.winner = ' ';
                else if (winner_char == 'D') g.winner = 'D';
                else if (winner_char == 'X' || winner_char == 'O') g.winner = winner_char;

                g.opponent_joined = (full_char == '1');
                g.start_in = start_in;
            }
        }
    };

    // 網路執行緒
    thread net_thread(network_thread_func);
    net_thread.detach();

    // --- ImGui 初始化 ---
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(620, 650, "Tic-Tac-Toe", NULL, NULL);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool sent_initial_sync = false;
    bool sent_second_sync = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Light-touch initial SYNC (server is mostly tick-driven).
        {
            const double t = now_sec();
            double last_rx;
            {
                lock_guard<mutex> lock(g.mtx);
                last_rx = g.last_rx;
            }
            if (!sent_initial_sync && t > 0.25) {
                send_line("SYNC");
                sent_initial_sync = true;
            }
            // If we still haven't received anything after a while, try one more time.
            if (!sent_second_sync && t > 1.25 && last_rx < 0) {
                send_line("SYNC");
                sent_second_sync = true;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Game", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);

        // snapshot state (avoid long locks)
        char my_role, current_turn, winner;
        bool ready;
        int start_in;
        char local_board[9];
        double last_rx;
        {
            lock_guard<mutex> lock(g.mtx);
            my_role = g.my_role;
            current_turn = g.turn;
            winner = g.winner;
            ready = g.opponent_joined;
            start_in = g.start_in;
            memcpy(local_board, g.board, 9);
            last_rx = g.last_rx;
        }

        ImGui::SetWindowFontScale(2.0f);

        // top status
        if (my_role == '?') {
            if (last_rx >= 0) ImGui::Text("Connected (waiting for role...)");
            else ImGui::Text("Connecting...");
        }
        else if (my_role == 'S') {
            ImGui::Text("Spectating");
        }
        else {
            ImGui::Text("You are: Player %c", my_role);
        }

        const double t = now_sec();
        if (last_rx >= 0) {
            const double dt = t - last_rx;
            // Only show when it actually matters (otherwise it distracts players who are thinking).
            if (dt > 30.0) {
                ImGui::SetWindowFontScale(1.0f);
                ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "Connection quiet: %.0fs", dt);
                if (dt > 60.0) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "(might be disconnected)");
                ImGui::SetWindowFontScale(2.0f);
            }
        }

        ImGui::Separator();

        if (my_role != '?' && my_role != 'S') {
            if (!ready) {
                ImGui::Dummy(ImVec2(0, 10));
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Waiting for another player...");
            }
            else if (winner == ' ') {
                ImGui::Text("Turn: Player %c", current_turn);
                if (my_role == current_turn)
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), ">> YOUR TURN <<");
                else
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Opponent's turn...");
            }
            else {
                if (winner == 'D')
                    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1), "DRAW GAME!");
                else
                    ImGui::TextColored(ImVec4(1, 0, 1, 1), "WINNER: Player %c", winner);

                if (start_in >= 0) {
                    ImGui::Dummy(ImVec2(0, 6));
                    if (start_in > 0) ImGui::TextColored(ImVec4(0.6f, 0.9f, 1, 1), "Game starts in %ds", start_in);
                    else ImGui::TextColored(ImVec4(0.6f, 0.9f, 1, 1), "Starting game...");
                }
            }
        }
        else if (my_role == 'S') {
            if (!ready) ImGui::TextColored(ImVec4(1, 1, 0, 1), "Waiting for players...");
            else if (winner == ' ') ImGui::Text("Turn: Player %c", current_turn);
            else if (winner == 'D') ImGui::Text("DRAW GAME!");
            else ImGui::Text("WINNER: Player %c", winner);

            if (winner != ' ' && start_in >= 0) {
                ImGui::Dummy(ImVec2(0, 6));
                if (start_in > 0) ImGui::Text("Game starts in %ds", start_in);
                else ImGui::Text("Starting game...");
            }
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));

        // board
        ImGui::Columns(3, nullptr, false);
        for (int i = 0; i < 9; i++) {
            string label;
            if (local_board[i] == ' ') label = "##" + to_string(i);
            else label = string(1, local_board[i]) + "##" + to_string(i);

            const bool can_click =
                (my_role == 'X' || my_role == 'O') &&
                ready &&
                (winner == ' ') &&
                (local_board[i] == ' ') &&
                (my_role == current_turn);

            if (!can_click) ImGui::BeginDisabled();
            if (ImGui::Button(label.c_str(), ImVec2(160, 160))) {
                send_line("MOVE " + to_string(i));
            }
            if (!can_click) ImGui::EndDisabled();

            ImGui::NextColumn();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

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

    void send_line(const string& s) {
        string msg = s;
        if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
        send_to_server(to_bytes(msg));
    }

    struct GameState {
        // ROLE: P1..P4 or S or ?
        string my_role = "?";

        // STATE
        int phase = 0;      // 0 waiting, 1 playing, 2 over
        int coins = 21;
        int turn = 0;       // 1..4, 0 none
        int winner = 0;     // 1..4, 0 none
        int players = 0;
        int start_in = -1;
        int seats_mask = 0;
        int score[4] = { 0,0,0,0 };

        double last_rx = -1.0;
        mutex mtx;
    } g;
}

static void network_thread_func() {
    try { run_game_player(); }
    catch (...) {}
}

static const char* phase_text(int ph) {
    switch (ph) {
    case 0: return "Waiting";
    case 1: return "Playing";
    case 2: return "Round Over";
    default: return "?";
    }
}

static void draw_coins(int coins) {
    // Simple visual: draw small circles in a grid
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float r = 7.0f;
    float pad = 6.0f;
    int per_row = 14;

    for (int i = 0; i < coins; i++) {
        int x = i % per_row;
        int y = i / per_row;
        ImVec2 c(p0.x + x * (2 * r + pad) + r, p0.y + y * (2 * r + pad) + r);
        dl->AddCircleFilled(c, r, IM_COL32(240, 200, 60, 255));
        dl->AddCircle(c, r, IM_COL32(140, 110, 30, 255), 0, 1.5f);
    }

    int rows = (coins + per_row - 1) / per_row;
    float h = (rows > 0 ? rows * (2 * r + pad) : (2 * r + pad));
    ImGui::Dummy(ImVec2(0, h));
}

int main(int argc, char** argv) {
    if (argc <= 1) return 1;
    client_port = static_cast<uint16_t>(stoul(argv[1]));

    on_message = [](const vector<uint8_t>& data) {
        static string rx_buf;

        lock_guard<mutex> lock(g.mtx);
        g.last_rx = now_sec();

        rx_buf.append(reinterpret_cast<const char*>(data.data()), data.size());
        if (rx_buf.size() > (1u << 20)) rx_buf.clear();

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
                string r;
                ss >> r;
                if (!r.empty()) g.my_role = r;
            }
            else if (cmd == "STATE") {
                int ph = 0, coins = 21, turn = 0, win = 0, players = 0, start_in = -1, mask = 0;
                int s1 = 0, s2 = 0, s3 = 0, s4 = 0;
                ss >> ph >> coins >> turn >> win >> players >> start_in >> mask >> s1 >> s2 >> s3 >> s4;
                g.phase = ph;
                g.coins = coins;
                g.turn = turn;
                g.winner = win;
                g.players = players;
                g.start_in = start_in;
                g.seats_mask = mask;
                g.score[0] = s1; g.score[1] = s2; g.score[2] = s3; g.score[3] = s4;
            }
        }
    };

    thread net_thread(network_thread_func);
    net_thread.detach();

    // --- ImGui init ---
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(720, 640, "Coin Take (2-4 players)", NULL, NULL);
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // One gentle SYNC shortly after start (server also warmups in tick).
        if (!sent_initial_sync && now_sec() > 0.25) {
            send_line("SYNC");
            sent_initial_sync = true;
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

        // snapshot
        string my_role;
        int ph, coins, turn, win, players, start_in, mask;
        int score[4];
        double last_rx;
        {
            lock_guard<mutex> lock(g.mtx);
            my_role = g.my_role;
            ph = g.phase;
            coins = g.coins;
            turn = g.turn;
            win = g.winner;
            players = g.players;
            start_in = g.start_in;
            mask = g.seats_mask;
            for (int i = 0; i < 4; i++) score[i] = g.score[i];
            last_rx = g.last_rx;
        }

        ImGui::SetWindowFontScale(1.8f);

        // Header
        if (my_role == "?") {
            ImGui::Text("Connecting...");
        }
        else if (my_role == "S") {
            ImGui::Text("Spectating");
        }
        else {
            ImGui::Text("You are: %s", my_role.c_str());
        }

        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("Phase: %s", phase_text(ph));

        // Only warn if it's *really* quiet.
        if (last_rx >= 0) {
            double dt = now_sec() - last_rx;
            if (dt > 30.0) {
                ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "Connection quiet: %.0fs", dt);
                if (dt > 60.0) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "(might be disconnected)");
            }
        }

        ImGui::Separator();

        // Seats and scores
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("Players (need 3+ to start):");
        for (int i = 0; i < 4; i++) {
            bool occ = (mask & (1 << i)) != 0;
            string label = string("P") + to_string(i + 1);
            if (occ) {
                ImGui::Text("  %s  [IN]   Score: %d", label.c_str(), score[i]);
            }
            else {
                ImGui::TextDisabled("  %s  [--]  Score: %d", label.c_str(), score[i]);
            }
        }

        ImGui::Separator();

        // Main state
        ImGui::SetWindowFontScale(1.4f);
        ImGui::Text("Coins remaining: %d", coins);
        ImGui::SetWindowFontScale(1.0f);
        draw_coins(coins);

        ImGui::Separator();

        // Turn + result
        ImGui::SetWindowFontScale(1.35f);
        if (ph == 0) {
            ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "Waiting for players...");
        }
        else if (ph == 1) {
            if (turn >= 1 && turn <= 4) {
                ImGui::Text("Turn: P%d", turn);
                if (my_role == (string("P") + to_string(turn))) {
                    ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), ">> YOUR TURN <<");
                }
            }
            else {
                ImGui::Text("Turn: -");
            }
        }
        else {
            if (win >= 1 && win <= 4) {
                ImGui::TextColored(ImVec4(1, 0.2f, 1, 1), "Winner: P%d", win);
            }
            else {
                ImGui::Text("Round finished");
            }
            if (start_in >= 0) {
                if (start_in > 0) ImGui::TextColored(ImVec4(0.6f, 0.9f, 1, 1), "Next round starts in %ds", start_in);
                else ImGui::TextColored(ImVec4(0.6f, 0.9f, 1, 1), "Starting...");
            }
        }

        ImGui::Separator();

        // Actions
        bool is_player = (!my_role.empty() && my_role.size() == 2 && my_role[0] == 'P');
        bool my_turn = is_player && (ph == 1) && (my_role == (string("P") + to_string(turn)));

        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("Your move:");

        auto take_btn = [&](int n) {
            bool enabled = my_turn && coins >= n;
            if (!enabled) ImGui::BeginDisabled();
            if (ImGui::Button((string("Take ") + to_string(n)).c_str(), ImVec2(160, 50))) {
                send_line("TAKE " + to_string(n));
            }
            if (!enabled) ImGui::EndDisabled();
        };

        take_btn(1);
        ImGui::SameLine();
        take_btn(2);
        ImGui::SameLine();
        take_btn(3);

        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::TextDisabled("Tip: If you see stuck UI, you can close and reopen the client. The server auto-syncs every second during countdown/warmup.");

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
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

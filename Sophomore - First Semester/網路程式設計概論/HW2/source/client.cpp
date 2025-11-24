#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "asio/asio.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "glfw/include/GLFW/glfw3.h"

using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

/*
Data Type Definition:
1 [Client] ping
2 [Server] pong

3 [Client] registration (username, account name, password)

6 [Server] Server Accept
7 [Server] Server Reject (and reason)

8 [Client] login (account, password)
9 [Client] logout

10 [Client] Create a Room
11 [Client] Leave a Room
12 ([Client] and [Server]) Join a Public Room
13 ([Client] and [Server]) Watch a Public Room Game

14 ([Client] and [Server]) invitation
15 ([Client] and [Server]) accept invitation
16 ([Client] and [Server]) declined invitation

17 [Server to DB Server] Get public rooms list.
18 [Server to DB Server] Get online players list.

19 [Server] Public rooms list.
20 [Server] Online players list.

21 [Server] Room info.
22 [Server] Room host left.

25 ([Client] and [Server]) Start Game ([Server] Port_number)

30 [Client] Game Operation (0 = left, 1 = soft drop, 2 = right, 3 = hard drop, 4 = left rotate, 5 = right rotate)

31 [Server] Game Stat (Game table, score, time...etc)

50 [Server to DB Server] UpdateUserELO (user_id, new_elo)

*/

// Compile command
// Windows: g++ client.cpp imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I asio -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -std=c++20 -lws2_32 -lmswsock -lopengl32 -lgdi32 -o client.exe
// unused -lwinmm -luser32 -lshell32 -luuid

template <class T>
class ConcurrentQueue
{
private:
    mutex m_;
    queue<T> q_;

public:
    void push(const T &v)
    {
        unique_lock<mutex> lk(m_);
        q_.push(v);
    }
    void pop()
    {
        unique_lock<mutex> lk(m_);
        if (q_.empty())
            return;
        q_.pop();
        return;
    }
    T front()
    {
        unique_lock<mutex> lk(m_);
        return q_.front();
    }
    bool empty()
    {
        unique_lock<mutex> lk(m_);
        return q_.empty();
    }
};

struct Packet
{
    uint8_t type;
    vector<uint8_t> bytes;
};

class NetClient
{
private:
    asio::io_context ioc_;
    asio::steady_timer timer_;
    asio::ip::tcp::socket socket_;
    mutex safe_lock_;
    thread io_thread_;

    atomic<bool> connected;
    vector<uint8_t> read_buf;
    asio::steady_timer ping_timer_;
    asio::steady_timer pong_timer_;

    uint32_t received_data_len;

    bool not_first_time;

    void start_heartbeat() // Heartbeat with server
    {
        ping_timer_.expires_after(1s);
        ping_timer_.async_wait([this](asio::error_code ec)
                               {
                        if (ec)
                            return;
                        vector<uint8_t> ping = {1}; // 1 = ping 2 = pong
                        send_bytes(ping);
                        start_heartbeat(); });
    }

    void receive_data() // Heartbeat with server
    {
        pong_timer_.expires_after(5s);
        pong_timer_.async_wait([this](asio::error_code ec)
                               {
                        if (!ec){
                            stop();
                        } });
        asio::async_read(socket_, asio::buffer(read_buf.data(), 4), [this](asio::error_code ec, size_t)
                         {
                            if(!ec){
                                pong_timer_.cancel();
                                received_data_len = ntohl((read_buf[0]<<24)+(read_buf[1]<<16)+(read_buf[2]<<8)+read_buf[3]);
                                if(received_data_len>0 && received_data_len<65536)
                                    asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t)
                                                    { 
                                                        uint8_t data_type = read_buf[0];
                                                        if (data_type != 2){
                                                            Packet tmp;
                                                            tmp.type = data_type;
                                                            for (int i = 1; i < received_data_len; i++)
                                                                tmp.bytes.push_back(read_buf[i]);
                                                            received_data.push(tmp);
                                                        }
                                                        receive_data();
                                                    });
                                else
                                    stop();
                            } });
    }

public:
    ConcurrentQueue<Packet> received_data;

    NetClient() : ioc_(),
                  socket_(ioc_),
                  ping_timer_(ioc_),
                  pong_timer_(ioc_),
                  timer_(ioc_)
    {
        connected = 0;
        read_buf.resize(65536);
        not_first_time = 0;
    }

    ~NetClient() { stop(); }

    bool connect(const string &server_ip, const uint16_t &port)
    {
        connected = 0;
        tcp::endpoint server_ep(asio::ip::make_address(server_ip), port);
        vector<tcp::endpoint> server_eps;
        server_eps.push_back(server_ep);
        timer_.expires_after(3s);
        timer_.async_wait([this](auto ec)
                          {
                if (!ec) { asio::error_code ig; socket_.close(ig); connected = 0;} });
        socket_.open(asio::ip::tcp::v4());
        socket_.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 0)); // 0 = let os assign
        asio::async_connect(socket_, server_eps, [this](asio::error_code ec, const tcp::endpoint &)
                            {
                            timer_.cancel();
                            if (ec)
                            {
                                connected = 0;
                                return;
                            }
                            else{ connected = 1; } });
        if (not_first_time)
            ioc_.restart();
        ioc_.run();
        if (connected)
            io_thread_ = thread([this]
                                { start_heartbeat(); receive_data(); ioc_.restart(); ioc_.run(); });
        return connected;
    }

    void stop()
    {
        unique_lock<mutex> lk(safe_lock_);
        ping_timer_.cancel();
        timer_.cancel();
        pong_timer_.cancel();
        socket_.close();
        connected = 0;
        not_first_time = 1;
        if (io_thread_.joinable() && this_thread::get_id() != io_thread_.get_id())
            io_thread_.join();
    }

    // Length-Prefixed Framing Protocol
    void send_bytes(vector<uint8_t> &bytes)
    {
        unique_lock<mutex> lk(safe_lock_);
        uint32_t len = htonl(bytes.size());
        vector<uint8_t> data_to_send;
        data_to_send.push_back((len >> 24) & 0xFF);
        data_to_send.push_back((len >> 16) & 0xFF);
        data_to_send.push_back((len >> 8) & 0xFF);
        data_to_send.push_back(len & 0xFF);
        data_to_send.insert(data_to_send.end(), bytes.begin(), bytes.end());
        asio::error_code ec;
        asio::write(socket_, asio::buffer(data_to_send.data(), data_to_send.size()), ec);
    }

    bool connection_alive()
    {
        return connected;
    }
};

class OneSecTimer
{
private:
    asio::io_context ioc_;
    asio::steady_timer timer_;
    thread io_thread_;
    void repetition()
    {
        timer_.expires_after(1s);
        timer_.async_wait([this](asio::error_code ec)
                          {
                            if (!ec){
                                ready = 1;
                                repetition();
                            } });
    }

public:
    atomic<bool> ready;
    OneSecTimer() : ioc_(), timer_(ioc_) {}
    ~OneSecTimer() { stop(); }
    void start_repetition()
    {
        ready = 0;
        io_thread_ = thread([this]
                            { repetition(); ioc_.run(); });
    }
    void stop()
    {
        timer_.cancel();
        ready = 0;
        if (io_thread_.joinable())
            io_thread_.join();
    }
};

class Room
{
public:
    uint8_t PrivatePublic, GameMode, WatchingNumber = 255;
    uint16_t room_code, HostPlayerELO, Player2ELO;
    string HostPlayerUsername, Player2Username;
};

class Invitation
{
public:
    uint8_t GameMode;
    uint16_t room_code, HostPlayerELO;
    string HostPlayerUsername;
};

class Player_info
{
public:
    uint16_t UserID, PlayerELO, GamePlayed;
    string PlayerUsername;
};

class GameTable
{
    /*
    Table Definition:
        0 = empty;
        1 = sky blue;
        2 = blue;
        3 = orange;
        4 = yellow;
        5 = green;
        6 = purple;
        7 = red;
        8 = Obstacle block
        9 ~ 15 = frame block
    */
public:
    uint8_t GameMode;
    uint16_t Player1ELO, Player2ELO;

    uint8_t GameStat; // 0~3: Ready?  4: In Game  5: P1 Win  6: P2 Win  7: Tie
    uint32_t P1_score, P2_score;
    uint8_t P1_Table[20][10], P2_Table[20][10];
    uint16_t GameTime; // (Second)

    string Player1Username, Player2Username;

    GameTable()
    {
        GameMode = 0;
        Player1ELO = Player2ELO = 0;
        GameStat = 0;
        P1_score = P2_score = 0;
        memset(P1_Table, 0, sizeof(P1_Table));
        memset(P2_Table, 0, sizeof(P2_Table));
        GameTime = 0;
    }
};

list<tuple<string, double, int>> toast_info;
void ShowToast(const string &msg, float duration = 2.0f, int type = 0)
{
    toast_info.push_back(make_tuple(msg, ImGui::GetTime() + duration, type));
}

signed main()
{
    FreeConsole();

    // GUI shading, backend: OpenGL3, GLFW
    glfwInit(); // OpenGL3,GLFW initialization
    GLFWwindow *win = glfwCreateWindow(1280, 720, "The Tetris by 113550153", 0, 0);
    glfwSetWindowSizeLimits(win, 800, 600, 3840, 2160);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // v-sync on
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330"); // OpenGL version

    const double targetFPS = 60.0;
    const double targetFrameTime = 1.0 / targetFPS;
    double lastTime = glfwGetTime();

    string init[4] = {"Hello, dear player! This is The Tetris by 113550153.\n\nInitializing......\nConnecting.......\n", "Welcome to ", "The Tetris", "Sorry, unable to connect \"The Tetris\" game server."};
    ImFont *SystemFont16 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Cousine-Regular.ttf", 16.0f);
    ImFont *SystemFont14 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Cousine-Regular.ttf", 14.0f); // ProggyClean
    ImFont *Smorufont36 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Smoru.ttf", 36.0f);
    ImFont *Smorufont72 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Smoru.ttf", 72.0f);

    ImU32 grid_color = IM_COL32(255, 255, 255, 255);
    ImU32 table_colors[9] = {
        IM_COL32(180, 180, 180, 255),
        IM_COL32(0, 220, 255, 255),
        IM_COL32(0, 100, 255, 255),
        IM_COL32(255, 180, 0, 255),
        IM_COL32(255, 255, 0, 255),
        IM_COL32(128, 255, 0, 255),
        IM_COL32(185, 0, 255, 255),
        IM_COL32(255, 30, 30, 255),
        IM_COL32(100, 100, 100, 255)};

    int ui_stat = 1;
    int ui_wait = 0;
    bool button_disable = 0;
    uint8_t room_host_or_p2 = 0;
    int room_PrivatePublic_buf = 0;
    int room_GameMode_buf = 0;

    Room MyRoomInfo;
    uint16_t MyGamePort;
    GameTable MyGameTable;

    future<bool> connect_to_server;
    NetClient net_server;
    NetClient net_game_server;
    // OneSecTimer Timer1s;

    vector<Room> room_list;
    vector<Player_info> player_list;
    list<Invitation> invitation_list;
    // room_list.push_back(Room());
    // invitation_list.push_back(Invitation());

    string account_buf, password_buf, reg_username_buf, reg_account_buf, reg_password_buf;
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        // ImGui::ShowDemoWindow();
        /*
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace"); //Set up dockspace
        ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_None);
        */

        if (ui_stat == 0)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
            ImGui::Begin("##Fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            ImGuiIO &io = ImGui::GetIO();
            ImGui::PushFont(Smorufont36);
            float text_w = ImGui::CalcTextSize(init[3].c_str()).x;
            ImVec2 pos1((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.5 - 36);
            ImGui::SetCursorPos(pos1);
            ImGui::Text("%s", init[3].c_str());
            ImGui::PopFont();
            ImGui::End();
        }
        else if (ui_stat <= 120)
        {
            // fps restrict
            double currentTime = glfwGetTime();
            double elapsed = currentTime - lastTime;
            if (elapsed < targetFrameTime)
                std::this_thread::sleep_for(chrono::duration<double>(targetFrameTime - elapsed));
            lastTime = currentTime;

            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
            ImGui::Begin("##Fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            ImGuiIO &io = ImGui::GetIO();
            ImGui::PushFont(Smorufont36);
            float text_w = ImGui::CalcTextSize("Hello, dear player! This is The Tetris by 113550153.").x;
            ImVec2 pos1((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.5 - 72);
            ImGui::SetCursorPos(pos1);
            int len = ((ui_stat * init[0].length() / 60) < init[0].length() ? (ui_stat * init[0].length() / 60) : init[0].length());
            ImGui::Text("%s", init[0].substr(0, len).c_str());
            ImGui::PopFont();
            ImGui::End();
            if (ui_stat == 1)
                connect_to_server = async(launch::async, &NetClient::connect, &net_server, "140.113.17.11", 52023);
            if (ui_stat == 120)
            {
                if (connect_to_server.wait_for(chrono::milliseconds(0)) == future_status::ready)
                {
                    if (connect_to_server.get())
                        ui_stat = 121;
                    else
                        ui_stat = 0;
                }
            }
            else
                ui_stat++;
        }
        else
        {
            if (!net_server.connection_alive())
                ui_stat = 0;
            if (ui_stat == 121) // lobby ui
            {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                ImVec2 panel_size(400, 300);
                ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f, (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
                ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                            (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(Smorufont72);
                string title = "The Tetris";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                ImGui::SetCursorPos(panel_pos);
                ImGui::BeginChild("lobby_panel", panel_size, true);
                ImGui::PushFont(SystemFont16);
                ImVec2 button_size(240, 64);
                ImVec2 pos2((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f);
                ImGui::SetCursorPos(pos2);
                if (ImGui::Button("Login", button_size))
                {
                    ui_stat = 122;
                }
                ImVec2 pos3((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f + 100);
                ImGui::SetCursorPos(pos3);
                if (ImGui::Button("Register", button_size))
                {
                    ui_stat = 123;
                }
                ImVec2 pos4((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f + 200);
                ImGui::SetCursorPos(pos4);
                if (ImGui::Button("Exit", button_size))
                {
                    vector<uint8_t> data;
                    data.push_back(9);
                    net_server.send_bytes(data);
                    ui_stat = -1;
                }
                ImGui::PopFont();
                ImGui::EndChild();
                ImGui::End();
            }
            else if (ui_stat == 122) // login ui
            {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                ImVec2 panel_size(400, 120);
                ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f,
                                 (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
                ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                            (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(Smorufont72);
                string title = "The Tetris";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                ImGui::SetCursorPos(panel_pos);
                ImGui::BeginChild("login_panel", panel_size, true);
                ImGui::PushFont(SystemFont16);
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" Account:  ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetNextItemWidth(270);
                ImGui::InputText("##Account", &account_buf);
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" Password: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetNextItemWidth(270);
                bool submit = ImGui::InputText("##Password", &password_buf, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
                int ipt_text_h = ImGui::GetItemRectSize().y;
                ImVec2 pos2((panel_size.x - 350) * 0.5f, ipt_text_h * 3.2);
                ImGui::SetCursorPos(pos2);
                if (ImGui::Button("Login", ImVec2(160, 36)) || submit)
                {
                    if (account_buf.length() <= 12 && password_buf.length() <= 12 && account_buf.length() > 0 && password_buf.length() > 0)
                    {
                        vector<uint8_t> data;
                        data.push_back(8);
                        data.push_back(account_buf.length());
                        data.push_back(password_buf.length());
                        data.insert(data.end(), account_buf.begin(), account_buf.end());
                        data.insert(data.end(), password_buf.begin(), password_buf.end());
                        net_server.send_bytes(data);
                        ui_wait = 1;
                    }
                    else
                        ShowToast("The account and password length should be between 1~12.", 4.0f, 2);
                }
                ImVec2 pos3((panel_size.x - 350) * 0.5f + 160 + 30, ipt_text_h * 3.2);
                ImGui::SetCursorPos(pos3);
                if (ImGui::Button("Cancel", ImVec2(160, 36)))
                {
                    ui_stat = 121;
                }
                ImGui::PopFont();
                ImGui::EndChild();
                if (ui_wait && !ImGui::IsPopupOpen("Verifying..."))
                    ImGui::OpenPopup("Verifying...");
                if (ui_wait)
                {
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                    if (ImGui::BeginPopupModal("Verifying...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        ImVec2 size = ImVec2(500, 30);
                        float speed = 0.8f;
                        float block_frac = 0.25f;
                        const ImGuiStyle &style = ImGui::GetStyle();

                        // 取得繪製起點（螢幕座標）
                        ImVec2 p = ImVec2(ImGui::GetCursorScreenPos().x + (590 - size.x) * 0.5, ImGui::GetCursorScreenPos().y + 200 - size.y * 0.5);
                        ImVec2 q = ImVec2(p.x + size.x, p.y + size.y);
                        // 顏色
                        ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
                        ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
                        // 畫底框
                        ImDrawList *draw = ImGui::GetWindowDrawList();
                        draw->AddRectFilled(p, q, col_bg, style.FrameRounding);
                        // 動畫（讓方塊從左外側滑進來、滑出右側）
                        float t = fmodf((float)ImGui::GetTime() * speed, 1.0f);
                        float block_w = size.x * block_frac;
                        float x0 = p.x + (size.x + block_w) * t - block_w; // 從 -block_w 到 size.x
                        ImVec2 b0(x0, p.y);
                        ImVec2 b1(x0 + block_w, q.y);
                        // 只在條內繪製
                        draw->PushClipRect(p, q, true);
                        draw->AddRectFilled(b0, b1, col_fill, style.FrameRounding);
                        draw->PopClipRect();
                        // 佔位，推進游標（用 Dummy 不會建立可互動元素）
                        ImGui::Dummy(size);
                        if (!net_server.received_data.empty())
                        {
                            if (net_server.received_data.front().type == 6)
                            {
                                net_server.received_data.pop();
                                ui_stat = 124;
                                // Timer1s.start_repetition();
                            }
                            else // if (net_server.received_data.front().type == 7)
                            {
                                vector<uint8_t> tmp = net_server.received_data.front().bytes;
                                net_server.received_data.pop();
                                string msg(tmp.begin(), tmp.end());
                                ShowToast("Server Reject: " + msg, 4, 2);
                            }
                            ui_wait = 0;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::End();
            }
            else if (ui_stat == 123) // register ui
            {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                ImVec2 panel_size(400, 144);
                ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f,
                                 (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
                ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                            (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(Smorufont72);
                string title = "The Tetris";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                ImGui::SetCursorPos(panel_pos);
                ImGui::BeginChild("register_panel", panel_size, true);
                ImGui::PushFont(SystemFont16);
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" Username: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetNextItemWidth(270);
                ImGui::InputText("##Username", &reg_username_buf);
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" Account:  ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetNextItemWidth(270);
                ImGui::InputText("##Account", &reg_account_buf);
                ImGui::AlignTextToFramePadding();
                ImGui::Text(" Password: ");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SetNextItemWidth(270);
                bool submit = ImGui::InputText("##Password", &reg_password_buf, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
                int ipt_text_h = ImGui::GetItemRectSize().y;
                ImVec2 pos2((panel_size.x - 350) * 0.5f, ipt_text_h * 4.2);
                ImGui::SetCursorPos(pos2);
                if (ImGui::Button("Register", ImVec2(160, 36)) || submit)
                {
                    bool valid = 1;
                    if (reg_username_buf.length() > 12 || reg_username_buf.length() == 0 || reg_account_buf.length() > 12 || reg_account_buf.length() == 0 || reg_password_buf.length() > 12 || reg_password_buf.length() == 0)
                        valid = 0;
                    for (int i = 0; i < reg_username_buf.length(); i++)
                        if (reg_username_buf[i] == ' ') //(!(reg_username_buf[i] >= '0' && reg_username_buf[i] <= '9'))
                            valid = 0;
                    for (int i = 0; i < reg_account_buf.length(); i++)
                        if (reg_account_buf[i] == ' ')
                            valid = 0;
                    for (int i = 0; i < reg_password_buf.length(); i++)
                        if (reg_password_buf[i] == ' ')
                            valid = 0;
                    if (valid)
                    {
                        vector<uint8_t> data;
                        data.push_back(3);
                        data.push_back(reg_username_buf.length());
                        data.push_back(reg_account_buf.length());
                        data.push_back(reg_password_buf.length());
                        data.insert(data.end(), reg_username_buf.begin(), reg_username_buf.end());
                        data.insert(data.end(), reg_account_buf.begin(), reg_account_buf.end());
                        data.insert(data.end(), reg_password_buf.begin(), reg_password_buf.end());
                        net_server.send_bytes(data);
                        ui_wait = 1;
                    }
                    else
                        ShowToast("Your Username, Account, and Password should NOT contain spaces. The length should be between 1~12.", 4, 0);
                }
                ImVec2 pos3((panel_size.x - 350) * 0.5f + 160 + 30, ipt_text_h * 4.2);
                ImGui::SetCursorPos(pos3);
                if (ImGui::Button("Cancel", ImVec2(160, 36)))
                {
                    ui_stat = 121;
                }
                ImGui::PopFont();
                ImGui::EndChild();
                if (ui_wait && !ImGui::IsPopupOpen("Verifying..."))
                    ImGui::OpenPopup("Verifying...");
                if (ui_wait)
                {
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                    if (ImGui::BeginPopupModal("Verifying...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        ImVec2 size = ImVec2(500, 30);
                        float speed = 0.8f;
                        float block_frac = 0.25f;
                        const ImGuiStyle &style = ImGui::GetStyle();

                        // 取得繪製起點（螢幕座標）
                        ImVec2 p = ImVec2(ImGui::GetCursorScreenPos().x + (590 - size.x) * 0.5, ImGui::GetCursorScreenPos().y + 200 - size.y * 0.5);
                        ImVec2 q = ImVec2(p.x + size.x, p.y + size.y);
                        // 顏色
                        ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
                        ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
                        // 畫底框
                        ImDrawList *draw = ImGui::GetWindowDrawList();
                        draw->AddRectFilled(p, q, col_bg, style.FrameRounding);
                        // 動畫（讓方塊從左外側滑進來、滑出右側）
                        float t = fmodf((float)ImGui::GetTime() * speed, 1.0f);
                        float block_w = size.x * block_frac;
                        float x0 = p.x + (size.x + block_w) * t - block_w; // 從 -block_w 到 size.x
                        ImVec2 b0(x0, p.y);
                        ImVec2 b1(x0 + block_w, q.y);
                        // 只在條內繪製
                        draw->PushClipRect(p, q, true);
                        draw->AddRectFilled(b0, b1, col_fill, style.FrameRounding);
                        draw->PopClipRect();
                        // 佔位，推進游標（用 Dummy 不會建立可互動元素）
                        ImGui::Dummy(size);
                        if (!net_server.received_data.empty())
                        {
                            if (net_server.received_data.front().type == 6)
                            {
                                net_server.received_data.pop();
                                reg_username_buf = "";
                                reg_account_buf = "";
                                reg_password_buf = "";
                                ShowToast("Account has been registered successfully.", 4, 1);
                                ui_stat = 121;
                            }
                            else // if (net_server.received_data.front().type == 7)
                            {
                                vector<uint8_t> tmp = net_server.received_data.front().bytes;
                                net_server.received_data.pop();
                                string msg(tmp.begin(), tmp.end());
                                ShowToast("Server Reject: " + msg, 4, 2);
                            }
                            ui_wait = 0;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::End();
            }
            else if (ui_stat == 124) // Game Lobby UI
            {
                /*if (Timer1s.ready)
                {
                    Timer1s.ready = 0;
                    vector<uint8_t> data;
                    data.push_back(17);
                    net_server.send_bytes(data);
                }*/
                if (!net_server.received_data.empty())
                {
                    int data_type = net_server.received_data.front().type;
                    vector<uint8_t> tmp = net_server.received_data.front().bytes;
                    net_server.received_data.pop();
                    if (data_type == 12) // join room
                    {
                        ui_stat = 125;
                        room_host_or_p2 = 1;
                        button_disable = 0;
                    }
                    else if (data_type == 13) // watch game
                    {
                        MyGamePort = tmp[0] * 256 + tmp[1];
                        ui_stat = 151;
                        MyGameTable = GameTable();
                        net_game_server.connect("140.113.17.11", MyGamePort);
                    }
                    else if (data_type == 15) // accept room game
                    {
                        ui_stat = 125;
                        room_host_or_p2 = 1;
                        button_disable = 0;
                    }
                    else if (data_type == 7)
                    {
                        string msg(tmp.begin(), tmp.end());
                        ShowToast("Server Reject: " + msg, 4, 2);
                        button_disable = 0;
                    }
                    else if (data_type == 14) // invitation
                    {
                        Invitation invitation_tmp;
                        invitation_tmp.GameMode = tmp[0];
                        invitation_tmp.room_code = tmp[1] * 256 + tmp[2];
                        invitation_tmp.HostPlayerELO = tmp[3] * 256 + tmp[4];
                        uint8_t hostlen = tmp[5];
                        string host_username(tmp.begin() + 6, tmp.begin() + 6 + hostlen);
                        invitation_tmp.HostPlayerUsername = host_username;
                        invitation_list.push_back(invitation_tmp);
                    }
                    else if (data_type == 19) // public rooms info
                    {
                        room_list.clear();
                        uint8_t data_num = tmp[0];
                        int ptr = 1;
                        for (int i = 0; i < data_num; i++)
                        {
                            Room room_tmp;
                            room_tmp.PrivatePublic = tmp[ptr + 0];
                            room_tmp.GameMode = tmp[ptr + 1];
                            room_tmp.WatchingNumber = tmp[ptr + 2];
                            room_tmp.room_code = tmp[ptr + 3] * 256 + tmp[ptr + 4];
                            room_tmp.HostPlayerELO = tmp[ptr + 5] * 256 + tmp[ptr + 6];
                            room_tmp.Player2ELO = tmp[ptr + 7] * 256 + tmp[ptr + 8];
                            uint8_t hostlen = tmp[ptr + 9], p2len = tmp[ptr + 10];
                            ptr += 11;
                            string host_username(tmp.begin() + ptr, tmp.begin() + ptr + hostlen);
                            ptr += hostlen;
                            room_tmp.HostPlayerUsername = host_username;
                            string p2_username(tmp.begin() + ptr, tmp.begin() + ptr + p2len);
                            ptr += p2len;
                            room_tmp.Player2Username = p2_username;
                            room_list.push_back(room_tmp);
                        }
                    }
                }
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                // title
                ImVec2 pos1(io.DisplaySize.x * 0.075f, io.DisplaySize.y * 0.1f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(Smorufont72);
                string title = "The Tetris";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                ImGui::PushFont(Smorufont36);
                ImVec2 pos2(pos1.x + 300, pos1.y);
                ImGui::SetCursorPos(pos2);
                string title_PublicRoomList = "Public Room List";
                ImGui::Text("%s", title_PublicRoomList.c_str());
                ImVec2 pos2_2(pos1.x, pos1.y + 200);
                ImGui::SetCursorPos(pos2_2);
                string title_ReceivedInvitation = "Received Invitation";
                ImGui::Text("%s", title_ReceivedInvitation.c_str());
                ImGui::PopFont();
                // room list
                ImGui::PushFont(SystemFont14);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 50, 255));
                ImVec2 pos3(pos1.x + 300, pos1.y + 60);
                ImGui::SetCursorPos(pos3);
                ImGui::BeginTable("Public Room List", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2((io.DisplaySize.x - (pos1.x + 300)) * 0.9, (io.DisplaySize.y - (pos1.y + 60)) * 0.9));
                ImGui::TableSetupColumn("Host Player");
                ImGui::TableSetupColumn("Host Player's ELO");
                ImGui::TableSetupColumn("Player 2");
                ImGui::TableSetupColumn("Player 2's ELO");
                ImGui::TableSetupColumn("Game Mode");
                ImGui::TableSetupColumn("Watching");
                ImGui::TableSetupColumn("Operation", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();
                for (int i = 0; i < room_list.size(); i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", room_list[i].HostPlayerUsername.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", to_string(room_list[i].HostPlayerELO).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", (room_list[i].Player2Username != "" ? room_list[i].Player2Username.c_str() : "Waiting..."));
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", (room_list[i].Player2Username != "" ? to_string(room_list[i].Player2ELO).c_str() : "N/A"));
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", (room_list[i].GameMode == 0 ? "Timed" : "Survival"));
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", (room_list[i].WatchingNumber != 255 ? to_string(room_list[i].WatchingNumber).c_str() : "N/A"));
                    ImGui::TableNextColumn();
                    ImGui::PushID(i + 10000);
                    ImGui::BeginDisabled(room_list[i].Player2Username != "" || button_disable);
                    if (ImGui::Button("Join", ImVec2(55, 0)))
                    {
                        vector<uint8_t> data;
                        data.push_back(12);
                        data.push_back(room_list[i].room_code / 256);
                        data.push_back(room_list[i].room_code % 256);
                        net_server.send_bytes(data);
                        button_disable = 1;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(room_list[i].WatchingNumber == 255 || button_disable);
                    if (ImGui::Button("Watch", ImVec2(55, 0)))
                    {
                        vector<uint8_t> data;
                        data.push_back(13);
                        data.push_back(room_list[i].room_code / 256);
                        data.push_back(room_list[i].room_code % 256);
                        net_server.send_bytes(data);
                        button_disable = 1;
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndTable();
                // invitation list
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 120, 255, 255));
                ImVec2 pos4(pos1.x, pos1.y + 250);
                ImGui::SetCursorPos(pos4);
                ImGui::BeginTable("Received Invitation", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2((300) * 0.9, (io.DisplaySize.y - (pos1.y + 250)) * 0.9 - 20));
                ImGui::TableSetupColumn("Host");
                ImGui::TableSetupColumn("ELO");
                ImGui::TableSetupColumn("Game Mode");
                ImGui::TableSetupColumn("Ops."); //, ImGuiTableColumnFlags_WidthFixed, 120.0f
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();
                int id_i = 0;
                for (auto it = invitation_list.begin(); it != invitation_list.end(); id_i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", (*it).HostPlayerUsername.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", to_string((*it).HostPlayerELO).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", ((*it).GameMode == 0 ? "Timed" : "Survival"));
                    ImGui::TableNextColumn();
                    ImGui::PushID(id_i);
                    bool push_button = 0;
                    ImGui::BeginDisabled(button_disable);
                    if (ImGui::Button("Accept", ImVec2(60, 0)))
                    {
                        vector<uint8_t> data;
                        data.push_back(15);
                        data.push_back((*it).room_code / 256);
                        data.push_back((*it).room_code % 256);
                        net_server.send_bytes(data);
                        push_button = 1;
                        button_disable = 1;
                    }
                    // ImGui::SameLine();
                    if (ImGui::Button("Decline", ImVec2(60, 0)))
                    {
                        vector<uint8_t> data;
                        data.push_back(16);
                        data.push_back((*it).room_code / 256);
                        data.push_back((*it).room_code % 256);
                        net_server.send_bytes(data);
                        push_button = 1;
                    }
                    ImGui::EndDisabled();
                    if (push_button)
                        it = invitation_list.erase(it);
                    else
                        it++;
                    ImGui::PopID();
                }
                ImGui::EndTable();
                ImGui::PopFont();
                // create room button
                ImVec2 pos5(pos1.x, pos1.y + 100);
                ImGui::SetCursorPos(pos5);
                ImGui::PushFont(Smorufont36);
                if (ImGui::Button("Create a Room", ImVec2(250, 60)))
                {
                    ui_wait = 1;
                }
                ImGui::PopFont();
                if (ui_wait && !ImGui::IsPopupOpen("Room and Game Mode Setting"))
                    ImGui::OpenPopup("Room and Game Mode Setting");
                if (ui_wait)
                {
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                    if (ImGui::BeginPopupModal("Room and Game Mode Setting", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        ImVec2 pos6(ImGui::GetCursorPos().x + (600 - 300) * 0.5, ImGui::GetCursorPos().y + 80);
                        ImGui::SetCursorPos(pos6);
                        ImGui::SetNextItemWidth(300.0f);
                        const char *PrivatePublic_names[] = {"Private", "Public"};
                        ImGui::SliderInt("##room_PrivatePublic_slider", &room_PrivatePublic_buf, 0, 1, PrivatePublic_names[room_PrivatePublic_buf]);
                        ImVec2 pos7(ImGui::GetCursorPos().x + (600 - 300) * 0.5, ImGui::GetCursorPos().y + 50);
                        ImGui::SetCursorPos(pos7);
                        ImGui::SetNextItemWidth(300.0f);
                        const char *GameMode_names[] = {"Timed", "Survival"};
                        ImGui::SliderInt("##room_GameMode_slider", &room_GameMode_buf, 0, 1, GameMode_names[room_GameMode_buf]);
                        ImVec2 pos8(ImGui::GetCursorPos().x + (600 - 300) * 0.5, ImGui::GetCursorPos().y + 50);
                        ImGui::SetCursorPos(pos8);
                        ImGui::PushFont(Smorufont36);
                        if (ImGui::Button("Confirm", ImVec2(300, 60)))
                        {
                            vector<uint8_t> data;
                            data.push_back(10); // 10 room_PrivatePublic_buf room_GameMode_buf
                            data.push_back(room_PrivatePublic_buf);
                            data.push_back(room_GameMode_buf);
                            net_server.send_bytes(data);
                            ui_stat = 125;
                            MyRoomInfo = Room();
                            MyRoomInfo.PrivatePublic = room_PrivatePublic_buf;
                            MyRoomInfo.GameMode = room_GameMode_buf;
                            room_host_or_p2 = 0;
                            ui_wait = 0;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }
                }
                ImGui::End();
            }
            else if (ui_stat == 125) // Room UI
            {
                if (!net_server.received_data.empty())
                {
                    int data_type = net_server.received_data.front().type;
                    vector<uint8_t> tmp = net_server.received_data.front().bytes;
                    net_server.received_data.pop();
                    if (data_type == 15) // accept room game
                    {
                        string the_player_name(tmp.begin(), tmp.end());
                        ShowToast("User " + the_player_name + " accepted your invitation", 4, 1);
                    }
                    else if (data_type == 16) // decline room game
                    {
                        string the_player_name(tmp.begin(), tmp.end());
                        ShowToast("User " + the_player_name + " declined your invitation", 4, 0);
                    }
                    else if (data_type == 7)
                    {
                        string msg(tmp.begin(), tmp.end());
                        ShowToast("Server Reject: " + msg, 4, 2);
                        button_disable = 0;
                    }
                    else if (data_type == 20) // players list info
                    {
                        player_list.clear();
                        uint8_t data_num = tmp[0];
                        int ptr = 1;
                        for (int i = 0; i < data_num; i++)
                        {
                            Player_info player_tmp;
                            player_tmp.UserID = tmp[0 + ptr] * 256 + tmp[1 + ptr];
                            player_tmp.PlayerELO = tmp[2 + ptr] * 256 + tmp[3 + ptr];
                            player_tmp.GamePlayed = tmp[4 + ptr] * 256 + tmp[5 + ptr];
                            uint8_t username_len = tmp[6 + ptr];
                            ptr += 7;
                            string host_username(tmp.begin() + ptr, tmp.begin() + ptr + username_len);
                            ptr += username_len;
                            player_tmp.PlayerUsername = host_username;
                            player_list.push_back(player_tmp);
                        }
                    }
                    else if (data_type == 21) // room info
                    {
                        MyRoomInfo.PrivatePublic = tmp[0];
                        MyRoomInfo.GameMode = tmp[1];
                        MyRoomInfo.WatchingNumber = tmp[2];
                        MyRoomInfo.room_code = tmp[3] * 256 + tmp[4];
                        MyRoomInfo.HostPlayerELO = tmp[5] * 256 + tmp[6];
                        MyRoomInfo.Player2ELO = tmp[7] * 256 + tmp[8];
                        uint8_t hostlen = tmp[9], p2len = tmp[10];
                        string host_username(tmp.begin() + 11, tmp.begin() + 11 + hostlen);
                        string p2_username(tmp.begin() + 11 + hostlen, tmp.begin() + 11 + hostlen + p2len);
                        MyRoomInfo.HostPlayerUsername = host_username;
                        MyRoomInfo.Player2Username = p2_username;
                    }
                    else if (data_type == 22) // host leave the room
                    {
                        MyRoomInfo = Room();
                        invitation_list.clear();
                        room_list.clear();
                        player_list.clear();

                        ShowToast("The host has left, and the room is now closed.", 4.0f, 2);
                        ui_stat = 124;
                    }
                    else if (data_type == 25) // start the game
                    {
                        MyGamePort = tmp[0] * 256 + tmp[1];
                        ui_stat = 150;
                        MyGameTable = GameTable();
                        net_game_server.connect("140.113.17.11", MyGamePort);
                    }
                }
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                // title
                ImVec2 pos1(io.DisplaySize.x * 0.075f, io.DisplaySize.y * 0.1f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(Smorufont72);
                string title = "The Tetris";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                ImGui::PushFont(Smorufont36);
                ImVec2 pos2((io.DisplaySize.x - (pos1.x + 300)) * 0.1 + pos1.x + 300, pos1.y + 80);
                ImGui::SetCursorPos(pos2);
                string room_info = (string)((MyRoomInfo.PrivatePublic == 0) ? "Private Room" : "Public Room ") + "\n" + "Game mode:   " + (string)((MyRoomInfo.GameMode == 0) ? "Timed" : "Survival") + "\n" + "\n" + "\n" +
                                   "Host Player:   " + (MyRoomInfo.HostPlayerUsername != "" ? MyRoomInfo.HostPlayerUsername : "[WAITING...]") + "\n" +
                                   "Host Player's Rating:   " + to_string(MyRoomInfo.HostPlayerELO) + "\n" + "\n" +
                                   "Player 2:   " + (MyRoomInfo.Player2Username != "" ? MyRoomInfo.Player2Username : "[WAITING...]") + "\n" +
                                   "Player 2's Rating:   " + to_string(MyRoomInfo.Player2ELO) + "\n";
                ImGui::Text("%s", room_info.c_str());
                ImVec2 pos3(pos2.x, (io.DisplaySize.y - 600) * 0.5 + 500);
                ImGui::SetCursorPos(pos3);
                if (ImGui::Button("Leave", ImVec2(250 * (io.DisplaySize.x / 1440.0f), 60)))
                {
                    MyRoomInfo = Room();
                    invitation_list.clear();
                    room_list.clear();
                    player_list.clear();

                    vector<uint8_t> data;
                    data.push_back(11);
                    net_server.send_bytes(data);
                    ui_stat = 124;
                    button_disable = 0;
                }
                if (room_host_or_p2 == 0)
                {
                    ImVec2 pos4((io.DisplaySize.x - (pos2.x + 250 * (io.DisplaySize.x / 1440.0f))) * 0.3 + pos2.x + 250 * (io.DisplaySize.x / 1440.0f), (io.DisplaySize.y - 600) * 0.5 + 500);
                    ImGui::SetCursorPos(pos4);
                    ImGui::BeginDisabled(MyRoomInfo.Player2Username == "");
                    if (ImGui::Button("Start", ImVec2(250 * (io.DisplaySize.x / 1440.0f), 60)))
                    {
                        vector<uint8_t> data;
                        data.push_back(25);
                        net_server.send_bytes(data);
                    }
                    ImGui::EndDisabled();
                }
                ImGui::PopFont();
                if (room_host_or_p2 == 0)
                {
                    // player list
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 120, 255, 255));
                    ImVec2 pos5(pos1.x, pos1.y + 100);
                    ImGui::SetCursorPos(pos5);
                    ImGui::BeginTable("Avail Players", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2((300) * 0.9, (io.DisplaySize.y - (pos1.y + 100)) * 0.9));
                    ImGui::TableSetupColumn("User");
                    ImGui::TableSetupColumn("ELO");
                    ImGui::TableSetupColumn("Games"); // Games Played
                    ImGui::TableSetupColumn("Op.");   //, ImGuiTableColumnFlags_WidthFixed, 120.0f
                    ImGui::TableHeadersRow();
                    ImGui::PopStyleColor();
                    for (int i = 0; i < player_list.size(); i++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", player_list[i].PlayerUsername.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", to_string(player_list[i].PlayerELO).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", to_string(player_list[i].GamePlayed).c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        // ImGui::BeginDisabled();
                        if (ImGui::Button("Invite", ImVec2(60, 0)))
                        {
                            vector<uint8_t> data;
                            data.push_back(14);
                            data.push_back(player_list[i].UserID / 256);
                            data.push_back(player_list[i].UserID % 256);
                            net_server.send_bytes(data);
                            ShowToast("User " + player_list[i].PlayerUsername + " has been invited", 4, 1);
                        }
                        // ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::End();
            }
            else if (ui_stat == 150 || ui_stat == 151) // Game UI
            {
                if (ui_stat == 150)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(0);
                        net_game_server.send_bytes(data);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(1);
                        net_game_server.send_bytes(data);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(2);
                        net_game_server.send_bytes(data);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Space))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(3);
                        net_game_server.send_bytes(data);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Z))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(4);
                        net_game_server.send_bytes(data);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_X))
                    {
                        vector<uint8_t> data;
                        data.push_back(30);
                        data.push_back(room_host_or_p2);
                        data.push_back(5);
                        net_game_server.send_bytes(data);
                    }
                }
                if (MyGameTable.GameStat <= 4)
                {
                    if (!net_game_server.received_data.empty())
                    {
                        int data_type = net_game_server.received_data.front().type;
                        vector<uint8_t> tmp = net_game_server.received_data.front().bytes;
                        net_game_server.received_data.pop();
                        if (data_type == 31)
                        {
                            MyGameTable.GameMode = tmp[0];
                            MyGameTable.Player1ELO = tmp[1] * 256 + tmp[2];
                            MyGameTable.Player2ELO = tmp[3] * 256 + tmp[4];
                            MyGameTable.GameStat = tmp[5];
                            MyGameTable.P1_score = (uint32_t)(tmp[6] << 24) + (uint32_t)(tmp[7] << 16) + (uint32_t)(tmp[8] << 8) + tmp[9];
                            MyGameTable.P2_score = (uint32_t)(tmp[10] << 24) + (uint32_t)(tmp[11] << 16) + (uint32_t)(tmp[12] << 8) + tmp[13];
                            for (int i = 0; i < 20; i++)
                                for (int j = 0; j < 10; j++)
                                    MyGameTable.P1_Table[i][j] = tmp[14 + i * 10 + j];
                            for (int i = 0; i < 20; i++)
                                for (int j = 0; j < 10; j++)
                                    MyGameTable.P2_Table[i][j] = tmp[214 + i * 10 + j];
                            MyGameTable.GameTime = tmp[414] * 256 + tmp[415];
                            uint8_t p1name_len = tmp[416], p2name_len = tmp[417];
                            string P1name(tmp.begin() + 418, tmp.begin() + 418 + p1name_len);
                            string P2name(tmp.begin() + 418 + p1name_len, tmp.begin() + 418 + p1name_len + p2name_len);
                            MyGameTable.Player1Username = P1name;
                            MyGameTable.Player2Username = P2name;
                        }
                    }
                }
                else
                {
                    if (net_game_server.connection_alive())
                        net_game_server.stop();
                }
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
                ImGui::Begin("##FullscreenGame", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
                ImGuiIO &io = ImGui::GetIO();
                ImGui::PushFont(Smorufont36);
                // title
                ImVec2 pos1(io.DisplaySize.x * 0.1f, io.DisplaySize.y * 0.05f);
                ImGui::SetCursorPos(pos1);
                string p1_info = "Player 1:   " + MyGameTable.Player1Username + "\n" +
                                 "Player 1's Rating: " + to_string(MyGameTable.Player1ELO) + "\n" +
                                 "SCORE: " + to_string(MyGameTable.P1_score);
                ImGui::Text("%s", p1_info.c_str());
                ImVec2 pos2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.05f);
                ImGui::SetCursorPos(pos2);
                string p2_info = "Player 2:   " + MyGameTable.Player2Username + "\n" +
                                 "Player 2's Rating: " + to_string(MyGameTable.Player2ELO) + "\n" +
                                 "SCORE: " + to_string(MyGameTable.P2_score);
                ImGui::Text("%s", p2_info.c_str());
                if (MyGameTable.GameStat < 4)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 135, 255, 255));
                    string MyGameTime = (MyGameTable.GameStat == 0 ? "Ready?" : (MyGameTable.GameStat == 1 ? "3" : (MyGameTable.GameStat == 2 ? "2" : "1")));
                    float text_w = ImGui::CalcTextSize(MyGameTime.c_str()).x;
                    ImVec2 pos3((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.1f);
                    ImGui::SetCursorPos(pos3);
                    ImGui::Text("%s", MyGameTime.c_str());
                    ImGui::PopStyleColor();
                }
                int rect_w = (io.DisplaySize.x * 0.4 * 0.1);
                int rect_h = ((io.DisplaySize.y - 130 - io.DisplaySize.y * 0.05f) * 0.95 * 0.05);
                int square_w = min(rect_w, rect_h);
                ImDrawList *dl = ImGui::GetWindowDrawList();
                for (int i = 0; i < 20; i++)
                    for (int j = 0; j < 10; j++)
                    {
                        ImVec2 PosA((io.DisplaySize.x * 0.5 - square_w * 10) * 0.5 + j * square_w, 130 + io.DisplaySize.y * 0.05f + i * square_w);
                        ImVec2 PosB(PosA.x + square_w, PosA.y + square_w);
                        if (MyGameTable.P1_Table[i][j] < 9)
                        {
                            dl->AddRectFilled(PosA, PosB, table_colors[MyGameTable.P1_Table[i][j]], 3.0f);
                            dl->AddRect(PosA, PosB, grid_color, 0, 0, 2.0f);
                        }
                        else
                        {
                            dl->AddRectFilled(PosA, PosB, table_colors[0], 3.0f);
                            dl->AddRect(PosA, PosB, table_colors[MyGameTable.P1_Table[i][j] - 8], 0, 0, 2.0f);
                        }
                    }
                for (int i = 0; i < 20; i++)
                    for (int j = 0; j < 10; j++)
                    {
                        ImVec2 PosA(io.DisplaySize.x * 0.5 + (io.DisplaySize.x * 0.5 - square_w * 10) * 0.5 + j * square_w, 130 + io.DisplaySize.y * 0.05f + i * square_w);
                        ImVec2 PosB(PosA.x + square_w, PosA.y + square_w);
                        if (MyGameTable.P2_Table[i][j] < 9)
                        {
                            dl->AddRectFilled(PosA, PosB, table_colors[MyGameTable.P2_Table[i][j]], 3.0f);
                            dl->AddRect(PosA, PosB, grid_color, 0, 0, 2.0f);
                        }
                        else
                        {
                            dl->AddRectFilled(PosA, PosB, table_colors[0], 3.0f);
                            dl->AddRect(PosA, PosB, table_colors[MyGameTable.P2_Table[i][j] - 8], 0, 0, 2.0f);
                        }
                    }
                if (MyGameTable.GameStat == 4)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 50, 255));
                    string MyGameTime = to_string(MyGameTable.GameTime / 60) + ":" + ((MyGameTable.GameTime % 60) < 10 ? "0" : "") + to_string(MyGameTable.GameTime % 60);
                    float text_w = ImGui::CalcTextSize(MyGameTime.c_str()).x;
                    ImVec2 pos3((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.1f);
                    ImGui::SetCursorPos(pos3);
                    ImGui::Text("%s", MyGameTime.c_str());
                    ImGui::PopStyleColor();
                }
                else if (MyGameTable.GameStat >= 5)
                {
                    ImGui::PopFont();
                    ImGui::PushFont(Smorufont72);
                    if (!ImGui::IsPopupOpen("GameOver"))
                        ImGui::OpenPopup("GameOver");
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                    if (ImGui::BeginPopupModal("GameOver", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
                    {
                        string GameOver_title = (MyGameTable.GameMode == 0 ? "Time's up!" : "Game Over");
                        float text_w = ImGui::CalcTextSize(GameOver_title.c_str()).x;
                        ImVec2 pos4(ImGui::GetCursorPos().x + (600 - text_w) * 0.5, ImGui::GetCursorPos().y + 60);
                        ImGui::SetCursorPos(pos4);
                        ImGui::Text("%s", GameOver_title.c_str());
                        GameOver_title = (MyGameTable.GameStat == 5 ? MyGameTable.Player1Username + " WIN!" : (MyGameTable.GameStat == 6 ? MyGameTable.Player2Username + " WIN!" : (MyGameTable.GameMode == 0 ? "All Square!" : "Both Survived!")));
                        text_w = ImGui::CalcTextSize(GameOver_title.c_str()).x;
                        ImVec2 pos5(ImGui::GetCursorPos().x + (600 - text_w) * 0.5, ImGui::GetCursorPos().y);
                        ImGui::SetCursorPos(pos5);
                        ImGui::Text("%s", GameOver_title.c_str());
                        ImGui::PopFont();
                        ImGui::PushFont(Smorufont36);
                        // ELO update
                        double E_A = 1.0 / (1.0 + pow(10.0, ((double)(MyGameTable.Player2ELO - MyGameTable.Player1ELO) / 400.0)));
                        double E_B = 1.0 - E_A;
                        int P1_delta_ELO = (int)(32 * ((MyGameTable.GameStat == 5 ? 1.0 : (MyGameTable.GameStat == 6 ? 0.0 : 0.5)) - E_A));
                        int P2_delta_ELO = (int)(32 * ((MyGameTable.GameStat == 5 ? 0.0 : (MyGameTable.GameStat == 6 ? 1.0 : 0.5)) - E_B));
                        string Delta_ELO_title = (MyGameTable.Player1Username + " " + (P1_delta_ELO > 0 ? "+" : "") + to_string(P1_delta_ELO) + "   " + MyGameTable.Player2Username + " " + (P2_delta_ELO > 0 ? "+" : "") + to_string(P2_delta_ELO));
                        float title_w = ImGui::CalcTextSize(Delta_ELO_title.c_str()).x;
                        ImVec2 pos5_2(ImGui::GetCursorPos().x + (600 - title_w) * 0.5, ImGui::GetCursorPos().y);
                        ImGui::SetCursorPos(pos5_2);
                        ImGui::Text("%s", Delta_ELO_title.c_str());
                        ImVec2 pos6(ImGui::GetCursorPos().x + (600 - 300) * 0.5, ImGui::GetCursorPos().y + 30);
                        ImGui::SetCursorPos(pos6);
                        if (ImGui::Button((ui_stat == 150 ? "Return to Room" : "Return to Lobby"), ImVec2(300, 80)))
                        {
                            ImGui::CloseCurrentPopup();
                            ui_stat = (ui_stat == 150 ? 125 : 124);
                            button_disable = 0;
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::PopFont();
                ImGui::End();
            }
        }

        if (ui_stat != 0)
        {
            double t = ImGui::GetTime();
            int info_number = 0; // 0 warning sign 1 success 2 error
            for (auto it = toast_info.begin(); it != toast_info.end();)
            {
                if (t < get<1>(*it))
                {
                    ImGuiIO &io = ImGui::GetIO();
                    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 128));
                    float alpha = (get<1>(*it) - t > 0.79 ? 0.79 : get<1>(*it) - t) / 0.8;
                    ImGui::PushStyleColor(ImGuiCol_Text, (get<2>(*it) == 2 ? IM_COL32(255, 0, 0, (int)(255.0 * alpha)) : (get<2>(*it) == 0 ? IM_COL32(255, 255, 0, (int)(255.0 * alpha)) : IM_COL32(10, 255, 10, (int)(255.0 * alpha))))); // notification type
                    ImGui::SetNextWindowBgAlpha(alpha);
                    ImGui::SetNextWindowSize(ImVec2(0, 35));
                    ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y - 60 - 60 * (info_number++)), ImGuiCond_Always);
                    string winID = "##TOAST" + to_string(info_number);
                    ImGui::Begin(winID.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
                    string toast = get<0>(*it);
                    ImGui::TextUnformatted(toast.c_str());
                    ImGui::End();
                    ImGui::PopStyleColor(2);
                    it++;
                }
                else
                    it = toast_info.erase(it);
            }
        }

        signed w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);

        if (ui_stat == -1)
            break;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
}
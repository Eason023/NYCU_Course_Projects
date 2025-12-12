#define PLAYER_SERVER_PORT 52023
#define PLAYER_SERVER_IP "140.113.17.11"
#include <functional>
#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <filesystem>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include "asio/asio.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "glfw/include/GLFW/glfw3.h"
#include "subprocess/subprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

using namespace __gnu_pbds;

template<class Key, class Val>
using od_map = tree<Key, Val, less<Key>, rb_tree_tag, tree_order_statistics_node_update>;
template<class T>
using od_set = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;

/*
Data Type Definition:
1 [Client] ping
2 [Server] pong

3 [Client] registration (username, account name, password)
4 [Client] login (account, password)
5 [Client] logout

6 [Server] Server Accept
7 [Server] Server Reject (and reason)

10 [Client] request game list
11 [Server] game list (list of game IDs)
12 [Client] request game details (game id)
13 [Server] game details (game id, game image, game name, developer, publisher, version, description, price, own the game or not, reviews, max room number, max players per room)
14 [Client] request room list (game id)
15 [Server] room list (game id, list of room infos (room id, current players))
16 [Client] update review (game id, rating, review text)
17 [Server] review update ack (game id) (ok)
18 [Server] game page out of date (old game id and new game id)
19 [Server] room list out of date

20 [Client] request purchase (game id)
21 [Server] purchase ack (game id) (ok)
22 [Client] launch game (game id, version)
23 [Server] launch game result (game id, ok / need update)
24 [Client] request game download / update (game id)
25 [Server] game download info (game id, version, file size (Bytes) ) // at most 1kB per chunk
26 [Server] game download chunk (chunk size, chunk data)
27 [Client] create a room (game id)
28 [Client] join game room (game id, room id)
29 [Server] requested room ready, game start (room id)

30 [Game Client] msg from game (forward to server)
31 [Server] msg from server (forward to game client)
32 [Client] leave game room (stop gaming)
33 [Server] game server stopped (and reason)

// Compile command
// Windows: g++ GameStore.cpp imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I asio -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -std=c++20 -lws2_32 -lmswsock -lopengl32 -lgdi32 -o GameStore.exe -mwindows
*/

template <class T>
class ConcurrentQueue {
private:
    mutex m_;
    queue<T> q_;

public:
    void push(const T& v) {
        unique_lock<mutex> lk(m_);
        q_.push(v);
    }
    void pop() {
        unique_lock<mutex> lk(m_);
        if (q_.empty())
            return;
        q_.pop();
        return;
    }
    T front() {
        unique_lock<mutex> lk(m_);
        return q_.front();
    }
    bool empty() {
        unique_lock<mutex> lk(m_);
        return q_.empty();
    }
};

struct Packet {
    uint8_t type;
    vector<uint8_t> bytes;
};

vector<uint8_t> uint32_to_bytes(uint32_t val) {
    vector<uint8_t> bytes(4);
    bytes[0] = (val >> 24) & 0xFF;
    bytes[1] = (val >> 16) & 0xFF;
    bytes[2] = (val >> 8) & 0xFF;
    bytes[3] = val & 0xFF;
    return bytes;
}

uint32_t bytes_to_uint32(const vector<uint8_t>& bytes, size_t start_index = 0) {
    return (static_cast<uint32_t>(bytes[0 + start_index]) << 24) |
        (static_cast<uint32_t>(bytes[1 + start_index]) << 16) |
        (static_cast<uint32_t>(bytes[2 + start_index]) << 8) |
        static_cast<uint32_t>(bytes[3 + start_index]);
}

class NetClient {
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

    // for game process
    vector<uint8_t> game_read_buf;
    tcp::acceptor game_acp_;
    asio::ip::tcp::socket game_socket_;
    bool game_forwarding;
    uint32_t game_forwarding_data_len;

    void start_heartbeat() { // Heartbeat with server
        ping_timer_.expires_after(1s);
        ping_timer_.async_wait([this](asio::error_code ec) {
            if (ec)
                return;
            send_bytes({ 1 });
            start_heartbeat();
        });
    }

    void receive_data() { // Heartbeat with server
        pong_timer_.expires_after(5s);
        pong_timer_.async_wait([this](asio::error_code ec) {
            if (!ec)
                stop();
        });
        asio::async_read(socket_, asio::buffer(read_buf.data(), 4), [this](asio::error_code ec, size_t) {
            if (!ec) {
                pong_timer_.cancel();
                received_data_len = ntohl((read_buf[0] << 24) + (read_buf[1] << 16) + (read_buf[2] << 8) + read_buf[3]);
                asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t) {
                    uint8_t data_type = read_buf[0];
                    if (game_forwarding && data_type == 31) {
                        asio::error_code ec;
                        vector<uint8_t> data_to_send(read_buf.begin() + 1, read_buf.begin() + received_data_len), header = uint32_to_bytes(static_cast<uint32_t>(received_data_len - 1));
                        data_to_send.insert(data_to_send.begin(), header.begin(), header.end());
                        asio::write(game_socket_, asio::buffer(data_to_send.data(), data_to_send.size()), ec);
                    }
                    else if (data_type != 2) {
                        Packet tmp;
                        tmp.type = data_type;
                        tmp.bytes = vector<uint8_t>(read_buf.begin() + 1, read_buf.begin() + received_data_len);
                        received_data.push(tmp);
                    }
                    receive_data();
                });
            }
        });
    }

    // Length-Prefixed Framing Protocol
    void send_bytes(vector<uint8_t>&& bytes) {
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

    //for game process
    void game_msg_to_server_forwarding() {
        asio::async_read(game_socket_, asio::buffer(game_read_buf.data(), 4), [this](asio::error_code ec, size_t) {
            game_forwarding_data_len = (game_read_buf[0] << 24) + (game_read_buf[1] << 16) + (game_read_buf[2] << 8) + game_read_buf[3];
            if (!ec) {
                asio::async_read(game_socket_, asio::buffer(game_read_buf.data(), game_forwarding_data_len), [this](asio::error_code ec, size_t) {
                    if (!ec) {
                        send_packet(30, vector<uint8_t>(game_read_buf.begin(), game_read_buf.begin() + game_forwarding_data_len));
                        game_msg_to_server_forwarding();
                    }
                });
            }
        });
    }

public:
    ConcurrentQueue<Packet> received_data;

    NetClient() : ioc_(),
        socket_(ioc_),
        ping_timer_(ioc_),
        pong_timer_(ioc_),
        timer_(ioc_),
        game_acp_(ioc_, tcp::endpoint(tcp::v4(), 0)),
        game_socket_(ioc_)
    {
        connected = 0;
        read_buf.resize(262144); // Atmost 256kB per msg!
        game_read_buf.resize(262144);
        not_first_time = 0;
        game_forwarding = 0;
    }

    ~NetClient() { stop(); }

    bool connect(const string& server_ip, const uint16_t& port) {
        connected = 0;
        tcp::endpoint server_ep(asio::ip::make_address(server_ip), port);
        vector<tcp::endpoint> server_eps;
        server_eps.push_back(server_ep);
        timer_.expires_after(3s);
        timer_.async_wait([this](auto ec) {
            if (!ec) {
                asio::error_code ig;
                socket_.close(ig);
                connected = 0;
            }
        });
        socket_.open(asio::ip::tcp::v4());
        socket_.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 0)); // 0 = let os assign
        asio::async_connect(socket_, server_eps, [this](asio::error_code ec, const tcp::endpoint&) {
            timer_.cancel();
            if (ec)
                connected = 0;
            else
                connected = 1;
        });
        if (not_first_time)
            ioc_.restart();
        ioc_.run();
        if (connected)
            io_thread_ = thread([this] { start_heartbeat(); receive_data(); ioc_.restart(); ioc_.run(); });
        return connected;
    }

    void stop() {
        {
            unique_lock<mutex> lk(safe_lock_);
            ping_timer_.cancel();
            timer_.cancel();
            pong_timer_.cancel();
            socket_.close();
            connected = 0;
            not_first_time = 1;
        }
        if (io_thread_.joinable() && this_thread::get_id() != io_thread_.get_id())
            io_thread_.join();
    }

    void send_packet(uint8_t data_type, vector<uint8_t> bytes = {}) {
        bytes.insert(bytes.begin(), data_type);
        send_bytes(move(bytes));
    }

    bool connection_alive() {
        return connected;
    }

    //for game process
    uint16_t accept_connection_from_game() {
        game_socket_ = tcp::socket(ioc_);
        game_acp_.cancel();
        game_acp_.async_accept(game_socket_, [this](asio::error_code ec) {
            if (!ec) {
                game_forwarding = 1;
                game_msg_to_server_forwarding();
            }
            else {
                game_forwarding = 0;
            }
        });
        return game_acp_.local_endpoint().port();
    }

    void stop_game_forwarding() {
        game_forwarding = 0;
        game_socket_.close();
        game_acp_.cancel();
    }
};

class GamePage {
public:
    uint32_t game_id = 0;
    vector<uint8_t> image_data;
    string game_name;
    string developer;
    string publisher;
    uint8_t version[3] = {}; // major, minor, patch
    string description;
    uint32_t price = 0;
    uint8_t owned = 0;

    list<pair<string, pair<uint8_t, string>>> reviews; // user, rating, comment

    uint16_t max_room_number = 0;
    uint16_t max_players_per_room = 0;

    GLuint tex_id = 0;
    int tex_w = 0, tex_h = 0;

    ~GamePage() {
        if (tex_id != 0)
            glDeleteTextures(1, &tex_id);
    }

    bool LoadTextureFromPNGMemory(const vector<uint8_t>& png_data) {
        int w, h, channels_in_file;

        // PNG → RGBA
        unsigned char* pixels = stbi_load_from_memory(png_data.data(), (int)png_data.size(), &w, &h, &channels_in_file, 4);

        if (!pixels)
            return false;

        // OpenGL texture
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // 0x812F = GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // set alignment to 1 byte

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels); // free image memory

        tex_id = tex;
        tex_w = w;
        tex_h = h;
        return true;
    }
};

list<tuple<string, double, int>> toast_info;
void ShowToast(const string& msg, float duration = 2.0f, int type = 0) {
    toast_info.push_back(make_tuple(msg, ImGui::GetTime() + duration, type));
}

// main variables
NetClient net_server;
future<bool> connect_to_server;

const double targetFPS = 60.0;
const double targetFrameTime = 1.0 / targetFPS;
double lastTime = glfwGetTime();

ImFont* SystemFont16;
ImFont* SystemFont32;
ImFont* SystemFont48;
ImFont* SystemFont64;

string init[] = { "The Game Store by Eason023" , "Sorry, unable to connect to the server." };

int ui_stat = 1;
int ui_wait = 0;
string account_buf, password_buf, reg_username_buf, reg_account_buf, reg_password_buf;

bool refresh_game_list = 1;
od_set<uint32_t> game_list; // list of game IDs
uint8_t page_num = 0;
map<uint32_t, GamePage> game_pages; // game ID -> game page details
uint32_t selected_game_id = 0, game_room_id = 0;

const char* game_list_filter[] = { "Game Store", "Tools and Plugins" }; // 0: game list, 1: tools or plugin
int game_list_filter_current = 0;

string username = "";
uint32_t userpoints = 0;

tuple<uint8_t, uint8_t, uint8_t> check_local_game_version(string& game_name) {
    string game_folder = "downloads/" + username + "/" + game_name + "/";
    ifstream version_file(game_folder + "version");
    if (!version_file.is_open())
        return make_tuple(0, 0, 0);
    string version_str;
    getline(version_file, version_str);
    version_file.close();
    int major = 0, minor = 0, patch = 0;
    sscanf(version_str.c_str(), "v%d.%d.%d", &major, &minor, &patch);
    return make_tuple((uint8_t)major, (uint8_t)minor, (uint8_t)patch);
}

void remove_game_file_and_version(string& game_name) {
    string game_folder = "downloads/" + username + "/" + game_name + "/";
    filesystem::remove(game_folder + "version");
    filesystem::remove(game_folder + game_name + ".exe");
}

void create_version_file(string& game_name, tuple<uint8_t, uint8_t, uint8_t> version) {
    string game_folder = "downloads/" + username + "/" + game_name + "/";
    filesystem::create_directories(game_folder);
    ofstream file(game_folder + "version");
    if (!file)
        return;
    string v_str = ("v" + to_string(get<0>(version)) + "." + to_string(get<1>(version)) + "." + to_string(get<2>(version)));
    file << v_str;
    file.close();
}

// Not used (Avoid IO performance issue)
/*
void append_bytes_to_game_file(string& game_name, vector<uint8_t>& bytes, size_t start_index = 0) { // for exe file
    string game_folder = "downloads/" + username + "/" + game_name + "/";
    filesystem::create_directories(game_folder);
    ofstream file(game_folder + game_name + ".exe", std::ios::binary | std::ios::app);
    if (!file)
        return;
    file.write(reinterpret_cast<const char*>(bytes.data() + start_index), bytes.size() - start_index);
    file.close();
}
*/

subprocess_s game_process = { 0 };
void launch_the_game(string& game_name) {
    uint16_t game_process_port = net_server.accept_connection_from_game();
    string game_folder = "downloads/" + username + "/" + game_name + "/";
    string execute_path = "./" + game_folder + game_name + ".exe";
    string the_port = to_string(game_process_port);
    const char* cmd[] = { execute_path.c_str(), the_port.c_str(), NULL };
    int rc = subprocess_create(cmd, subprocess_option_inherit_environment, &game_process);
    if (rc != 0) {
        // error
        net_server.stop_game_forwarding();
        ShowToast("Failed to launch game (rc=" + to_string(rc) + "): " + game_name, 6.0f, 2);
        return;
    }
}

void stop_the_game_process() {
    net_server.stop_game_forwarding();
    subprocess_terminate(&game_process);
    subprocess_destroy(&game_process);
    memset(&game_process, 0, sizeof(game_process));
}

void RenderInfProgressBar(const float width = 500.0f, const float height = 30.0f) {
    ImVec2 size = ImVec2(width, height);
    float speed = 0.8f;
    float block_frac = 0.25f;
    const ImGuiStyle& style = ImGui::GetStyle();

    // 取得繪製起點（螢幕座標）
    ImVec2 p = ImVec2(ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - size.x) * 0.5, ImGui::GetCursorScreenPos().y + 200 - size.y * 0.5);
    ImVec2 q = ImVec2(p.x + size.x, p.y + size.y);
    // 顏色
    ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 col_fill = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    // 畫底框
    ImDrawList* draw = ImGui::GetWindowDrawList();
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
}

void RenderScene(GLFWwindow* win) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    if (ui_stat == 0) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("##Fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(SystemFont64);
        float text_w = ImGui::CalcTextSize(init[1].c_str()).x;
        ImVec2 pos1((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.5 - 72);
        ImGui::SetCursorPos(pos1);
        ImGui::Text("%s", init[1].c_str());
        ImGui::PopFont();
        ImGui::End();
    }
    else if (ui_stat <= 120) {
        // fps limiter
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastTime;
        if (elapsed < targetFrameTime)
            std::this_thread::sleep_for(chrono::duration<double>(targetFrameTime - elapsed));
        lastTime = currentTime;

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("##Fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, min(((float)ui_stat) / 60.0f, 1.0f));
        ImGui::PushFont(SystemFont64);
        float text_w = ImGui::CalcTextSize(init[0].c_str()).x;
        ImVec2 pos1((io.DisplaySize.x - text_w) * 0.5, io.DisplaySize.y * 0.5 - 72);
        ImGui::SetCursorPos(pos1);
        ImGui::Text("%s", init[0].c_str());
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::End();
        if (ui_stat == 1)
            connect_to_server = async(launch::async, &NetClient::connect, &net_server, PLAYER_SERVER_IP, PLAYER_SERVER_PORT);
        if (ui_stat == 120) {
            if (connect_to_server.wait_for(chrono::milliseconds(0)) == future_status::ready) {
                if (connect_to_server.get())
                    ui_stat = 121;
                else
                    ui_stat = 0;
            }
        }
        else
            ui_stat++;
    }
    else {
        if (!net_server.connection_alive())
            ui_stat = 0;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("##Fullscreen2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        ImGuiIO& io = ImGui::GetIO();
        if (ui_stat == 121) { // lobby ui
            ImVec2 panel_size(400, 300);
            ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f, (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
            ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
            ImGui::SetCursorPos(pos1);
            ImGui::PushFont(SystemFont64);
            string title = "The Game Store";
            ImGui::Text("%s", title.c_str());
            ImGui::PopFont();
            ImGui::SetCursorPos(panel_pos);
            ImGui::BeginChild("lobby_panel", panel_size, true);
            ImGui::PushFont(SystemFont32);
            ImVec2 button_size(240, 64);
            ImVec2 pos2((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f);
            ImGui::SetCursorPos(pos2);
            if (ImGui::Button("Login", button_size)) {
                ui_stat = 122;
            }
            ImVec2 pos3((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f + 100);
            ImGui::SetCursorPos(pos3);
            if (ImGui::Button("Register", button_size)) {
                ui_stat = 123;
            }
            ImVec2 pos4((panel_size.x - button_size.x) * 0.5f, (100 - button_size.y) * 0.5f + 200);
            ImGui::SetCursorPos(pos4);
            if (ImGui::Button("Exit", button_size)) {
                net_server.send_packet(5);
                ui_stat = -1;
            }
            ImGui::PopFont();
            ImGui::EndChild();
        }
        else if (ui_stat == 122) { // login ui
            ImVec2 panel_size(450, 180);
            ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f,
                (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
            ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
            ImGui::SetCursorPos(pos1);
            ImGui::PushFont(SystemFont64);
            string title = "The Game Store";
            ImGui::Text("%s", title.c_str());
            ImGui::PopFont();
            ImGui::SetCursorPos(panel_pos);
            ImGui::BeginChild("login_panel", panel_size, true);
            ImGui::PushFont(SystemFont32);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" Account:  ");
            ImGui::SameLine(130);
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##Account", &account_buf);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" Password: ");
            ImGui::SameLine(130);
            ImGui::SetNextItemWidth(300);
            bool submit = ImGui::InputText("##Password", &password_buf, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
            int ipt_text_h = ImGui::GetItemRectSize().y;
            ImVec2 pos2((panel_size.x - 350) * 0.5f, ipt_text_h * 3);
            ImGui::SetCursorPos(pos2);
            if (ImGui::Button("Login", ImVec2(160, 42)) || submit) {
                if (account_buf.length() <= 12 && password_buf.length() <= 12 && account_buf.length() > 0 && password_buf.length() > 0) {
                    vector<uint8_t> data;
                    data.push_back(account_buf.length());
                    data.push_back(password_buf.length());
                    data.insert(data.end(), account_buf.begin(), account_buf.end());
                    data.insert(data.end(), password_buf.begin(), password_buf.end());
                    data.push_back(0); // 0 denote player client
                    net_server.send_packet(4, data);
                    ui_wait = 1;
                }
                else
                    ShowToast("The account and password length should be between 1~12.", 4.0f, 2);
            }
            ImVec2 pos3((panel_size.x - 350) * 0.5f + 160 + 30, ipt_text_h * 3);
            ImGui::SetCursorPos(pos3);
            if (ImGui::Button("Cancel", ImVec2(160, 42))) {
                ui_stat = 121;
            }
            ImGui::PopFont();
            ImGui::EndChild();
            if (ui_wait && !ImGui::IsPopupOpen("Verifying..."))
                ImGui::OpenPopup("Verifying...");
            if (ui_wait) {
                ImGui::PushFont(SystemFont32);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("Verifying...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    RenderInfProgressBar();
                    if (!net_server.received_data.empty()) {
                        if (net_server.received_data.front().type == 6) {
                            userpoints = bytes_to_uint32(net_server.received_data.front().bytes);
                            vector<uint8_t> tmp = net_server.received_data.front().bytes;
                            username = string(tmp.begin() + 4, tmp.end());
                            net_server.received_data.pop();
                            ui_stat = 124;
                        }
                        else { // if (net_server.received_data.front().type == 7)
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
                ImGui::PopFont();
            }
        }
        else if (ui_stat == 123) { // register ui
            ImVec2 panel_size(450, 210);
            ImVec2 panel_pos((io.DisplaySize.x - panel_size.x) * 0.5f,
                (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f + 150 * (io.DisplaySize.y / 810.0f));
            ImVec2 pos1((io.DisplaySize.x - panel_size.x) * 0.5f,
                (io.DisplaySize.y - panel_size.y - 72 - 150 * (io.DisplaySize.y / 810.0f)) * 0.5f);
            ImGui::SetCursorPos(pos1);
            ImGui::PushFont(SystemFont64);
            string title = "The Game Store";
            ImGui::Text("%s", title.c_str());
            ImGui::PopFont();
            ImGui::SetCursorPos(panel_pos);
            ImGui::BeginChild("register_panel", panel_size, true);
            ImGui::PushFont(SystemFont32);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" Username: ");
            ImGui::SameLine(135);
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##Username", &reg_username_buf);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" Account:  ");
            ImGui::SameLine(135);
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##Account", &reg_account_buf);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" Password: ");
            ImGui::SameLine(135);
            ImGui::SetNextItemWidth(300);
            bool submit = ImGui::InputText("##Password", &reg_password_buf, ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
            int ipt_text_h = ImGui::GetItemRectSize().y;
            ImVec2 pos2((panel_size.x - 350) * 0.5f, ipt_text_h * 4);
            ImGui::SetCursorPos(pos2);
            if (ImGui::Button("Register", ImVec2(160, 42)) || submit) {
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
                if (valid) {
                    vector<uint8_t> data;
                    data.push_back(reg_username_buf.length());
                    data.push_back(reg_account_buf.length());
                    data.push_back(reg_password_buf.length());
                    data.insert(data.end(), reg_username_buf.begin(), reg_username_buf.end());
                    data.insert(data.end(), reg_account_buf.begin(), reg_account_buf.end());
                    data.insert(data.end(), reg_password_buf.begin(), reg_password_buf.end());
                    data.push_back(0); // 0 denote player account
                    net_server.send_packet(3, data);
                    ui_wait = 1;
                }
                else
                    ShowToast("Your Username, Account, and Password should NOT contain spaces. The length should be between 1~12.", 4, 0);
            }
            ImVec2 pos3((panel_size.x - 350) * 0.5f + 160 + 30, ipt_text_h * 4);
            ImGui::SetCursorPos(pos3);
            if (ImGui::Button("Cancel", ImVec2(160, 42))) {
                ui_stat = 121;
            }
            ImGui::PopFont();
            ImGui::EndChild();
            if (ui_wait && !ImGui::IsPopupOpen("Verifying..."))
                ImGui::OpenPopup("Verifying...");
            if (ui_wait) {
                ImGui::PushFont(SystemFont32);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("Verifying...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    RenderInfProgressBar();
                    if (!net_server.received_data.empty()) {
                        if (net_server.received_data.front().type == 6) {
                            net_server.received_data.pop();
                            reg_username_buf = "";
                            reg_account_buf = "";
                            reg_password_buf = "";
                            ShowToast("Account has been registered successfully.", 4, 1);
                            ui_stat = 121;
                        }
                        else { // if (net_server.received_data.front().type == 7)
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
                ImGui::PopFont();
            }
        }
        else if (ui_stat == 124 || ui_stat == 125) {
            // process received data
            bool selected_page_out_of_date = 0, selected_page_has_been_removed = 0;
            // 0: not launching, 1: launching, 2: downloading, 3: choose the room, 4: game running
            static int launching_state = 0;
            static bool resend_request = 0;
            static uint32_t downloaded_byte_num = 0, download_total_byte_num = 0, download_gid = 0;
            static tuple<uint8_t, uint8_t, uint8_t> downloaded_game_version = make_tuple(0, 0, 0);
            static ofstream download_file_stream;
            static vector<pair<uint32_t, vector<string>>> room_list; // room_id, players in the room
            static string game_server_stop_reason;
            while (!net_server.received_data.empty()) {
                int data_type = net_server.received_data.front().type;
                vector<uint8_t> tmp = net_server.received_data.front().bytes;
                net_server.received_data.pop();
                if (data_type == 7) {
                    string msg(tmp.begin(), tmp.end());
                    ShowToast("Server Reject: " + msg, 4, 2);
                }
                else if (data_type == 11) { // get game list
                    size_t idx = 0;
                    while (idx + 4 <= tmp.size()) {
                        uint32_t gid = bytes_to_uint32(tmp, idx);
                        idx += 4;
                        game_list.insert(gid);
                        game_pages.insert({ gid, GamePage() });
                    }
                }
                else if (data_type == 13) { // get game page details
                    GamePage gp;
                    size_t idx = 0;
                    gp.game_id = bytes_to_uint32(tmp, idx);
                    idx += 4;
                    uint32_t img_size = bytes_to_uint32(tmp, idx);
                    idx += 4;
                    gp.image_data.insert(gp.image_data.end(), tmp.begin() + idx, tmp.begin() + idx + img_size);
                    idx += img_size;
                    uint8_t name_len = tmp[idx];
                    idx += 1;
                    gp.game_name = string(tmp.begin() + idx, tmp.begin() + idx + name_len);
                    idx += name_len;
                    uint8_t dev_len = tmp[idx];
                    idx += 1;
                    gp.developer = string(tmp.begin() + idx, tmp.begin() + idx + dev_len);
                    idx += dev_len;
                    uint8_t pub_len = tmp[idx];
                    idx += 1;
                    gp.publisher = string(tmp.begin() + idx, tmp.begin() + idx + pub_len);
                    idx += pub_len;
                    gp.version[0] = tmp[idx]; gp.version[1] = tmp[idx + 1]; gp.version[2] = tmp[idx + 2];
                    idx += 3;
                    uint16_t desc_len = (static_cast<uint16_t>(tmp[idx]) << 8) | static_cast<uint16_t>(tmp[idx + 1]);
                    idx += 2;
                    gp.description = string(tmp.begin() + idx, tmp.begin() + idx + desc_len);
                    idx += desc_len;
                    gp.price = bytes_to_uint32(tmp, idx);
                    idx += 4;
                    gp.owned = tmp[idx];
                    idx += 1;
                    uint8_t review_count = tmp[idx];
                    idx += 1;
                    for (int i = 0; i < review_count; i++) {
                        uint8_t user_len = tmp[idx];
                        idx += 1;
                        string user = string(tmp.begin() + idx, tmp.begin() + idx + user_len);
                        idx += user_len;
                        uint8_t rating = tmp[idx];
                        idx += 1;
                        uint16_t comment_len = (static_cast<uint16_t>(tmp[idx]) << 8) | static_cast<uint16_t>(tmp[idx + 1]);
                        idx += 2;
                        string comment = string(tmp.begin() + idx, tmp.begin() + idx + comment_len);
                        idx += comment_len;
                        gp.reviews.push_back(make_pair(user, make_pair(rating, comment)));
                    }
                    gp.max_room_number = (static_cast<uint16_t>(tmp[idx]) << 8) | static_cast<uint16_t>(tmp[idx + 1]);
                    idx += 2;
                    gp.max_players_per_room = (static_cast<uint16_t>(tmp[idx]) << 8) | static_cast<uint16_t>(tmp[idx + 1]);
                    idx += 2;
                    game_pages[gp.game_id] = gp;
                    game_pages[gp.game_id].LoadTextureFromPNGMemory(gp.image_data);
                }
                else if (data_type == 15) { // get room list
                    room_list.clear();
                    size_t idx = 0;
                    while (idx + 4 <= tmp.size()) {
                        uint32_t room_id = bytes_to_uint32(tmp, idx);
                        idx += 4;
                        uint16_t members_num = bytes_to_uint32(tmp, idx);
                        idx += 4;
                        vector<string> members;
                        for (int i = 0; i < members_num; i++) {
                            uint8_t username_len = tmp[idx];
                            idx += 1;
                            members.push_back(string(tmp.begin() + idx, tmp.begin() + idx + username_len));
                            idx += username_len;
                        }
                        room_list.push_back(make_pair(room_id, members));
                    }
                }
                else if (data_type == 17) { // review updated
                    uint32_t gid = bytes_to_uint32(tmp);
                    net_server.send_packet(12, uint32_to_bytes(gid));
                }
                else if (data_type == 18) { // game page out of date
                    uint32_t old_gid = bytes_to_uint32(tmp, 0), new_gid = bytes_to_uint32(tmp, 4);
                    if (selected_game_id == old_gid) {
                        selected_page_out_of_date = 1;
                        selected_game_id = new_gid;
                        if (new_gid == 0) {
                            selected_page_has_been_removed = 1;
                            launching_state = 0;
                            resend_request = 0;
                        }
                    }
                    game_list.erase(old_gid);
                    game_pages.erase(old_gid);
                    if (new_gid != 0) {
                        game_list.insert(new_gid);
                        game_pages[new_gid] = GamePage();
                    }
                }
                else if (data_type == 19) { // room list out of date (operation fail)
                    net_server.send_packet(14, uint32_to_bytes(selected_game_id));
                }
                else if (data_type == 21) { // purchase successe
                    uint32_t gid = bytes_to_uint32(tmp);
                    if (game_pages.find(gid) != game_pages.end()) {
                        game_pages[gid].owned = 1;
                        userpoints -= game_pages[gid].price;
                    }
                }
                else if (data_type == 23) { // launch game result
                    uint32_t gid = bytes_to_uint32(tmp);
                    uint8_t result = tmp[4];
                    if (result == 0) { // ok, ready to start the game (go to room selection page)
                        net_server.send_packet(14, uint32_to_bytes(selected_game_id));
                        launching_state = 3;
                    }
                    else if (result == 1) // need update
                        net_server.send_packet(24, uint32_to_bytes(gid)); // request download
                }
                else if (data_type == 25) { // download info (the packet right before download data)
                    uint32_t gid = bytes_to_uint32(tmp);
                    download_gid = gid;
                    downloaded_game_version = make_tuple(tmp[4], tmp[5], tmp[6]);
                    downloaded_byte_num = 0; download_total_byte_num = bytes_to_uint32(tmp, 7);
                    launching_state = 2;
                    remove_game_file_and_version(game_pages[download_gid].game_name);
                    string game_folder = "downloads/" + username + "/" + game_pages[download_gid].game_name + "/";
                    filesystem::create_directories(game_folder);
                    download_file_stream.open(game_folder + game_pages[download_gid].game_name + ".exe", std::ios::binary | std::ios::app);
                }
                else if (data_type == 26) { // download data
                    uint32_t datalen = bytes_to_uint32(tmp);
                    // append_bytes_to_game_file(game_pages[download_gid].game_name, tmp, 4);
                    if (download_file_stream.is_open()) {
                        download_file_stream.write(reinterpret_cast<const char*>(tmp.data() + 4), tmp.size() - 4);
                        downloaded_byte_num += datalen;
                    }
                    else
                        ShowToast("Can't open file, download error!", 4, 2);
                    if (downloaded_byte_num == download_total_byte_num) {
                        download_file_stream.close();
                        create_version_file(game_pages[download_gid].game_name, downloaded_game_version);
                        vector<uint8_t> data = uint32_to_bytes(download_gid);
                        data.push_back(get<0>(downloaded_game_version));
                        data.push_back(get<1>(downloaded_game_version));
                        data.push_back(get<2>(downloaded_game_version));
                        net_server.send_packet(22, data);
                    }
                }
                else if (data_type == 29) { // room ready, game start
                    game_room_id = bytes_to_uint32(tmp);
                    launch_the_game(game_pages[selected_game_id].game_name);
                    launching_state = 4;
                }
                else if (data_type == 33) { // game server stopped
                    game_server_stop_reason = string(tmp.begin(), tmp.end());
                    if (launching_state == 4)
                        stop_the_game_process();
                    launching_state = 0;
                    ShowToast("Game Server Stopped: " + game_server_stop_reason, 10, 0);
                }
            }
            // UI
            if (ui_stat == 124) { // Game List UI
                if (refresh_game_list) {
                    game_list.clear();
                    game_pages.clear();
                    net_server.send_packet(10);
                    page_num = 0;
                    refresh_game_list = 0;
                }
                // title
                ImVec2 pos1(io.DisplaySize.x * 0.075f, io.DisplaySize.y * 0.1f);
                ImGui::SetCursorPos(pos1);
                ImGui::PushFont(SystemFont64);
                string title = "The Game Store";
                ImGui::Text("%s", title.c_str());
                ImGui::PopFont();
                // User info
                ImGui::PushFont(SystemFont48);
                ImVec2 pos_u1(io.DisplaySize.x * 0.9f - ImGui::CalcTextSize(username.c_str()).x, pos1.y);
                ImGui::SetCursorPos(pos_u1);
                ImGui::Text(username.c_str());
                ImGui::PopFont();
                ImGui::PushFont(SystemFont32);
                ImVec2 pos_u2(io.DisplaySize.x * 0.9f - ImGui::CalcTextSize(("Points: " + to_string(userpoints)).c_str()).x, pos1.y + 50);
                ImGui::SetCursorPos(pos_u2);
                ImGui::Text(("Points: " + to_string(userpoints)).c_str());
                ImGui::PopFont();
                // Search
                bool search_change = 0;
                ImGui::PushFont(SystemFont32);
                ImVec2 pos_s1(pos1.x, pos1.y + 90);
                ImGui::SetCursorPos(pos_s1);
                ImGui::Text("Search type : ");
                ImGui::SetNextItemWidth(300);
                ImGui::SameLine(); search_change |= ImGui::Combo("##game_list_filter", &game_list_filter_current, game_list_filter, IM_ARRAYSIZE(game_list_filter));
                ImGui::PopFont();
                if (game_list_filter_current == 0) {
                    ImGui::PushFont(SystemFont32);
                    ImGui::BeginDisabled(page_num == 0);
                    ImGui::SameLine(0.0f, 50.0f);
                    if (ImGui::Button("Previous Page")) {
                        page_num--;
                        game_pages.clear();
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled((page_num + 1) * 8 >= game_list.size());
                    ImGui::SameLine(0.0f, 50.0f);
                    if (ImGui::Button("Next Page")) {
                        page_num++;
                        game_pages.clear();
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine(0.0f, 50.0f);
                    if (ImGui::Button("Refresh Game List"))
                        refresh_game_list = 1;
                    ImGui::PopFont();
                    // Game list
                    ImGui::PushFont(SystemFont32);
                    ImVec2 game_page_size(240.0f, 210.0f);
                    for (int i = 0; i < 8; i++) {
                        if (page_num * 8 + i >= game_list.size())
                            break;
                        ImVec2 pos_g1(pos1.x + (i % 4) * (game_page_size.x + 30.0f), pos1.y + 150.0f + (i / 4) * (game_page_size.y + 30.0f));
                        ImGui::SetCursorPos(pos_g1);
                        ImGui::PushID(i + 10000);
                        uint32_t gid = *(game_list.find_by_order(page_num * 8 + i));
                        GamePage& gp = game_pages[gid];
                        if (gp.game_id == 0) {
                            gp.game_id = 1; // mark as requested
                            net_server.send_packet(12, uint32_to_bytes(gid));
                        }
                        bool is_selected = 0;
                        bool clicked = ImGui::Selectable("##Choose", &is_selected, ImGuiSelectableFlags_AllowItemOverlap, game_page_size);
                        ImGui::SetCursorPosX(pos_g1.x + (240 - ImGui::CalcTextSize(gp.game_name.c_str()).x) * 0.5f);
                        ImGui::SetCursorPosY(pos_g1.y);
                        ImGui::Text("%s", gp.game_name.c_str());
                        // game image
                        if (gp.tex_id != 0) {
                            ImGui::SetCursorPosX(pos_g1.x);
                            ImGui::SetCursorPosY(pos_g1.y + 35);
                            ImGui::Image(gp.tex_id, ImVec2(240, 135));
                        }
                        else {
                            ImGui::SetCursorPosX(pos_g1.x + (240 - ImGui::CalcTextSize("Loading...").x) * 0.5f);
                            ImGui::SetCursorPosY(pos_g1.y + 35 + 50);
                            ImGui::Text("Loading...");
                        }
                        string prc = (gp.owned ? "Owned" : (gp.price == 0 ? "Free" : to_string(gp.price) + " pts"));
                        ImGui::SetCursorPosX(pos_g1.x + (240 - ImGui::CalcTextSize(prc.c_str()).x));
                        ImGui::SetCursorPosY(pos_g1.y + 175);
                        ImGui::Text("%s", prc.c_str());
                        if (clicked) {
                            ui_stat = 125;
                            selected_game_id = gp.game_id;
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopFont();
                }
                else {}
            }
            else if (ui_stat == 125) { // Game Page UI
                ImVec2 pos_g1(io.DisplaySize.x * 0.04f, io.DisplaySize.y * 0.1f);
                GamePage& gp = game_pages[selected_game_id];
                if (selected_game_id != 0 && gp.game_id == 0) {
                    gp.game_id = 1; // mark as requested
                    net_server.send_packet(12, uint32_to_bytes(selected_game_id));
                }
                // User info
                ImGui::PushFont(SystemFont48);
                ImVec2 pos_u1(io.DisplaySize.x * 0.96f - ImGui::CalcTextSize(username.c_str()).x, io.DisplaySize.y * 0.1f + ImGui::GetScrollY());
                ImGui::SetCursorPos(pos_u1);
                ImGui::Text(username.c_str());
                ImGui::PopFont();
                ImGui::PushFont(SystemFont32);
                ImVec2 pos_u2(io.DisplaySize.x * 0.96f - ImGui::CalcTextSize(("Points: " + to_string(userpoints)).c_str()).x, io.DisplaySize.y * 0.1f + ImGui::GetScrollY() + 50);
                ImGui::SetCursorPos(pos_u2);
                ImGui::Text(("Points: " + to_string(userpoints)).c_str());
                ImGui::PopFont();
                // update review button
                ImGui::PushFont(SystemFont32);
                ImGui::SetCursorPosX(io.DisplaySize.x * 0.96f - 210);
                ImGui::SetCursorPosY(io.DisplaySize.y * 0.4f + ImGui::GetScrollY());
                ImGui::BeginDisabled(!gp.owned);
                if (ImGui::Button("Update review", ImVec2(210, 60))) {
                    if (!ImGui::IsPopupOpen("Update Review"))
                        ImGui::OpenPopup("Update Review");
                }
                ImGui::EndDisabled();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("Update Review", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    static int rating = 3;
                    static string comment_buf;
                    ImGui::Text("");
                    ImGui::Text("Rating (1~5): ");
                    ImGui::SameLine();
                    ImGui::SliderInt("##rating", &rating, 1, 5);
                    ImGui::Text("");
                    ImGui::Text("Comment: (at most 350 characters)");
                    ImGui::InputTextMultiline("##comment", &comment_buf, ImVec2(-1, 200));
                    if (comment_buf.length() > 350)
                        comment_buf = comment_buf.substr(0, 350);
                    ImGui::SetCursorPosX(150);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
                    if (ImGui::Button("Submit", ImVec2(120, 40))) {
                        vector<uint8_t> data = uint32_to_bytes(gp.game_id);
                        data.push_back(static_cast<uint8_t>(rating));
                        data.insert(data.end(), comment_buf.begin(), comment_buf.end());
                        net_server.send_packet(16, data);
                        comment_buf = "";
                        rating = 3;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine(0, 50);
                    if (ImGui::Button("Cancel", ImVec2(120, 40))) {
                        comment_buf = "";
                        rating = 3;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopFont();
                // Refresh button
                ImGui::PushFont(SystemFont32);
                ImGui::SetCursorPosX(io.DisplaySize.x * 0.96f - 210);
                ImGui::SetCursorPosY(io.DisplaySize.y * 0.75f + ImGui::GetScrollY() + 40);
                if (ImGui::Button("Refresh Page", ImVec2(210, 40)))
                    gp = GamePage(); // reset to request again (game_id = 0)
                ImGui::PopFont();
                // Back button
                ImGui::PushFont(SystemFont32);
                ImGui::SetCursorPosX(io.DisplaySize.x * 0.96f - 210);
                ImGui::SetCursorPosY(io.DisplaySize.y * 0.75f + ImGui::GetScrollY() + 100);
                if (ImGui::Button("<< Back to Store", ImVec2(210, 40))) {
                    ui_stat = 124;
                    selected_game_id = 0;
                }
                ImGui::PopFont();
                // game image
                if (gp.tex_id != 0) {
                    ImGui::SetCursorPosX(pos_g1.x);
                    ImGui::SetCursorPosY(pos_g1.y);
                    ImGui::Image(gp.tex_id, ImVec2(240, 135));
                }
                else {
                    ImGui::PushFont(SystemFont32);
                    ImGui::SetCursorPosX(pos_g1.x + (240 - ImGui::CalcTextSize("Loading...").x) * 0.5f);
                    ImGui::SetCursorPosY(pos_g1.y + 50);
                    ImGui::Text("Loading...");
                    ImGui::PopFont();
                }
                // game title
                ImGui::PushFont(SystemFont48);
                ImGui::SetCursorPosX(pos_g1.x + 270);
                ImGui::SetCursorPosY(pos_g1.y);
                ImGui::Text("%s", gp.game_name.c_str());
                ImGui::PopFont();
                ImGui::PushFont(SystemFont32);
                ImGui::SetCursorPosX(pos_g1.x + 270);
                ImGui::Text("%s", ("v" + to_string(gp.version[0]) + "." + to_string(gp.version[1]) + "." + to_string(gp.version[2])).c_str());
                ImGui::SetCursorPosX(pos_g1.x + 270);
                ImGui::Text("%s", ("Up to " + to_string(gp.max_players_per_room) + " players").c_str());
                ImGui::SetCursorPosX(pos_g1.x + 270);
                ImGui::Text("%s", (gp.publisher + " (" + gp.developer + ")").c_str());
                string purchase_str = gp.owned ? "Play Game" : (gp.price == 0 ? "Free" : "Buy for " + to_string(gp.price) + " pts").c_str();
                ImGui::SetCursorPosX(io.DisplaySize.x * 0.75f - 180);
                ImGui::SetCursorPosY(pos_g1.y + 90);
                if (ImGui::Button(purchase_str.c_str(), ImVec2(180, 60))) {
                    if (gp.owned) {
                        vector<uint8_t> data = uint32_to_bytes(gp.game_id);
                        tuple<uint8_t, uint8_t, uint8_t> version = check_local_game_version(gp.game_name);
                        data.push_back(get<0>(version));
                        data.push_back(get<1>(version));
                        data.push_back(get<2>(version));
                        net_server.send_packet(22, data);
                        if (!ImGui::IsPopupOpen("Launching...")) {
                            ImGui::OpenPopup("Launching...");
                            launching_state = 1;
                        }
                    }
                    else {
                        if (gp.game_id != 0 && gp.game_id != 1) {
                            if (gp.price > userpoints)
                                ShowToast("You do not have enough points to buy this game.", 4, 0);
                            else if (gp.price == 0) {
                                vector<uint8_t> data = uint32_to_bytes(gp.game_id);
                                net_server.send_packet(20, data);
                            }
                            else if (!ImGui::IsPopupOpen("Purchase Confirmation"))
                                ImGui::OpenPopup("Purchase Confirmation");
                        }
                        else
                            ShowToast("Game data is still loading. Please wait a moment and try again.", 4, 0);
                    }
                }
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("Launching...", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    if (launching_state == 1) {
                        if (selected_page_out_of_date && !selected_page_has_been_removed)
                            resend_request = 1;
                        if (resend_request && gp.game_id >= 100) {
                            resend_request = 0;
                            vector<uint8_t> data = uint32_to_bytes(gp.game_id);
                            tuple<uint8_t, uint8_t, uint8_t> version = check_local_game_version(gp.game_name);
                            data.push_back(get<0>(version));
                            data.push_back(get<1>(version));
                            data.push_back(get<2>(version));
                            net_server.send_packet(22, data);
                        }
                        string bartext = "Launching the game, please wait...";
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                        ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.5f - ImGui::CalcTextSize(bartext.c_str()).x * 0.5f);
                        ImGui::Text(bartext.c_str());
                        RenderInfProgressBar();
                    }
                    else if (launching_state == 2) {
                        string bartext = "Downloading... (" + to_string(downloaded_byte_num) + "/" + to_string(download_total_byte_num) + ")";
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                        ImGui::SetCursorPosX((ImGui::GetWindowSize().x * 0.5f - ImGui::CalcTextSize(bartext.c_str()).x) * 0.5f);
                        ImGui::Text(bartext.c_str());
                        float progress_percentage = ((float)downloaded_byte_num) / ((float)download_total_byte_num);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 500) * 0.5f);
                        ImGui::ProgressBar(progress_percentage, ImVec2(500, 30), (to_string((int)(progress_percentage * 100.0)) + "%").c_str());
                    }
                    else if (launching_state == 3) {
                        ImGui::SetCursorPosY(50);
                        ImGui::Text("Join the game room below, or Create a Room.");
                        ImGui::SameLine(0, 50);
                        ImGui::BeginDisabled(room_list.size() >= gp.max_room_number);
                        if (ImGui::Button("Create a Room")) {
                            net_server.send_packet(27, uint32_to_bytes(selected_game_id));
                        }
                        ImGui::EndDisabled();
                        ImGui::Text("");
                        if (ImGui::BeginTable("RoomTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
                            ImGui::TableSetupColumn("##RoomCol1");
                            ImGui::TableSetupColumn("##RoomCol2");
                            ImGui::TableSetupColumn("##RoomCol3");
                            int roomIndex = 0;
                            for (const auto& room : room_list) {
                                ImGui::TableNextColumn();
                                ImGui::PushID(roomIndex);
                                ImGui::BeginGroup();
                                for (const auto& name : room.second)
                                    ImGui::Text("%s", name.c_str());
                                ImGui::Spacing();
                                ImGui::BeginDisabled(room.second.size() >= gp.max_players_per_room);
                                if (ImGui::Button("Join", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                                    vector<uint8_t> data = uint32_to_bytes(selected_game_id);
                                    vector<uint8_t> data2 = uint32_to_bytes(room.first);
                                    data.insert(data.end(), data2.begin(), data2.end());
                                    net_server.send_packet(28, data);
                                }
                                ImGui::EndDisabled();
                                ImGui::EndGroup();
                                ImGui::PopID();
                                roomIndex++;
                            }
                            ImGui::EndTable();
                        }
                    }
                    else if (launching_state == 4) {
                        if (subprocess_alive(&game_process) == 0) {
                            net_server.send_packet(32);
                            launching_state = 0;
                            stop_the_game_process();
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("The Game is running now.").x) * 0.5f);
                        ImGui::Text("The Game is running now.");
                    }
                    else
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("Purchase Confirmation", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    ImGui::Text("");
                    ImGui::SetCursorPosX(50);
                    ImGui::PushTextWrapPos(550.0f);
                    ImGui::TextWrapped("Are you sure you want to purchase \"%s\" for %d points?", gp.game_name.c_str(), gp.price);
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosX(150);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                    if (ImGui::Button("Yes", ImVec2(120, 40))) {
                        vector<uint8_t> data = uint32_to_bytes(gp.game_id);
                        net_server.send_packet(20, data);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0, 50);
                    if (ImGui::Button("No", ImVec2(120, 40)))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::PopFont();
                // The page has been removed
                if (selected_page_has_been_removed && !ImGui::IsPopupOpen("WARNING"))
                    ImGui::OpenPopup("WARNING");
                ImGui::PushFont(SystemFont32);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_Appearing);
                if (ImGui::BeginPopupModal("WARNING", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                    ImGui::Text("");
                    ImGui::SetCursorPosX(50);
                    ImGui::PushTextWrapPos(550.0f);
                    ImGui::TextWrapped("Sorry, this page has been removed.");
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosX(250);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
                    if (ImGui::Button("OK", ImVec2(100, 40))) {
                        ui_stat = 124;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopFont();
                // description
                ImGui::PushFont(SystemFont48);
                ImGui::SetCursorPosX(pos_g1.x);
                ImGui::SetCursorPosY(pos_g1.y + 175);
                ImGui::Text("Description");
                ImGui::PopFont();
                ImGui::PushFont(SystemFont32);
                ImGui::SetCursorPosX(pos_g1.x);
                ImGui::PushTextWrapPos(io.DisplaySize.x * 0.75f);
                ImGui::TextWrapped("%s", gp.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopFont();
                // reviews
                ImGui::PushFont(SystemFont48);
                ImGui::SetCursorPosX(pos_g1.x);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
                ImGui::Text("Recent Reviews");
                ImGui::PopFont();
                ImGui::PushFont(SystemFont32);
                for (auto& review : gp.reviews) {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
                    ImGui::SetCursorPosX(pos_g1.x + 30);
                    ImGui::Text("%s - Rating: %d/5", review.first.c_str(), review.second.first);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                    ImGui::SetCursorPosX(pos_g1.x + 60);
                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.75f);
                    ImGui::TextWrapped("%s", review.second.second.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
                }
                ImGui::PopFont();
            }
        }
        ImGui::End();
    }

    if (ui_stat != 0) {
        double t = ImGui::GetTime();
        int info_number = 0; // 0 warning sign 1 success 2 error
        ImGui::PushFont(SystemFont16);
        for (auto it = toast_info.begin(); it != toast_info.end();) {
            if (t < get<1>(*it)) {
                ImGuiIO& io = ImGui::GetIO();
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
        ImGui::PopFont();
    }

    signed w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(win);
}

void WindowRefreshCallback(GLFWwindow* window) {
    RenderScene(window);
}

int main() {
    // test
    /*for (int i = 1; i <= 5; i++) {
        game_pages[i * 100] = GamePage();
        game_pages[i * 100].game_id = i * 100;
        game_pages[i * 100].game_name = "Game " + to_string(i);
        game_pages[i * 100].developer = "Dev " + to_string(i);
        game_pages[i * 100].publisher = "Pub " + to_string(i);
        game_pages[i * 100].version[0] = 1;
        game_pages[i * 100].version[1] = 0;
        game_pages[i * 100].version[2] = 0;
        game_pages[i * 100].description = "This is a description for Game " + to_string(i) + "." + "This is a longer description to showcase the text wrapping functionality in the game store UI.";
        game_pages[i * 100].price = (i - 1) * 150;
        for (int j = 0; j < 7; j++)
            game_pages[i * 100].reviews.push_back(make_pair("User " + to_string(j), make_pair(5, "The game offers a rich atmosphere and engaging mechanics that pull you in from the very beginning. Its world design and soundtrack work beautifully together, creating an experience that feels both immersive and emotionally resonant. While a few technical issues appear occasionally, the overall adventure remains highly enjoyable and worth your time.")));
        game_list.insert(i * 100);
    }
    username = "TestUser1234";
    userpoints = 1000000;
    selected_game_id = 100;
    refresh_game_list = 0;*/

    // GUI shading, backend: OpenGL3, GLFW
    glfwInit(); // OpenGL3,GLFW initialization
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Eason's Game Store system", 0, 0);
    // glfwSetWindowSizeLimits(win, 800, 600, 3840, 2160);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // v-sync on

    glfwSetWindowRefreshCallback(win, WindowRefreshCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330"); // OpenGL version

    SystemFont16 = io.Fonts->AddFontFromFileTTF("fonts/NotoSansTC-Regular.ttf", 16.0f);
    SystemFont32 = io.Fonts->AddFontFromFileTTF("fonts/NotoSansTC-Regular.ttf", 32.0f);
    SystemFont48 = io.Fonts->AddFontFromFileTTF("fonts/NotoSansTC-Regular.ttf", 48.0f);
    SystemFont64 = io.Fonts->AddFontFromFileTTF("fonts/NotoSansTC-Regular.ttf", 64.0f);

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        RenderScene(win);
        if (ui_stat == -1)
            break;
    }

    if (net_server.connection_alive() && username != "")
        net_server.send_packet(5);

    if (subprocess_alive(&game_process)) {
        stop_the_game_process();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

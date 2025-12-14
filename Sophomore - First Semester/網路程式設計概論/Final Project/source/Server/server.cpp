#define SERVER_SERVICE_PORT 52023
#include <functional>
#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <thread>
#include <mutex>
#include <chrono>
#include "asio/asio.hpp"
#include "subprocess/subprocess.h"

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

For developer only:
50 types: 1 - game page data, 2 - game file size (server cpp and player exe), 3 - server cpp chunk, 4 - player exe chunk (at most 1kB per chunk)
50 [Client] Upload a new game (type, game name, game image, developer, description, price, max room number, max players per room, server.cpp, player.exe)
51 [Server] new game ack (new game id)
52 [Client] Public a game (game id)
53 [Server] public a game ack (game id)
54 [Client] Take down a game (game id)
55 [Server] game removed from game list ack (game id)
56 [Client] Update gamepage (game id, developer, description, price)
57 [Server] gamepage updated ack (new game id)
58 types: 1 - release info (version type, max room number, max players per room), 2 - game file size (server cpp and player exe), 3 - new server cpp chunk, 4 new player exe chunk (at most 1kB per chunk)
58 [Client] Update gamefile and new version (game id, type, version type, max room number, max players per room, new server cpp, new player exe)
59 [Server] gamefile updated ack (new game id)

// Compile command
// Linux: g++ server.cpp -I asio -std=c++20 -o server
*/

class User {
private:
    static uint32_t uid_cnt;
public:
    bool player_or_developer = 0; // 0: player, 1: developer
    uint32_t user_id = uid_cnt++;
    string account, name, password;
    // game file id refer to game file, while game id refer to game page
    // gid/100 = gf_id
    set<uint32_t> owned_game_file_IDs;
    uint32_t points = 100;
};
uint32_t User::uid_cnt = 1;

class Client_Info {
public:
    uint32_t user_id = 0;
    uint32_t in_game_gfid = 0;
    uint32_t in_game_room_id = 0;
    /*
    client_stat definition:
        0: Unqualified
        1: Online and qualified
        2: In game
    */
    bool player_or_developer = 0;
    int client_stat = 0;
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

map<uint32_t, User> user_id_to_User;
map<string, uint32_t> account_to_user_id;
map<string, bool> used_account, used_username, online_account;

void write_log(string filename, string content) { // Game Log
    ofstream out(filename + ".gamelog", std::ios::app);
    out << content << '\n'
        << endl;
    out.close();
}

void save_users(string filename, vector<User>& us) { // Save users to userdb
    ofstream out(filename + ".userdb");
    for (auto& v : us) {
        out << v.player_or_developer << '\n';
        out << v.account << '\n';
        out << v.name << '\n';
        out << v.password << '\n';

        int num = v.owned_game_file_IDs.size(), idx = 0;
        if (num == 0)
            out << '\n';
        else
            for (auto& gfid : v.owned_game_file_IDs) {
                out << gfid << " \n"[num == ++idx];
            }
        out << v.points << '\n';
    }
    out.close();
}

void read_users(string filename) { // Read users from userdb
    ifstream in(filename + ".userdb");
    string tmp;
    while (getline(in, tmp)) {
        User u;
        u.player_or_developer = stoll(tmp);
        getline(in, u.account);
        getline(in, u.name);
        getline(in, u.password);
        getline(in, tmp);
        stringstream ss(tmp);
        string gfid;
        while (ss >> gfid)
            u.owned_game_file_IDs.insert(stoll(gfid));
        getline(in, tmp);
        u.points = stoll(tmp);
        used_account[u.account] = 1;
        used_username[u.name] = 1;
        user_id_to_User[u.user_id] = u;
        account_to_user_id[u.account] = u.user_id;
    }
}

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

class GamePage {
public:
    uint32_t game_id = 0; // game page id
    vector<uint8_t> image_data;
    string game_name;
    string developer;
    string publisher;
    uint8_t version[3] = {}; // major, minor, patch
    string description;
    uint32_t price = 0;

    list<pair<string, pair<uint8_t, string>>> reviews; // user, rating, comment

    uint16_t max_room_number = 0;
    uint16_t max_players_per_room = 0;

    uint32_t sales = 0;
};

class Room {
public:
    uint32_t room_id, gfid;
    map<uint32_t, string> players; // player id -> username
};

tuple<uint8_t, uint8_t, uint8_t> check_local_game_version(string& game_name) {
    string game_folder = "games/" + game_name + "/";
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

// Not used, ofstream will overwrite the file if it exists
/*void remove_game_file_server_file_and_version(string& game_name) {
    string game_folder = "games/" + game_name + "/";
    filesystem::remove(game_folder + "version");
    filesystem::remove(game_folder + game_name + ".exe");
    filesystem::remove(game_folder + game_name);
}*/

void create_version_file(const string& game_name, tuple<uint8_t, uint8_t, uint8_t> version) {
    string game_folder = "games/" + game_name + "/";
    filesystem::create_directories(game_folder);
    ofstream file(game_folder + "version");
    if (!file)
        return;
    string v_str = ("v" + to_string(get<0>(version)) + "." + to_string(get<1>(version)) + "." + to_string(get<2>(version)));
    file << v_str;
    file.close();
}

static uint32_t gf_id = 1;
od_map<uint32_t, GamePage> gid_to_game_page, gid_to_none_public_game_page;
set<string> used_game_names;

int count_line(string& str) {
    int cnt = 1;
    for (int i = 0;i < str.length();i++)
        if (str[i] == '\n')
            cnt++;
    return cnt;
}

void read_game_page(string& game_name) {
    string game_folder = "games/" + game_name + "/";
    GamePage gp;
    gp.game_id = (gf_id++) * 100;
    string tmp;
    ifstream in(game_folder + game_name + ".gamepage");
    if (!in.is_open()) {
        cerr << "read_game_page: cannot open file: "
            << game_folder + game_name + ".gamepage" << '\n';
        return;
    }
    bool public_;
    getline(in, tmp);
    public_ = stoll(tmp);
    getline(in, gp.game_name);
    used_game_names.insert(gp.game_name);
    getline(in, gp.developer);
    getline(in, gp.publisher);
    getline(in, tmp);
    gp.version[0] = stoll(tmp);
    getline(in, tmp);
    gp.version[1] = stoll(tmp);
    getline(in, tmp);
    gp.version[2] = stoll(tmp);
    getline(in, tmp);
    int line_num = stoll(tmp);
    for (int i = 0;i < line_num;i++) {
        getline(in, tmp);
        gp.description += tmp + (i == line_num - 1 ? "" : "\n");
    }
    getline(in, tmp);
    gp.price = stoll(tmp);
    getline(in, tmp);
    int review_num = stoll(tmp);
    for (int i = 0; i < review_num; i++) {
        getline(in, tmp);
        string rating_user = tmp;
        getline(in, tmp);
        uint8_t rating = stoll(tmp);
        getline(in, tmp);
        int comment_line_num = stoll(tmp);
        string comment;
        for (int i = 0; i < comment_line_num; i++) {
            getline(in, tmp);
            comment += tmp + (i == comment_line_num - 1 ? "" : "\n");
        }
        gp.reviews.push_back(make_pair(rating_user, make_pair(rating, comment)));
    }
    getline(in, tmp);
    gp.max_room_number = stoll(tmp);
    getline(in, tmp);
    gp.max_players_per_room = stoll(tmp);
    getline(in, tmp);
    gp.sales = stoll(tmp);
    ifstream in2(game_folder + gp.game_name + ".png");
    if (!in2.is_open()) {
        cerr << "read_game_page: cannot open file: "
            << game_folder + game_name + ".png" << '\n';
        return;
    }
    gp.image_data = vector<uint8_t>((istreambuf_iterator<char>(in2)), istreambuf_iterator<char>());
    if (public_)
        gid_to_game_page[gp.game_id] = gp;
    else
        gid_to_none_public_game_page[gp.game_id] = gp;
}

void save_game_page(GamePage& gp, bool public_) {
    string game_folder = "games/" + gp.game_name + "/";
    filesystem::create_directories(game_folder);
    ofstream out(game_folder + gp.game_name + ".gamepage");
    out << public_ << '\n';
    out << gp.game_name << '\n';
    out << gp.developer << '\n';
    out << gp.publisher << '\n';
    out << (int)gp.version[0] << '\n';
    out << (int)gp.version[1] << '\n';
    out << (int)gp.version[2] << '\n';
    out << count_line(gp.description) << '\n';
    out << gp.description << '\n';
    out << gp.price << '\n';
    out << gp.reviews.size() << '\n';
    for (auto& review : gp.reviews) {
        out << review.first << '\n';
        out << (int)review.second.first << '\n';
        out << count_line(review.second.second) << '\n';
        out << review.second.second << '\n';
    }
    out << gp.max_room_number << '\n';
    out << gp.max_players_per_room << '\n';
    out << gp.sales << '\n';
    out.close();
    ofstream out2(game_folder + gp.game_name + ".png");
    out2.write(reinterpret_cast<const char*>(gp.image_data.data()), gp.image_data.size());
    out2.close();
}

void save_game_list(string filename) {
    ofstream out(filename + ".gamedb");
    for (auto& gp : gid_to_game_page) {
        out << gp.second.game_name << '\n';
        save_game_page(gp.second, 1);
    }
    for (auto& gp : gid_to_none_public_game_page) {
        out << gp.second.game_name << '\n';
        save_game_page(gp.second, 0);
    }
    out.close();
}

void read_game_list(string filename) {
    ifstream in(filename + ".gamedb");
    string tmp;
    while (getline(in, tmp)) {
        read_game_page(tmp);
    }
}

class NetServer;
map<uint32_t, NetServer*> user_id_to_NetServer;
map<uint32_t, int> user_id_to_client_stat;

map<uint32_t, Room> room_id_to_Room;

map<uint32_t, set<uint32_t>> gfid_to_room_IDs;

class NetServer {
private:
    class GameServer {
    private:
        vector<uint8_t> game_read_buf; // 256kB per user
        tcp::acceptor gs_acp;
        asio::steady_timer activity_timer_;
        asio::steady_timer alive_timer_;
        asio::ip::tcp::socket game_socket_;
        subprocess_s game_process{};
        uint32_t game_server_forwarding_data_len;
        uint32_t forwarding_user_id;
        uint32_t my_gfid;
        uint32_t player_cnt;
        mutex game_send_lock_;

        // asynchronous write
        queue<vector<uint8_t>> game_write_queue;
        void game_do_write() {
            asio::async_write(game_socket_, asio::buffer(game_write_queue.front().data(), game_write_queue.front().size()), [this](std::error_code ec, std::size_t) {
                if (!ec) {
                    game_write_queue.pop();
                    if (!game_write_queue.empty())
                        game_do_write();
                }
            });
        }

        void server_inactive_timer() {
            activity_timer_.expires_after(5min); // 5 mins
            activity_timer_.async_wait([this](asio::error_code ec) {
                if (!ec) {
                    stop_server("Game server stopped due to inactivity.");
                }
            });
        }

        void server_alive_check() {
            alive_timer_.expires_after(5s);
            alive_timer_.async_wait([this](asio::error_code ec) {
                if (!ec) {
                    if (subprocess_alive(&game_process) == 0) {
                        stop_server("Game server stopped for unknown reason. Internal error.");
                    }
                    else
                        server_alive_check();
                }
            });
        }

        void game_server_msg_forwarding() {
            asio::async_read(game_socket_, asio::buffer(game_read_buf.data(), 8), [this](asio::error_code ec, size_t) {
                if (!ec) {
                    forwarding_user_id = bytes_to_uint32(game_read_buf);
                    game_server_forwarding_data_len = bytes_to_uint32(game_read_buf, 4);
                    asio::async_read(game_socket_, asio::buffer(game_read_buf.data(), game_server_forwarding_data_len), [this](asio::error_code ec, size_t) {
                        if (!ec) {
                            if (user_id_to_NetServer.find(forwarding_user_id) != user_id_to_NetServer.end() && user_id_to_NetServer[forwarding_user_id]->ThisClient.in_game_gfid == my_gfid)
                                user_id_to_NetServer[forwarding_user_id]->send_packet(31, vector<uint8_t>(game_read_buf.begin(), game_read_buf.begin() + game_server_forwarding_data_len));
                            game_server_msg_forwarding();
                        }
                    });
                }
            });
        }

        void launch_the_game(string& game_name) {
            server_inactive_timer();
            gs_acp.async_accept(game_socket_, [this](asio::error_code ec) {
                game_server_msg_forwarding();
                gs_acp.close();
            });
            uint16_t game_process_port = gs_acp.local_endpoint().port();
            string game_folder = "games/" + game_name + "/";
            string execute_path = "./" + game_folder + game_name;
            string the_port = to_string(game_process_port);
            const char* cmd[] = { execute_path.c_str(), the_port.c_str(), NULL };
            int rc = subprocess_create(cmd, 0, &game_process);
            if (rc != 0) {
                // error
                return;
            }
            server_alive_check();
        }
        void send_bytes(vector<uint8_t>&& bytes) {
            unique_lock<mutex> lk(game_send_lock_);
            vector<uint8_t> data_to_send(uint32_to_bytes(bytes.size()));
            data_to_send.insert(data_to_send.end(), bytes.begin(), bytes.end());
            if (game_write_queue.empty()) {
                game_write_queue.push(data_to_send);
                game_do_write();
            }
            else
                game_write_queue.push(data_to_send);
        }

    public:
        GameServer(uint32_t gfid, string& game_name) :
            activity_timer_(client_ioc_),
            alive_timer_(client_ioc_),
            game_socket_(client_ioc_),
            gs_acp(client_ioc_, tcp::endpoint(tcp::v4(), 0))
        {
            game_read_buf.resize(262144);
            my_gfid = gfid;
            player_cnt = 0;
            launch_the_game(game_name);
        }

        void stop_server(string reason_and_notify = "Server stopped without specific reason.") {
            game_socket_.close();
            activity_timer_.cancel();
            alive_timer_.cancel();
            set<uint32_t>& room_IDs = gfid_to_room_IDs[my_gfid];
            for (auto& room_id : room_IDs) {
                Room& the_room = room_id_to_Room[room_id];
                for (auto& player : the_room.players) {
                    if (user_id_to_NetServer.find(player.first) != user_id_to_NetServer.end()) {
                        Client_Info& the_client = user_id_to_NetServer[player.first]->ThisClient;
                        user_id_to_NetServer[player.first]->send_packet(33, vector<uint8_t>(reason_and_notify.begin(), reason_and_notify.end()));
                        the_client.client_stat = 1;
                        the_client.in_game_room_id = 0;
                        the_client.in_game_gfid = 0;
                    }
                }
                room_id_to_Room.erase(room_id);
            }
            if (subprocess_alive(&game_process) > 0) {
                subprocess_terminate(&game_process);
                subprocess_join(&game_process, nullptr);
            }
            subprocess_destroy(&game_process);
            gfid_to_GameServer.erase(my_gfid);
            gfid_to_room_IDs.erase(my_gfid);
            delete this;
        }

        void player_join_room(uint32_t player_id, uint32_t room_id, string player_name) {
            vector<uint8_t> data, tmp;
            data.push_back(1);
            tmp = uint32_to_bytes(player_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            tmp = uint32_to_bytes(room_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            data.insert(data.end(), player_name.begin(), player_name.end());
            send_bytes(move(data));
        }
        void player_create_room(uint32_t player_id, uint32_t room_id, string player_name) {
            vector<uint8_t> data, tmp;
            data.push_back(2);
            tmp = uint32_to_bytes(player_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            tmp = uint32_to_bytes(room_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            data.insert(data.end(), player_name.begin(), player_name.end());
            send_bytes(move(data));
        }
        void player_leave(uint32_t player_id) {
            vector<uint8_t> data, tmp;
            data.push_back(3);
            tmp = uint32_to_bytes(player_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            send_bytes(move(data));
        }
        void send_message(uint32_t player_id, const vector<uint8_t>& msg) {
            vector<uint8_t> data, tmp;
            data.push_back(4);
            tmp = uint32_to_bytes(player_id);
            data.insert(data.end(), tmp.begin(), tmp.end());
            data.insert(data.end(), msg.begin(), msg.end());
            send_bytes(move(data));
        }

        void add_player_in_game() {
            player_cnt++;
            activity_timer_.cancel();
        }
        void decrease_player_in_game() {
            player_cnt--;
            if (player_cnt == 0)
                server_inactive_timer();
        }
    };

    static map<uint32_t, GameServer*> gfid_to_GameServer;
    static set<NetServer*> MyClients;
    static tcp::acceptor acp;
    vector<uint8_t> read_buf; // single thread, static safe

    asio::steady_timer timer_;
    asio::ip::tcp::socket socket_;

    uint32_t received_data_len;
    mutex send_lock_;

    ifstream download_stream;
    ofstream upload_stream;

    queue<vector<uint8_t>> write_queue;
    void do_write() {
        asio::async_write(socket_, asio::buffer(write_queue.front().data(), write_queue.front().size()), [&](std::error_code ec, std::size_t) {
            if (!ec) {
                write_queue.pop();
                if (!write_queue.empty()) {
                    do_write();
                }
            }
        });
    }

    function<uint32_t(od_map<uint32_t, GamePage>&, uint32_t)> get_next_gid =
        [&](od_map<uint32_t, GamePage>& mp, uint32_t requested_gid) -> uint32_t {
        auto it = mp.lower_bound(requested_gid + 1);
        if (it != mp.end() && it->first / 100 == requested_gid / 100)
            return it->first;
        return 0;
    };

    // for upload
    uint8_t upload_state = 0;
    GamePage uploading_game_page;
    uint32_t uploading_server_file_size = 0, uploading_server_file_size_received = 0;
    uint32_t uploading_player_file_size = 0, uploading_player_file_size_received = 0;
    uint32_t update_game_id = 0;

    void StartService() {
        timer_.expires_after(5s);
        timer_.async_wait([this](asio::error_code ec) {
            if (!ec) {
                socket_.close();
            }
        });
        asio::async_read(socket_, asio::buffer(read_buf.data(), 4), [this](asio::error_code ec, size_t) {
            timer_.cancel();
            if (!ec) {
                received_data_len = ntohl((read_buf[0] << 24) + (read_buf[1] << 16) + (read_buf[2] << 8) + read_buf[3]);
                asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t) {
                    uint8_t data_type = read_buf[0];
                    vector<uint8_t> data;
                    if (data_type == 1) {
                        send_packet(2);
                    }
                    else if (data_type == 3) {
                        uint8_t usr_len = read_buf[1];
                        uint8_t acc_len = read_buf[2];
                        uint8_t pwd_len = read_buf[3];
                        string username(read_buf.begin() + 4, read_buf.begin() + 4 + usr_len);
                        string account(read_buf.begin() + 4 + usr_len, read_buf.begin() + 4 + usr_len + acc_len);
                        string password(read_buf.begin() + 4 + usr_len + acc_len, read_buf.begin() + 4 + usr_len + acc_len + pwd_len);
                        if (used_username[username])
                            send_reject("Username has been used!");
                        else if (used_account[account])
                            send_reject("Account has been used!");
                        else {
                            User new_user;
                            new_user.player_or_developer = read_buf[4 + usr_len + acc_len + pwd_len];
                            new_user.name = username;
                            new_user.account = account;
                            new_user.password = password;
                            used_username[username] = 1;
                            used_account[account] = 1;
                            user_id_to_User[new_user.user_id] = new_user;
                            account_to_user_id[account] = new_user.user_id;
                            send_packet(6);
                            write_log("TheGameStore", account + " registered.");
                        }
                    }
                    else if (data_type == 4) {
                        uint8_t acc_len = read_buf[1];
                        uint8_t pwd_len = read_buf[2];
                        string account(read_buf.begin() + 3, read_buf.begin() + 3 + acc_len);
                        string password(read_buf.begin() + 3 + acc_len, read_buf.begin() + 3 + acc_len + pwd_len);
                        uint8_t client_type = read_buf[3 + acc_len + pwd_len];
                        if (account_to_user_id.find(account) == account_to_user_id.end() || user_id_to_User[account_to_user_id[account]].password != password)
                            send_reject("Account/Password is incorrect.");
                        else if (online_account[account])
                            send_reject("You are NOT ALLOW to login twice in different process!");
                        else {
                            User the_user = user_id_to_User[account_to_user_id[account]];
                            if (client_type == the_user.player_or_developer) {
                                online_account[account] = 1;
                                user_id_to_NetServer[the_user.user_id] = this;
                                data = uint32_to_bytes(the_user.points);
                                data.insert(data.end(), the_user.name.begin(), the_user.name.end());
                                send_packet(6, data);
                                write_log("TheGameStore", account + " login.");
                                user_id_to_client_stat[the_user.user_id] = 1;
                                ThisClient.client_stat = 1;
                                ThisClient.user_id = the_user.user_id;
                                ThisClient.player_or_developer = the_user.player_or_developer;
                            }
                            else if (client_type == 0)
                                send_reject("This is Developer account, please use the DevGameStore client instead.");
                            else if (client_type == 1)
                                send_reject("This is Player account, please use the GameStore client instead");
                        }
                    }
                    else if (data_type == 5) {
                        // next StartService() will result in ec, let the branch handle logout
                        stop();
                    }
                    else if (data_type == 10) { // request game list
                        if (ThisClient.player_or_developer == 0) {
                            for (auto& gp : gid_to_game_page) {
                                vector<uint8_t> gid_tmp = uint32_to_bytes(gp.first);
                                data.insert(data.end(), gid_tmp.begin(), gid_tmp.end());
                            }
                        }
                        else {
                            set<uint32_t>& my_dev_games = user_id_to_User[ThisClient.user_id].owned_game_file_IDs;
                            for (auto& gfid : my_dev_games) {
                                uint32_t target_search_key = (gfid + 1) * 100;

                                // --- 修正開始：檢查 Public 遊戲 ---
                                if (!gid_to_game_page.empty()) {
                                    size_t idx = gid_to_game_page.order_of_key(target_search_key);
                                    // 關鍵修正：必須 idx > 0 才能減 1，否則會溢位
                                    if (idx > 0) {
                                        auto it = gid_to_game_page.find_by_order(idx - 1);
                                        // 再次確認找到的遊戲確實屬於這個 gfid
                                        if (it != gid_to_game_page.end() && it->first / 100 == gfid) {
                                            uint32_t gfid_game_id = it->first;
                                            vector<uint8_t> gid_tmp = uint32_to_bytes(gfid_game_id);
                                            data.insert(data.end(), gid_tmp.begin(), gid_tmp.end());
                                        }
                                    }
                                }

                                // --- 修正開始：檢查 Private (Non-Public) 遊戲 ---
                                if (!gid_to_none_public_game_page.empty()) {
                                    size_t idx = gid_to_none_public_game_page.order_of_key(target_search_key);
                                    // 關鍵修正：必須 idx > 0 才能減 1
                                    if (idx > 0) {
                                        auto it = gid_to_none_public_game_page.find_by_order(idx - 1);
                                        // 再次確認
                                        if (it != gid_to_none_public_game_page.end() && it->first / 100 == gfid) {
                                            uint32_t gfid_game_id = it->first;
                                            vector<uint8_t> gid_tmp = uint32_to_bytes(gfid_game_id);
                                            data.insert(data.end(), gid_tmp.begin(), gid_tmp.end());
                                        }
                                    }
                                }
                                // --- 修正結束 ---
                            }
                        }
                        send_packet(11, data);
                    }
                    else if (data_type == 12) { // request game detail (game page)
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        bool found_page = 1;
                        if (ThisClient.player_or_developer == 0)
                            found_page = gid_to_game_page.find(requested_gid) != gid_to_game_page.end();
                        else
                            found_page = (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) || (gid_to_none_public_game_page.find(requested_gid) != gid_to_none_public_game_page.end());
                        bool public_ = (gid_to_game_page.find(requested_gid) != gid_to_game_page.end());
                        if (found_page) {
                            GamePage& gp = (public_ ? gid_to_game_page[requested_gid] : gid_to_none_public_game_page[requested_gid]);
                            vector<uint8_t> tmp = uint32_to_bytes(gp.game_id);
                            data.insert(data.end(), tmp.begin(), tmp.end());
                            tmp = uint32_to_bytes(gp.image_data.size());
                            data.insert(data.end(), tmp.begin(), tmp.end());
                            data.insert(data.end(), gp.image_data.begin(), gp.image_data.end());
                            data.push_back(gp.game_name.length());
                            data.insert(data.end(), gp.game_name.begin(), gp.game_name.end());
                            data.push_back(gp.developer.length());
                            data.insert(data.end(), gp.developer.begin(), gp.developer.end());
                            data.push_back(gp.publisher.length());
                            data.insert(data.end(), gp.publisher.begin(), gp.publisher.end());
                            data.push_back(gp.version[0]);
                            data.push_back(gp.version[1]);
                            data.push_back(gp.version[2]);
                            data.push_back(gp.description.length() / 256);
                            data.push_back(gp.description.length() % 256);
                            data.insert(data.end(), gp.description.begin(), gp.description.end());
                            tmp = uint32_to_bytes(gp.price);
                            data.insert(data.end(), tmp.begin(), tmp.end());
                            if (ThisClient.player_or_developer == 0) // for player
                                data.push_back(user_id_to_User[ThisClient.user_id].owned_game_file_IDs.count(requested_gid / 100));
                            else // for developer
                                data.push_back(public_);
                            data.push_back(gp.reviews.size());
                            for (auto& review : gp.reviews) {
                                data.push_back(review.first.length());
                                data.insert(data.end(), review.first.begin(), review.first.end());
                                data.push_back(review.second.first);
                                data.push_back(review.second.second.length() / 256);
                                data.push_back(review.second.second.length() % 256);
                                data.insert(data.end(), review.second.second.begin(), review.second.second.end());
                            }
                            data.push_back(gp.max_room_number / 256);
                            data.push_back(gp.max_room_number % 256);
                            data.push_back(gp.max_players_per_room / 256);
                            data.push_back(gp.max_players_per_room % 256);
                            if (ThisClient.player_or_developer) { // for developer
                                tmp = uint32_to_bytes(gp.sales);
                                data.insert(data.end(), tmp.begin(), tmp.end());
                            }
                            send_packet(13, data);
                        }
                        else {
                            uint32_t next_gid = 0;
                            next_gid = get_next_gid(gid_to_game_page, requested_gid);
                            if (next_gid == 0 && ThisClient.player_or_developer == 1)
                                next_gid = get_next_gid(gid_to_none_public_game_page, requested_gid);
                            vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(next_gid);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);
                        }
                    }
                    else if (data_type == 14) { // request room list (of a game)
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        if (gfid_to_GameServer.find(requested_gid / 100) != gfid_to_GameServer.end()) {
                            if (gfid_to_room_IDs.find(requested_gid / 100) != gfid_to_room_IDs.end()) {
                                set<uint32_t>& IDs = gfid_to_room_IDs[requested_gid / 100];
                                for (auto room_id : IDs) {
                                    vector<uint8_t> tmp = uint32_to_bytes(room_id);
                                    data.insert(data.end(), tmp.begin(), tmp.end());
                                    tmp = uint32_to_bytes(room_id_to_Room[room_id].players.size());
                                    data.insert(data.end(), tmp.begin(), tmp.end());
                                    map<uint32_t, string>& players = room_id_to_Room[room_id].players;
                                    for (auto player : players) {
                                        data.push_back(player.second.length());
                                        data.insert(data.end(), player.second.begin(), player.second.end());
                                    }
                                }
                            }
                            send_packet(15, data);
                        }
                        else {
                            // 33 is the sign for stop game launching, 18 might be better
                            string msg = "The game server stopped working a while ago. Please try to refresh the page.";
                            data = vector<uint8_t>(msg.begin(), msg.end());
                            send_packet(33, data);
                            LeaveGame();
                            /*vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(gid_to_game_page.find_by_order(gid_to_game_page.order_of_key(requested_gid) + 1) != gid_to_game_page.end()
                                && ((gid_to_game_page.find_by_order(gid_to_game_page.order_of_key(requested_gid) + 1))->first) / 100 == requested_gid / 100 ? ((gid_to_game_page.find_by_order(gid_to_game_page.order_of_key(requested_gid) + 1))->first) : 0);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);*/
                        }
                    }
                    else if (data_type == 16) { // update review
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                            list<pair<string, pair<uint8_t, string>>>& reviews = gid_to_game_page[requested_gid].reviews;
                            string& my_name = user_id_to_User[ThisClient.user_id].name;
                            uint8_t my_rating = read_buf[5];
                            string my_comment = string(read_buf.begin() + 6, read_buf.begin() + received_data_len);
                            for (auto it = reviews.begin(); it != reviews.end();) {
                                if (it->first == my_name)
                                    it = reviews.erase(it);
                                else
                                    it++;
                            }
                            if (reviews.size() == 100)
                                reviews.pop_back();
                            reviews.push_front(make_pair(my_name, make_pair(my_rating, my_comment)));
                            send_packet(17, uint32_to_bytes(requested_gid));
                        }
                        else {
                            uint32_t next_gid = 0;
                            next_gid = get_next_gid(gid_to_game_page, requested_gid);
                            if (next_gid == 0 && ThisClient.player_or_developer == 1)
                                next_gid = get_next_gid(gid_to_none_public_game_page, requested_gid);
                            vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(next_gid);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);
                        }
                    }
                    else if (data_type == 20) { // request purchase
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                            if (user_id_to_User[ThisClient.user_id].owned_game_file_IDs.count(requested_gid / 100) == 0) {
                                if (user_id_to_User[ThisClient.user_id].points >= gid_to_game_page[requested_gid].price) {
                                    user_id_to_User[ThisClient.user_id].points -= gid_to_game_page[requested_gid].price;
                                    gid_to_game_page[requested_gid].sales += gid_to_game_page[requested_gid].price;
                                    user_id_to_User[ThisClient.user_id].owned_game_file_IDs.insert(requested_gid / 100);
                                    send_packet(21, uint32_to_bytes(requested_gid));
                                }
                                else
                                    send_reject("You don't have enought points to buy this game.");
                            }
                            else
                                send_reject("Unable to purchase because you own this game.");
                        }
                        else {
                            uint32_t next_gid = 0;
                            next_gid = get_next_gid(gid_to_game_page, requested_gid);
                            if (next_gid == 0 && ThisClient.player_or_developer == 1)
                                next_gid = get_next_gid(gid_to_none_public_game_page, requested_gid);
                            vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(next_gid);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);
                        }
                    }
                    else if (data_type == 22) { // launch the game
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                            if (user_id_to_User[ThisClient.user_id].owned_game_file_IDs.count(requested_gid / 100)) {
                                vector<uint8_t> gid = uint32_to_bytes(requested_gid);
                                data.insert(data.end(), gid.begin(), gid.end());
                                tuple<uint8_t, uint8_t, uint8_t> version = check_local_game_version(gid_to_game_page[requested_gid].game_name);
                                if (get<0>(version) == read_buf[5] && get<1>(version) == read_buf[6] && get<2>(version) == read_buf[7]) {
                                    data.push_back(0);
                                    ThisClient.client_stat = 2;
                                    ThisClient.in_game_gfid = requested_gid / 100;
                                    LaunchGameServer(ThisClient.in_game_gfid, gid_to_game_page[requested_gid].game_name);
                                    gfid_to_GameServer[ThisClient.in_game_gfid]->add_player_in_game();
                                }
                                else
                                    data.push_back(1);
                                send_packet(23, data);
                            }
                            else
                                send_reject("You do not own this game.");
                        }
                        else {
                            uint32_t next_gid = 0;
                            next_gid = get_next_gid(gid_to_game_page, requested_gid);
                            if (next_gid == 0 && ThisClient.player_or_developer == 1)
                                next_gid = get_next_gid(gid_to_none_public_game_page, requested_gid);
                            vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(next_gid);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);
                        }
                    }
                    else if (data_type == 24) { // request game download
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                            vector<uint8_t> tmp = uint32_to_bytes(requested_gid);
                            data.insert(data.end(), tmp.begin(), tmp.end());
                            tuple<uint8_t, uint8_t, uint8_t> version = check_local_game_version(gid_to_game_page[requested_gid].game_name);
                            data.push_back(get<0>(version));
                            data.push_back(get<1>(version));
                            data.push_back(get<2>(version));
                            string filename = "games/" + gid_to_game_page[requested_gid].game_name + "/" + gid_to_game_page[requested_gid].game_name + ".exe";
                            ifstream fs(filename, std::ios::binary | std::ios::ate);
                            if (fs.is_open()) {
                                uint32_t file_size = (uint32_t)fs.tellg();
                                tmp = uint32_to_bytes(file_size);
                                fs.close();
                                data.insert(data.end(), tmp.begin(), tmp.end());
                                send_packet(25, data);
                                download_stream.open(filename, std::ios::binary);
                                if (!download_stream.is_open())
                                    send_reject("SERVER FILE ERROR, please try again later.");
                                else {
                                    vector<char> file_buf(1024);
                                    uint32_t idx = 0;
                                    while (idx + 1024 < file_size) {
                                        data = uint32_to_bytes(1024);
                                        download_stream.read(file_buf.data(), 1024);
                                        data.insert(data.end(), file_buf.begin(), file_buf.end());
                                        send_packet(26, data);
                                        idx += 1024;
                                    }
                                    if (idx < file_size) {
                                        data = uint32_to_bytes(file_size - idx);
                                        download_stream.read(file_buf.data(), file_size - idx);
                                        data.insert(data.end(), file_buf.begin(), file_buf.begin() + (file_size - idx));
                                        send_packet(26, data);
                                    }
                                }
                            }
                            else
                                send_reject("SERVER FILE ERROR, please try again later.");
                        }
                        else {
                            uint32_t next_gid = 0;
                            next_gid = get_next_gid(gid_to_game_page, requested_gid);
                            if (next_gid == 0 && ThisClient.player_or_developer == 1)
                                next_gid = get_next_gid(gid_to_none_public_game_page, requested_gid);
                            vector<uint8_t> old_gid = uint32_to_bytes(requested_gid);
                            vector<uint8_t> new_gid = uint32_to_bytes(next_gid);
                            data.insert(data.end(), old_gid.begin(), old_gid.end());
                            data.insert(data.end(), new_gid.begin(), new_gid.end());
                            send_packet(18, data);
                        }
                    }
                    else if (data_type == 27) { // create a room
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                        static uint32_t room_code_cnt = 1;
                        if (gfid_to_GameServer.find(requested_gid / 100) != gfid_to_GameServer.end()) {
                            gfid_to_GameServer[requested_gid / 100]->player_create_room(ThisClient.user_id, room_code_cnt, user_id_to_User[ThisClient.user_id].name);
                            ThisClient.in_game_room_id = room_code_cnt;
                            room_id_to_Room[room_code_cnt] = Room();
                            room_id_to_Room[room_code_cnt].room_id = room_code_cnt;
                            room_id_to_Room[room_code_cnt].gfid = requested_gid / 100;
                            room_id_to_Room[room_code_cnt].players[ThisClient.user_id] = user_id_to_User[ThisClient.user_id].name;
                            gfid_to_room_IDs[requested_gid / 100].insert(room_code_cnt);
                            send_packet(29, uint32_to_bytes(room_code_cnt));
                            room_code_cnt++;
                        }
                        else {
                            string msg = "The game server stopped working a while ago. Please try to refresh the page.";
                            data = vector<uint8_t>(msg.begin(), msg.end());
                            send_packet(33, data);
                            LeaveGame();
                        }
                    }
                    else if (data_type == 28) { // join a room
                        uint32_t requested_gid = bytes_to_uint32(read_buf, 1), requested_room_id = bytes_to_uint32(read_buf, 5);
                        if (gfid_to_GameServer.find(requested_gid / 100) != gfid_to_GameServer.end()) {
                            if (room_id_to_Room.find(requested_room_id) != room_id_to_Room.end()) {
                                gfid_to_GameServer[requested_gid / 100]->player_join_room(ThisClient.user_id, requested_room_id, user_id_to_User[ThisClient.user_id].name);
                                ThisClient.in_game_room_id = requested_room_id;
                                room_id_to_Room[requested_room_id].players[ThisClient.user_id] = user_id_to_User[ThisClient.user_id].name;
                                send_packet(29, uint32_to_bytes(requested_room_id));
                            }
                            else
                                send_packet(19);
                        }
                        else {
                            string msg = "The game server stopped working a while ago. Please try to refresh the page.";
                            data = vector<uint8_t>(msg.begin(), msg.end());
                            send_packet(33, data);
                            LeaveGame();
                        }
                    }
                    else if (data_type == 30) { // msg to game server
                        if (gfid_to_GameServer.find(ThisClient.in_game_gfid) != gfid_to_GameServer.end()) {
                            gfid_to_GameServer[ThisClient.in_game_gfid]->send_message(ThisClient.user_id, vector<uint8_t>(read_buf.begin() + 1, read_buf.end()));
                        }
                        else {
                            string msg = "The game server stopped working a while ago. Please try to refresh the page.";
                            data = vector<uint8_t>(msg.begin(), msg.end());
                            send_packet(33, data);
                            LeaveGame();
                        }
                    }
                    else if (data_type == 32) { // player close game program
                        LeaveGame();
                    }
                    else if (data_type == 50) { // upload game file
                        if (ThisClient.player_or_developer == 1) {
                            uint8_t upload_type = read_buf[1];
                            if (upload_type == 1)
                                upload_state = 1;
                            if (upload_state == upload_type) {
                                if (upload_state == 1) { // upload game page info
                                    int idx = 2;
                                    uint8_t new_game_name_len = read_buf[idx];
                                    idx++;
                                    string new_game_name(read_buf.begin() + idx, read_buf.begin() + idx + new_game_name_len);
                                    idx += new_game_name_len;
                                    uint32_t img_len = bytes_to_uint32(read_buf, idx);
                                    idx += 4;
                                    vector<uint8_t> image_data(read_buf.begin() + idx, read_buf.begin() + idx + img_len);
                                    idx += img_len;
                                    uint8_t new_game_developer_len = read_buf[idx];
                                    idx++;
                                    string new_game_developer(read_buf.begin() + idx, read_buf.begin() + idx + new_game_developer_len);
                                    idx += new_game_developer_len;
                                    uint8_t new_game_description_len = read_buf[idx];
                                    idx++;
                                    string new_game_description(read_buf.begin() + idx, read_buf.begin() + idx + new_game_description_len);
                                    idx += new_game_description_len;
                                    uint32_t new_game_price = bytes_to_uint32(read_buf, idx);
                                    idx += 4;
                                    uint16_t new_game_max_room_number = (read_buf[idx] << 8) + read_buf[idx + 1];
                                    idx += 2;
                                    uint16_t new_game_max_players_per_room = (read_buf[idx] << 8) + read_buf[idx + 1];
                                    idx += 2;
                                    uploading_game_page.game_name = new_game_name;
                                    uploading_game_page.image_data = image_data;
                                    uploading_game_page.developer = new_game_developer;
                                    uploading_game_page.publisher = user_id_to_User[ThisClient.user_id].name;
                                    uploading_game_page.description = new_game_description;
                                    uploading_game_page.price = new_game_price;
                                    uploading_game_page.version[0] = 0;
                                    uploading_game_page.version[1] = 0;
                                    uploading_game_page.version[2] = 1;
                                    uploading_game_page.max_room_number = new_game_max_room_number;
                                    uploading_game_page.max_players_per_room = new_game_max_players_per_room;
                                    uploading_game_page.sales = 0;
                                    uploading_game_page.reviews.clear();
                                    uploading_game_page.game_id = (gf_id++) * 100;
                                    uploading_server_file_size_received = 0;
                                    uploading_player_file_size_received = 0;
                                    // note: not public yet
                                    upload_state = 2;
                                }
                                else if (upload_state == 2) { // upload game files size info
                                    uploading_server_file_size = bytes_to_uint32(read_buf, 2);
                                    uploading_player_file_size = bytes_to_uint32(read_buf, 6);
                                    string dir_name = "games/" + uploading_game_page.game_name + "/tmp/";
                                    filesystem::create_directories(dir_name);
                                    string server_filename = dir_name + "/" + uploading_game_page.game_name + ".cpp";
                                    upload_stream.open(server_filename, std::ios::binary);
                                    if (!upload_stream.is_open()) {
                                        send_reject("SERVER FILE ERROR, please try again later.");
                                        upload_state = 0;
                                    }
                                    else
                                        upload_state = 3;
                                }
                                else if (upload_state == 3) { // upload server file
                                    uint32_t chunk_size = bytes_to_uint32(read_buf, 2);
                                    upload_stream.write((char*)&read_buf[6], chunk_size);
                                    uploading_server_file_size_received += chunk_size;
                                    if (uploading_server_file_size_received == uploading_server_file_size) {
                                        upload_stream.close();
                                        string player_filename = "games/" + uploading_game_page.game_name + "/tmp/" + uploading_game_page.game_name + ".exe";
                                        upload_stream.open(player_filename, std::ios::binary);
                                        if (!upload_stream.is_open()) {
                                            send_reject("PLAYER FILE ERROR, please try again later.");
                                            upload_state = 0;
                                        }
                                        else
                                            upload_state = 4;
                                    }
                                }
                                else if (upload_state == 4) { // upload player file
                                    uint32_t chunk_size = bytes_to_uint32(read_buf, 2);
                                    upload_stream.write((char*)&read_buf[6], chunk_size);
                                    uploading_player_file_size_received += chunk_size;
                                    if (uploading_player_file_size_received == uploading_player_file_size) {
                                        upload_stream.close();
                                        if (used_game_names.count(uploading_game_page.game_name)) {
                                            send_reject("Game name already used, please change to another name.");
                                            upload_state = 0;
                                        }
                                        else {
                                            used_game_names.insert(uploading_game_page.game_name);
                                            // finish upload, compile server.cpp. command: g++ server.cpp -I games/EasonGS_API/asio -I games/EasonGS_API -std=c++20 -o server
                                            uint32_t uid = ThisClient.user_id;
                                            GamePage gp = uploading_game_page;
                                            // compile in a new thread
                                            thread upload_thread([uid, gp]() {
                                                string folder_name = "games/" + gp.game_name + "/tmp/";
                                                int ret = system(("g++ \"" + folder_name + gp.game_name + ".cpp\" " + " \"games/EasonGS_API/EasonGS_Server.cpp\" " + "-I games/EasonGS_API/asio -I games/EasonGS_API -std=c++20 " +
                                                    "-o \"" + folder_name + gp.game_name + "\"").c_str());
                                                if (ret == 0) {
                                                    rename((folder_name + gp.game_name).c_str(), ("games/" + gp.game_name + "/" + gp.game_name).c_str());
                                                    rename((folder_name + gp.game_name + ".exe").c_str(), ("games/" + gp.game_name + "/" + gp.game_name + ".exe").c_str());
                                                    ofstream output_file("games/" + gp.game_name + "/" + gp.game_name + ".png", std::ios::out | std::ios::binary);
                                                    output_file.write(reinterpret_cast<const char*>(gp.image_data.data()), gp.image_data.size());
                                                    create_version_file(gp.game_name, make_tuple(0, 0, 1));
                                                }
                                                // post the result back to the asio context to avoid race condition
                                                asio::post(client_ioc_, [uid, gp, ret]() {
                                                    if (ret == 0) {
                                                        gid_to_none_public_game_page[gp.game_id] = gp;
                                                        user_id_to_User[uid].owned_game_file_IDs.insert(gp.game_id / 100);
                                                        if (user_id_to_NetServer.find(uid) != user_id_to_NetServer.end())
                                                            user_id_to_NetServer[uid]->send_packet(51, uint32_to_bytes(gp.game_id));
                                                        write_log("TheGameStore", user_id_to_User[uid].account + " uploaded a new game: " + gp.game_name);
                                                    }
                                                    else if (user_id_to_NetServer.find(uid) != user_id_to_NetServer.end())
                                                        user_id_to_NetServer[uid]->send_reject("Compilation error, please check your code and re-upload the files.");
                                                });
                                            });
                                            upload_thread.detach();
                                            upload_state = 0;
                                        }
                                    }
                                }
                            }
                            else
                                send_reject("Upload error, please restart the upload process.");
                        }
                    }
                    else if (data_type == 52) { // public a game
                        if (ThisClient.player_or_developer == 1) {
                            uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                            if (gid_to_none_public_game_page.find(requested_gid) != gid_to_none_public_game_page.end()) {
                                gid_to_game_page[requested_gid] = gid_to_none_public_game_page[requested_gid];
                                gid_to_none_public_game_page.erase(requested_gid);
                                send_packet(53, uint32_to_bytes(requested_gid));
                                write_log("TheGameStore", user_id_to_User[ThisClient.user_id].account + " publish a game: " + gid_to_game_page[requested_gid].game_name);
                            }
                        }
                    }
                    else if (data_type == 54) { // take down a game
                        if (ThisClient.player_or_developer == 1) {
                            uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                            if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                                if (gfid_to_GameServer.find(requested_gid / 100) != gfid_to_GameServer.end())
                                    gfid_to_GameServer[requested_gid / 100]->stop_server("The game has been taken down by the developer.");
                                gid_to_none_public_game_page[requested_gid] = gid_to_game_page[requested_gid];
                                gid_to_game_page.erase(requested_gid);
                                send_packet(55, uint32_to_bytes(requested_gid));
                                write_log("TheGameStore", user_id_to_User[ThisClient.user_id].account + " take down a game: " + gid_to_none_public_game_page[requested_gid].game_name);
                            }
                        }
                    }
                    else if (data_type == 56) { // update game page info
                        if (ThisClient.player_or_developer == 1) {
                            uint32_t requested_gid = bytes_to_uint32(read_buf, 1);
                            if (requested_gid % 100 != 99) {
                                if (gid_to_game_page.find(requested_gid) != gid_to_game_page.end()) {
                                    int idx = 5;
                                    uint8_t new_game_developer_len = read_buf[idx];
                                    idx++;
                                    string new_game_developer(read_buf.begin() + idx, read_buf.begin() + idx + new_game_developer_len);
                                    idx += new_game_developer_len;
                                    uint16_t new_game_description_len = (read_buf[idx] << 8) + read_buf[idx + 1];
                                    idx += 2;
                                    string new_game_description(read_buf.begin() + idx, read_buf.begin() + idx + new_game_description_len);
                                    idx += new_game_description_len;
                                    uint32_t new_game_price = bytes_to_uint32(read_buf, idx);
                                    idx += 4;
                                    gid_to_game_page[requested_gid + 1] = gid_to_game_page[requested_gid];
                                    gid_to_game_page.erase(requested_gid);
                                    GamePage& gp = gid_to_game_page[requested_gid + 1];
                                    gp.game_id = requested_gid + 1;
                                    gp.developer = new_game_developer;
                                    gp.description = new_game_description;
                                    gp.price = new_game_price;
                                    send_packet(57, uint32_to_bytes(requested_gid + 1));
                                    write_log("TheGameStore", user_id_to_User[ThisClient.user_id].account + " update game page info: " + gid_to_game_page[requested_gid + 1].game_name);
                                }
                                else if (gid_to_none_public_game_page.find(requested_gid) != gid_to_none_public_game_page.end()) {
                                    int idx = 5;
                                    uint8_t new_game_developer_len = read_buf[idx];
                                    idx++;
                                    string new_game_developer(read_buf.begin() + idx, read_buf.begin() + idx + new_game_developer_len);
                                    idx += new_game_developer_len;
                                    uint16_t new_game_description_len = (read_buf[idx] << 8) + read_buf[idx + 1];
                                    idx += 2;
                                    string new_game_description(read_buf.begin() + idx, read_buf.begin() + idx + new_game_description_len);
                                    idx += new_game_description_len;
                                    uint32_t new_game_price = bytes_to_uint32(read_buf, idx);
                                    idx += 4;
                                    gid_to_none_public_game_page[requested_gid + 1] = gid_to_none_public_game_page[requested_gid];
                                    gid_to_none_public_game_page.erase(requested_gid);
                                    GamePage& gp = gid_to_none_public_game_page[requested_gid + 1];
                                    gp.game_id = requested_gid + 1;
                                    gp.developer = new_game_developer;
                                    gp.description = new_game_description;
                                    gp.price = new_game_price;
                                    send_packet(57, uint32_to_bytes(requested_gid + 1));
                                    write_log("TheGameStore", user_id_to_User[ThisClient.user_id].account + " update game page info: " + gid_to_none_public_game_page[requested_gid + 1].game_name);
                                }
                                else
                                    send_reject("Internal error. Game not found!");
                            }
                            else // Developer can only update 99 times at most before server restarts
                                send_reject("You\'ve updated this game 99 times, cannot update anymore.");
                        }
                    }
                    else if (data_type == 58) { // update game files
                        if (ThisClient.player_or_developer == 1) {
                            uint8_t upload_type = read_buf[1];
                            if (upload_type == 1)
                                upload_state = 1;
                            if (upload_state == upload_type) {
                                if (upload_state == 1) {
                                    uint32_t requested_gid = bytes_to_uint32(read_buf, 2);
                                    if (requested_gid % 100 != 99) {
                                        if (gid_to_none_public_game_page.find(requested_gid) != gid_to_none_public_game_page.end()) {
                                            tuple<uint8_t, uint8_t, uint8_t> current_version = check_local_game_version(gid_to_none_public_game_page[requested_gid].game_name), new_version;
                                            uint8_t new_version_type = read_buf[6]; // 0: major, 1: minor, 2: patch
                                            bool valid_version_update = true;
                                            if (new_version_type == 0) { // major
                                                if (get<0>(current_version) < 255)
                                                    new_version = make_tuple(get<0>(current_version) + 1, 0, 0);
                                                else
                                                    valid_version_update = false;
                                            }
                                            else if (new_version_type == 1) { // minor
                                                if (get<1>(current_version) < 255)
                                                    new_version = make_tuple(get<0>(current_version), get<1>(current_version) + 1, 0);
                                                else
                                                    valid_version_update = false;
                                            }
                                            else if (new_version_type == 2) { // patch
                                                if (get<2>(current_version) < 255)
                                                    new_version = make_tuple(get<0>(current_version), get<1>(current_version), get<2>(current_version) + 1);
                                                else
                                                    valid_version_update = false;
                                            }
                                            else
                                                valid_version_update = false;
                                            if (valid_version_update) {
                                                uploading_server_file_size_received = 0;
                                                uploading_player_file_size_received = 0;
                                                uint16_t new_game_max_room_number = (read_buf[7] << 8) + read_buf[8];
                                                uint16_t new_game_max_players_per_room = (read_buf[9] << 8) + read_buf[10];
                                                uploading_game_page = gid_to_none_public_game_page[requested_gid];
                                                uploading_game_page.game_id = requested_gid + 1;
                                                uploading_game_page.version[0] = get<0>(new_version);
                                                uploading_game_page.version[1] = get<1>(new_version);
                                                uploading_game_page.version[2] = get<2>(new_version);
                                                uploading_game_page.max_room_number = new_game_max_room_number;
                                                uploading_game_page.max_players_per_room = new_game_max_players_per_room;
                                                upload_state = 2;
                                            }
                                            else
                                                send_reject("Invalid version update.");
                                        }
                                        else
                                            send_reject("Internal error. Game not found!");
                                    }
                                    else
                                        send_reject("You\'ve updated this game 99 times, cannot update anymore.");
                                }
                                else if (upload_state == 2) { // upload game files size info
                                    uploading_server_file_size = bytes_to_uint32(read_buf, 2);
                                    uploading_player_file_size = bytes_to_uint32(read_buf, 6);
                                    string dir_name = "games/" + uploading_game_page.game_name + "/tmp/";
                                    filesystem::create_directories(dir_name);
                                    string server_filename = dir_name + "/" + uploading_game_page.game_name + ".cpp";
                                    upload_stream.open(server_filename, std::ios::binary);
                                    if (!upload_stream.is_open()) {
                                        send_reject("SERVER FILE ERROR, please try again later.");
                                        upload_state = 0;
                                    }
                                    else
                                        upload_state = 3;
                                }
                                else if (upload_state == 3) { // upload server file
                                    uint32_t chunk_size = bytes_to_uint32(read_buf, 2);
                                    upload_stream.write((char*)&read_buf[6], chunk_size);
                                    uploading_server_file_size_received += chunk_size;
                                    if (uploading_server_file_size_received == uploading_server_file_size) {
                                        upload_stream.close();
                                        string player_filename = "games/" + uploading_game_page.game_name + "/tmp/" + uploading_game_page.game_name + ".exe";
                                        upload_stream.open(player_filename, std::ios::binary);
                                        if (!upload_stream.is_open()) {
                                            send_reject("PLAYER FILE ERROR, please try again later.");
                                            upload_state = 0;
                                        }
                                        else
                                            upload_state = 4;
                                    }
                                }
                                else if (upload_state == 4) { // upload player file
                                    uint32_t chunk_size = bytes_to_uint32(read_buf, 2);
                                    upload_stream.write((char*)&read_buf[6], chunk_size);
                                    uploading_player_file_size_received += chunk_size;
                                    if (uploading_player_file_size_received == uploading_player_file_size) {
                                        upload_stream.close();
                                        // finish upload, compile server.cpp. command: g++ server.cpp -I asio -std=c++20 -o server
                                        uint32_t uid = ThisClient.user_id;
                                        GamePage gp = uploading_game_page;
                                        // compile in a new thread
                                        thread upload_thread([uid, gp]() {
                                            string folder_name = "games/" + gp.game_name + "/tmp/";
                                            int ret = system(("g++ \"" + folder_name + gp.game_name + ".cpp\" " + " \"games/EasonGS_API/EasonGS_Server.cpp\" " + "-I games/EasonGS_API/asio -I games/EasonGS_API -std=c++20 " +
                                                "-o \"" + folder_name + gp.game_name + "\"").c_str());
                                            if (ret == 0) {
                                                rename((folder_name + gp.game_name).c_str(), ("games/" + gp.game_name + "/" + gp.game_name).c_str());
                                                rename((folder_name + gp.game_name + ".exe").c_str(), ("games/" + gp.game_name + "/" + gp.game_name + ".exe").c_str());
                                                ofstream output_file("games/" + gp.game_name + "/" + gp.game_name + ".png", std::ios::out | std::ios::binary);
                                                output_file.write(reinterpret_cast<const char*>(gp.image_data.data()), gp.image_data.size());
                                                create_version_file(gp.game_name, make_tuple(gp.version[0], gp.version[1], gp.version[2]));
                                            }
                                            // post the result back to the asio context to avoid race condition
                                            asio::post(client_ioc_, [uid, gp, ret]() {
                                                if (ret == 0) {
                                                    gid_to_none_public_game_page[gp.game_id] = gid_to_none_public_game_page[gp.game_id - 1];
                                                    gid_to_none_public_game_page.erase(gp.game_id - 1);
                                                    GamePage& new_gp = gid_to_none_public_game_page[gp.game_id];
                                                    new_gp.game_id = gp.game_id;
                                                    new_gp.version[0] = gp.version[0];
                                                    new_gp.version[1] = gp.version[1];
                                                    new_gp.version[2] = gp.version[2];
                                                    new_gp.max_room_number = gp.max_room_number;
                                                    new_gp.max_players_per_room = gp.max_players_per_room;
                                                    if (user_id_to_NetServer.find(uid) != user_id_to_NetServer.end())
                                                        user_id_to_NetServer[uid]->send_packet(59, uint32_to_bytes(gp.game_id));
                                                    write_log("TheGameStore", user_id_to_User[uid].account + " updated the game: " + gp.game_name);
                                                }
                                                else if (user_id_to_NetServer.find(uid) != user_id_to_NetServer.end())
                                                    user_id_to_NetServer[uid]->send_reject("Compilation error, please check your code and re-upload the files.");
                                            });
                                        });
                                        upload_thread.detach();
                                        upload_state = 0;
                                    }
                                }
                            }
                        }
                    }
                    StartService();
                });
            }
            else {
                stop();
                if (ThisClient.client_stat != 0) {
                    write_log("TheGameStore", user_id_to_User[ThisClient.user_id].account + " logout.");
                    online_account.erase(user_id_to_User[ThisClient.user_id].account);
                    user_id_to_client_stat.erase(ThisClient.user_id);
                    user_id_to_NetServer.erase(ThisClient.user_id);
                    if (ThisClient.client_stat == 2) {
                        LeaveGame();
                    }
                }
                MyClients.erase(this);
                delete this;
            }
        });
    }

    void LeaveGame() {
        if (ThisClient.client_stat == 2) {
            room_id_to_Room[ThisClient.in_game_room_id].players.erase(ThisClient.user_id);
            if (room_id_to_Room[ThisClient.in_game_room_id].players.size() == 0) {
                if (gfid_to_GameServer.find(ThisClient.in_game_gfid) != gfid_to_GameServer.end())
                    gfid_to_room_IDs[ThisClient.in_game_gfid].erase(ThisClient.in_game_room_id);
                room_id_to_Room.erase(ThisClient.in_game_room_id);
            }
            if (gfid_to_GameServer.find(ThisClient.in_game_gfid) != gfid_to_GameServer.end()) {
                gfid_to_GameServer[ThisClient.in_game_gfid]->player_leave(ThisClient.user_id);
                gfid_to_GameServer[ThisClient.in_game_gfid]->decrease_player_in_game();
            }
            ThisClient.client_stat = 1;
            ThisClient.in_game_room_id = 0;
            ThisClient.in_game_gfid = 0;
        }
    }

    void send_bytes(vector<uint8_t>&& bytes) {
        unique_lock<mutex> lk(send_lock_);
        uint32_t len = htonl(bytes.size());
        vector<uint8_t> data_to_send;
        data_to_send.push_back((len >> 24) & 0xFF);
        data_to_send.push_back((len >> 16) & 0xFF);
        data_to_send.push_back((len >> 8) & 0xFF);
        data_to_send.push_back(len & 0xFF);
        data_to_send.insert(data_to_send.end(), bytes.begin(), bytes.end());
        if (write_queue.empty()) {
            write_queue.push(data_to_send);
            do_write();
        }
        else
            write_queue.push(data_to_send);
    }

    void send_packet(uint8_t data_type, vector<uint8_t> bytes = {}) {
        bytes.insert(bytes.begin(), data_type);
        send_bytes(move(bytes));
    }

    void stop() {
        timer_.cancel();
        socket_.close();
    }

    void send_reject(string&& reason) {
        vector<uint8_t> data_to_send;
        data_to_send.push_back(7);
        data_to_send.insert(data_to_send.end(), reason.begin(), reason.end());
        send_bytes(move(data_to_send));
    }

public:
    static asio::io_context client_ioc_;

    Client_Info ThisClient;

    NetServer() :
        socket_(client_ioc_),
        timer_(client_ioc_)
    {
        read_buf.resize(262144);
    }

    static void LaunchGameServer(uint32_t gfid, string& game_name) {
        if (gfid_to_GameServer.find(gfid) == gfid_to_GameServer.end()) {
            GameServer* gs = new GameServer(gfid, game_name);
            gfid_to_GameServer[gfid] = gs;
        }
    }

    static void AcceptConnection() {
        NetServer* NewClient;
        NewClient = new NetServer();
        MyClients.insert(NewClient);
        acp.async_accept(NewClient->socket_, [=](auto ec) {
            if (!ec) {
                NewClient->StartService();
                AcceptConnection();
            }
            else
                cerr << "[Important] Server stop accept connection" << '\n';
        });
    }

    static void StopService() {
        acp.close();
        // call stop_server will erase the GameServer from gfid_to_GameServer and delete the object
        // Thus we use a while loop to keep calling stop_server until the map is empty
        while (!gfid_to_GameServer.empty()) {
            auto it = gfid_to_GameServer.begin();
            it->second->stop_server("Server is shutting down.");
        }
        for (auto& NS : MyClients)
            NS->stop();
        client_ioc_.stop();
        write_log("TheGameStore", "[Important] server stop.");
    }
};
asio::io_context NetServer::client_ioc_;
tcp::acceptor NetServer::acp(NetServer::client_ioc_, tcp::endpoint(tcp::v4(), SERVER_SERVICE_PORT));
set<NetServer*> NetServer::MyClients;
// vector<uint8_t> NetServer::read_buf(262144);
map<uint32_t, NetServer::GameServer*> NetServer::gfid_to_GameServer;
// vector<uint8_t> NetServer::GameServer::game_read_buf(262144);



signed main() {
    cout << "The Game Store server is booting..." << '\n';
    read_users("userlist");
    read_game_list("gamelist");
    thread LobbyThread;
    LobbyThread = thread([&] {
        NetServer::AcceptConnection();
        write_log("TheGameStore", "[Important] server start.");
        NetServer::client_ioc_.run();
    });

    cout << "The Game Store is running." << '\n';
    cout << "CC BY-SA | The Game Store system | Eason023." << '\n';

    string cmd;
    cout << "=> ";
    while (cin >> cmd) {
        if (cmd == "stop") {
            cout << "Closing server..." << '\n';
            break;
        }
        else
            cout << "The only available command is \'stop\'" << '\n';
        cout << "=> ";
    }
    NetServer::StopService();
    LobbyThread.join();
    vector<User> save_userlist;
    for (auto& tmp : user_id_to_User)
        save_userlist.push_back(tmp.second);
    save_users("userlist", save_userlist);
    save_game_list("gamelist");
    cout << "Server closed." << '\n';
    return 0;
}

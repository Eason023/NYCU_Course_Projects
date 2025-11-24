#include <functional>
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <chrono>
#include "asio/asio.hpp"

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
// Windows: g++ Game-server.cpp -I asio -std=c++20 -lws2_32 -lmswsock -o Game-server.exe
// Linux: g++ Game-server.cpp -I asio -std=c++20 -o Game-server

class Room
{
public:
    uint8_t PrivatePublic, GameMode, WatchingNumber = 255;
    uint16_t room_code, HostPlayerID = 65535, Player2ID = 65535, HostPlayerELO, Player2ELO, GamePort = 0;
    string HostPlayerUsername, Player2Username;
};

uint16_t current_room_num = 0;

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
    uint16_t UserID = 65535, PlayerELO, GamePlayed;
    string PlayerUsername;
};

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
        if (io_thread_.joinable() && this_thread::get_id() != io_thread_.get_id())
            io_thread_.join();
    }

    // Length-Prefixed Framing Protocol
    void send_bytes(const vector<uint8_t> &bytes)
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

NetClient MyDBserver;

class NetServer;
map<uint16_t, NetServer *> user_id_to_NetServer;
map<uint16_t, int> user_id_to_client_stat;
map<uint16_t, Room> room_code_to_Room;
map<uint16_t, uint16_t> user_id_to_room_code;
set<NetServer *> MyClients;

mutex map_data_lock;

/*
client_stat definition:
    0: Unqualified
    1: Lobby
    2: In room
    3: In game
*/

uint16_t UpdateELO(uint16_t, uint16_t, uint16_t, uint16_t, uint8_t); // return a room code
void NotifyAllUser_PublicRoomList(void);
void NotifyAllUser_OnlinePlayersList(void);

void SendPublicRoomList(uint16_t);    // to a user
void SendOnlinePlayersList(uint16_t); // to a user
void SendUpdatedRoomInfo(uint16_t);   // to the room member(s)

void return_to_room(uint16_t);

class GameTable
{
    /*
    Table Definition:
        0 = empty;
        1 = sky blue; I
        2 = blue;     J
        3 = orange;   L
        4 = yellow;   O
        5 = green;    S
        6 = purple;   T
        7 = red;      Z
        8 = Obstacle block
        9 ~ 15 = frame block
    */

    /*
    SCORE:
        1 line: 100
        2 line: 300
        3 line: 500
        4 line: 800

        Combo: additional 50 (Accumulation)

        soft drop: 1 per block
        hard drop: 2 per block
    */
private:
    mt19937 rng;
    uniform_int_distribution<int> distribution{0, 100}; // distribution(rng)

public:
    uint8_t GameMode;
    uint16_t Player1ELO, Player2ELO;

    uint8_t GameStat; // 0~3: Ready?  4: In Game  5: P1 Win  6: P2 Win  7: Tie
    uint32_t P1_score, P2_score;
    uint8_t P1_Table[20][10], P2_Table[20][10];
    uint16_t GameTime; // (Second)

    string Player1Username, Player2Username;

    vector<pair<int, int>> IJLOSTZ[8][4]; // block [rotation][pos_i] delta(y, x)

    int p1_combo, p2_combo;

    pair<int, int> p1_block_pos, p2_block_pos;
    int p1_block_rotation, p2_block_rotation;
    queue<int> p1_bag, p2_bag;

    GameTable() : rng(chrono::high_resolution_clock::now().time_since_epoch().count())
    {
        IJLOSTZ[1][0] = {make_pair(0, -1), make_pair(0, 0), make_pair(0, 1), make_pair(0, 2)};
        IJLOSTZ[1][1] = {make_pair(-1, 1), make_pair(0, 1), make_pair(1, 1), make_pair(2, 1)};
        IJLOSTZ[1][2] = {make_pair(1, -1), make_pair(1, 0), make_pair(1, 1), make_pair(1, 2)};
        IJLOSTZ[1][3] = {make_pair(-1, 0), make_pair(0, 0), make_pair(1, 0), make_pair(2, 0)};

        IJLOSTZ[2][0] = {make_pair(-1, -1), make_pair(0, -1), make_pair(0, 0), make_pair(0, 1)};
        IJLOSTZ[2][1] = {make_pair(-1, 1), make_pair(-1, 0), make_pair(0, 0), make_pair(1, 0)};
        IJLOSTZ[2][2] = {make_pair(0, -1), make_pair(0, 0), make_pair(0, 1), make_pair(1, 1)};
        IJLOSTZ[2][3] = {make_pair(-1, 0), make_pair(0, 0), make_pair(1, 0), make_pair(1, -1)};

        IJLOSTZ[3][0] = {make_pair(-1, 1), make_pair(0, -1), make_pair(0, 0), make_pair(0, 1)};
        IJLOSTZ[3][1] = {make_pair(1, 1), make_pair(-1, 0), make_pair(0, 0), make_pair(1, 0)};
        IJLOSTZ[3][2] = {make_pair(0, -1), make_pair(0, 0), make_pair(0, 1), make_pair(1, -1)};
        IJLOSTZ[3][3] = {make_pair(-1, 0), make_pair(0, 0), make_pair(1, 0), make_pair(-1, -1)};

        IJLOSTZ[4][0] = {make_pair(0, 0), make_pair(0, 1), make_pair(1, 0), make_pair(1, 1)};
        IJLOSTZ[4][1] = {make_pair(0, 0), make_pair(0, 1), make_pair(1, 0), make_pair(1, 1)};
        IJLOSTZ[4][2] = {make_pair(0, 0), make_pair(0, 1), make_pair(1, 0), make_pair(1, 1)};
        IJLOSTZ[4][3] = {make_pair(0, 0), make_pair(0, 1), make_pair(1, 0), make_pair(1, 1)};

        IJLOSTZ[5][0] = {make_pair(0, -1), make_pair(0, 0), make_pair(-1, 0), make_pair(-1, 1)};
        IJLOSTZ[5][1] = {make_pair(-1, 0), make_pair(0, 0), make_pair(0, 1), make_pair(1, 1)};
        IJLOSTZ[5][2] = {make_pair(1, -1), make_pair(1, 0), make_pair(0, 0), make_pair(0, 1)};
        IJLOSTZ[5][3] = {make_pair(-1, -1), make_pair(0, -1), make_pair(0, 0), make_pair(1, 0)};

        IJLOSTZ[6][0] = {make_pair(0, -1), make_pair(0, 0), make_pair(-1, 0), make_pair(0, 1)};
        IJLOSTZ[6][1] = {make_pair(1, 0), make_pair(0, 0), make_pair(-1, 0), make_pair(0, 1)};
        IJLOSTZ[6][2] = {make_pair(0, -1), make_pair(0, 0), make_pair(1, 0), make_pair(0, 1)};
        IJLOSTZ[6][3] = {make_pair(1, 0), make_pair(0, 0), make_pair(-1, 0), make_pair(0, -1)};

        IJLOSTZ[7][0] = {make_pair(-1, -1), make_pair(-1, 0), make_pair(0, 0), make_pair(0, 1)};
        IJLOSTZ[7][1] = {make_pair(-1, 1), make_pair(0, 1), make_pair(0, 0), make_pair(1, 0)};
        IJLOSTZ[7][2] = {make_pair(0, -1), make_pair(0, 0), make_pair(1, 0), make_pair(1, 1)};
        IJLOSTZ[7][3] = {make_pair(-1, 0), make_pair(0, 0), make_pair(0, -1), make_pair(1, -1)};

        p1_block_pos = p2_block_pos = make_pair(0, 4); // (y, x)
        p1_block_rotation = p2_block_rotation = 0;
        GameMode = 0;
        Player1ELO = Player2ELO = 0;
        GameStat = 0;
        P1_score = P2_score = 0;
        memset(P1_Table, 0, sizeof(P1_Table));
        memset(P2_Table, 0, sizeof(P2_Table));
        GameTime = 0;

        fill_bag();
    }

    void fill_bag()
    {
        int arr[] = {1, 2, 3, 4, 5, 6, 7};
        shuffle(arr, arr + 7, rng);
        for (int i = 6; i >= 0; i--)
        {
            p1_bag.push(arr[i]);
            p2_bag.push(arr[i]);
        }
    }

    bool can_block_fall(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        bool success = 1;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].second + the_block_pos.second;
            if (y_pos + 1 >= 0 && (!(y_pos + 1 < 20 && the_table[y_pos + 1][x_pos] == 0)))
                success = 0;
        }
        return success;
    }

    bool can_block_move_left(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        bool success = 1;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].second + the_block_pos.second;
            if ((!(x_pos - 1 >= 0)) || y_pos >= 0 && (!(x_pos - 1 >= 0 && the_table[y_pos][x_pos - 1] == 0)))
                success = 0;
        }
        return success;
    }

    bool can_block_move_right(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        bool success = 1;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].second + the_block_pos.second;
            if ((!(x_pos + 1 < 10)) || y_pos >= 0 && (!(the_table[y_pos][x_pos + 1] == 0)))
                success = 0;
        }
        return success;
    }

    bool can_block_rotate_left(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        bool success = 1;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][(the_block_rotation + 4 - 1) % 4][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][(the_block_rotation + 4 - 1) % 4][i].second + the_block_pos.second;
            if ((!(x_pos < 10 && x_pos >= 0)) || y_pos >= 0 && (!(the_table[y_pos][x_pos] == 0)))
                success = 0;
        }
        return success;
    }

    bool can_block_rotate_right(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        bool success = 1;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][(the_block_rotation + 1) % 4][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][(the_block_rotation + 1) % 4][i].second + the_block_pos.second;
            if ((!(x_pos < 10 && x_pos >= 0)) || y_pos >= 0 && (!(the_table[y_pos][x_pos] == 0)))
                success = 0;
        }
        return success;
    }

    int place_down_and_update_score_and_next_block(int p1_or_p2)
    {
        queue<int> &the_bag = (p1_or_p2 == 0 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 0 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 0 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 0 ? p1_block_rotation : p2_block_rotation);
        int &the_combo = (p1_or_p2 == 0 ? p1_combo : p2_combo);
        uint32_t &the_score = (p1_or_p2 == 0 ? P1_score : P2_score);

        int cleared_line_num = 0;
        for (int i = 0; i < 4; i++)
        {
            int y_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].first + the_block_pos.first;
            int x_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].second + the_block_pos.second;
            if (y_pos < 0)
                return -1;
            the_table[y_pos][x_pos] = the_bag.front();
        }
        the_bag.pop();
        if (the_bag.empty())
            fill_bag();
        the_block_pos = make_pair(0, 4);
        the_block_rotation = 0;
        for (int i = 0; i < 20; i++)
        {
            bool can_clear = 1;
            for (int j = 0; j < 10; j++)
                if (the_table[i][j] == 0)
                    can_clear = 0;
            if (can_clear)
            {
                for (int j = 0; j < 10; j++)
                    the_table[i][j] = 0;
                cleared_line_num++;
                for (int j = i; j > 0; j--)
                    for (int k = 0; k < 10; k++)
                        the_table[j][k] = the_table[j - 1][k];
                for (int k = 0; k < 10; k++)
                    the_table[0][k] = 0;
            }
        }
        if (cleared_line_num > 0)
        {
            the_score += the_combo * 50 + (cleared_line_num == 1 ? 100 : (cleared_line_num == 2 ? 300 : (cleared_line_num == 3 ? 500 : 800)));
            the_combo++;
        }
        else
            the_combo = 0;
        return cleared_line_num;
    }
    void user_attack(uint8_t p1_or_p2, int attack_line_num)
    {
        queue<int> &the_bag = (p1_or_p2 == 1 ? p1_bag : p2_bag);
        uint8_t (&the_table)[20][10] = (p1_or_p2 == 1 ? P1_Table : P2_Table);
        pair<int, int> &the_block_pos = (p1_or_p2 == 1 ? p1_block_pos : p2_block_pos);
        int &the_block_rotation = (p1_or_p2 == 1 ? p1_block_rotation : p2_block_rotation);

        int found = 20;
        for (int i = 0; i < 20; i++)
        {
            for (int j = 0; j < 10; j++)
                if (the_table[i][j] != 0)
                {
                    found = i;
                    break;
                }
            if (found != 20)
                break;
        }
        if (found < attack_line_num)
            GameStat = (p1_or_p2 == 0 ? 5 : 6);
        the_block_pos.first--;
        if (!can_block_fall((p1_or_p2 == 0 ? 1 : 0)))
        {
            while (!can_block_fall((p1_or_p2 == 0 ? 1 : 0)))
                the_block_pos.first--;
            the_block_pos.first++;
            bool can_place = 1;
            for (int i = 0; i < 4; i++)
            {
                int y_pos = IJLOSTZ[the_bag.front()][the_block_rotation][i].first + the_block_pos.first;
                if (y_pos < 0)
                    can_place = 0;
            }
            if (can_place)
                place_down_and_update_score_and_next_block((p1_or_p2 == 0 ? 1 : 0));
            else
                GameStat = (p1_or_p2 == 0 ? 5 : 6);
        }
        for (int i = 0; i < 20 - attack_line_num; i++)
            for (int j = 0; j < 10; j++)
                the_table[i][j] = the_table[i + attack_line_num][j];
        int skipped_x = distribution(rng) % 10;
        for (int i = 20 - attack_line_num; i < 20; i++)
            for (int j = 0; j < 10; j++)
            {
                if (j != skipped_x)
                    the_table[i][j] = 8;
                else
                    the_table[i][j] = 0;
            }
    }
};

ConcurrentQueue<uint16_t> need_notify_P1uid, need_notify_P2uid, need_notify_P1ELO, need_notify_P2ELO;
ConcurrentQueue<uint8_t> need_notify_GameStat;

class NetGame
{
private:
    asio::io_context game_ioc_;
    tcp::acceptor game_acp_;
    asio::steady_timer timer_;

    thread game_thread_;

    GameTable TetrisGameTable;

    chrono::steady_clock::time_point game_start_time;
    int fall_block_cnt;

    class NetConnection
    {
    public:
        GameTable &Tetris;

        asio::steady_timer pong_timer_;
        asio::ip::tcp::socket socket_;
        vector<uint8_t> read_buf;
        uint32_t received_data_len;

        mutex safe_lock_;

        NetConnection(asio::io_context &ioc_, GameTable &GT) : Tetris(GT), pong_timer_(ioc_),
                                                               socket_(ioc_) { read_buf.resize(65536); }

        void send_bytes(const vector<uint8_t> &bytes)
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

        void start_game_service()
        {
            pong_timer_.expires_after(5s);
            pong_timer_.async_wait([this](asio::error_code ec)
                                   {
                                                    if (!ec){
                                                        socket_.close();
                                                    } });
            asio::async_read(socket_, asio::buffer(read_buf.data(), 4), [this](asio::error_code ec, size_t)
                             { if(!ec){
                                pong_timer_.cancel();
                                received_data_len = ntohl((read_buf[0]<<24)+(read_buf[1]<<16)+(read_buf[2]<<8)+read_buf[3]);
                                if(received_data_len>0 && received_data_len<65536){
                                    asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t) {
                                        if(!ec){
                                            uint8_t data_type = read_buf[0];
                                            vector<uint8_t> data;
                                            if (data_type == 1){
                                                data.push_back(2);
                                                send_bytes(data);
                                            }
                                            else if(data_type == 30 && Tetris.GameStat == 4){
                                                uint8_t p1_or_p2 = read_buf[1], op = read_buf[2];
                                                if(op == 0){
                                                    if(Tetris.can_block_move_left(p1_or_p2)){
                                                        if(p1_or_p2 == 0)
                                                            Tetris.p1_block_pos.second--;
                                                        else
                                                            Tetris.p2_block_pos.second--;
                                                    }
                                                }
                                                else if(op == 1){
                                                    if(Tetris.can_block_fall(p1_or_p2)){
                                                        if(p1_or_p2 == 0){
                                                            Tetris.p1_block_pos.first++;
                                                            Tetris.P1_score += 1;
                                                        }
                                                        else{
                                                            Tetris.p2_block_pos.first++;
                                                            Tetris.P2_score += 1;
                                                        }
                                                    }
                                                    else{
                                                        int cleared_line_num = Tetris.place_down_and_update_score_and_next_block(p1_or_p2);
                                                        if(cleared_line_num > 0 && Tetris.GameMode == 1)
                                                            Tetris.user_attack(p1_or_p2, cleared_line_num - 1);
                                                        else if(cleared_line_num == -1)
                                                            Tetris.GameStat = (p1_or_p2 == 0 ? 6 : 5);
                                                    }
                                                }
                                                else if(op == 2){
                                                    if(Tetris.can_block_move_right(p1_or_p2)){
                                                        if(p1_or_p2 == 0)
                                                            Tetris.p1_block_pos.second++;
                                                        else
                                                            Tetris.p2_block_pos.second++;
                                                    }
                                                }
                                                else if(op == 3){
                                                    while(Tetris.can_block_fall(p1_or_p2)){
                                                        if(p1_or_p2 == 0){
                                                            Tetris.p1_block_pos.first++;
                                                            Tetris.P1_score += 2;
                                                        }
                                                        else{
                                                            Tetris.p2_block_pos.first++;
                                                            Tetris.P2_score += 2;
                                                        }
                                                    }
                                                    int cleared_line_num = Tetris.place_down_and_update_score_and_next_block(p1_or_p2);
                                                    if(cleared_line_num > 0 && Tetris.GameMode == 1)
                                                        Tetris.user_attack(p1_or_p2, cleared_line_num - 1);
                                                    else if(cleared_line_num == -1)
                                                        Tetris.GameStat = (p1_or_p2 == 0 ? 6 : 5);
                                                }
                                                else if(op == 4){
                                                    if(Tetris.can_block_rotate_left(p1_or_p2)){
                                                        if(p1_or_p2 == 0)
                                                            Tetris.p1_block_rotation = (Tetris.p1_block_rotation + 4 - 1) % 4;
                                                        else
                                                            Tetris.p2_block_rotation = (Tetris.p2_block_rotation + 4 - 1) % 4;
                                                    }
                                                }
                                                else if(op == 5){
                                                    if(Tetris.can_block_rotate_right(p1_or_p2)){
                                                        if(p1_or_p2 == 0)
                                                            Tetris.p1_block_rotation = (Tetris.p1_block_rotation + 1) % 4;
                                                        else
                                                            Tetris.p2_block_rotation = (Tetris.p2_block_rotation + 1) % 4;
                                                    }
                                                }
                                            }
                                            start_game_service();
                                        }
                                    });
                                }
                                else{
                                    socket_.close();
                                }
                            } });
        }
    };

    vector<NetConnection *> NetConnection_list;

    void start()
    {
        NetConnection *Player_connection = new NetConnection(game_ioc_, TetrisGameTable);
        NetConnection_list.push_back(Player_connection);
        game_acp_.async_accept(Player_connection->socket_, [this, Player_connection = Player_connection](auto ec)
                               {
                                if(!ec){
                                    start();
                                    Player_connection->start_game_service();
                                } });
    }
    void start_broadcast_game_stat()
    {
        timer_.expires_after(25ms); // ~40 fps by default
        timer_.async_wait([this](asio::error_code ec)
                          {
                            if (!ec){
                                chrono::duration<double> elapsed_seconds = chrono::steady_clock::now() - game_start_time;
                                if(elapsed_seconds.count() <= 1)
                                    TetrisGameTable.GameStat = 0;
                                else if(elapsed_seconds.count() <= 2)
                                    TetrisGameTable.GameStat = 1;
                                else if(elapsed_seconds.count() <= 3)
                                    TetrisGameTable.GameStat = 2;
                                else if(elapsed_seconds.count() <= 4)
                                    TetrisGameTable.GameStat = 3;
                                else if(TetrisGameTable.GameStat < 4)
                                    TetrisGameTable.GameStat = 4;
                                TetrisGameTable.GameTime = max(0, 420 - ((int)(elapsed_seconds.count()) - 4));
                                if(TetrisGameTable.GameTime == 0){
                                    if(TetrisGameTable.GameMode == 0)
                                        TetrisGameTable.GameStat = (TetrisGameTable.P1_score == TetrisGameTable.P2_score ? 7 : (TetrisGameTable.P1_score > TetrisGameTable.P2_score ? 5 : 6));
                                    else
                                        TetrisGameTable.GameStat = 7;
                                }
                                if(fall_block_cnt != TetrisGameTable.GameTime && TetrisGameTable.GameStat == 4){
                                    if(TetrisGameTable.can_block_fall(0))
                                        TetrisGameTable.p1_block_pos.first++;
                                    else{
                                        int cleared_line_num = TetrisGameTable.place_down_and_update_score_and_next_block(0);
                                        if(cleared_line_num > 0 && TetrisGameTable.GameMode == 1)
                                            TetrisGameTable.user_attack(0, cleared_line_num - 1);
                                        else if(cleared_line_num == -1)
                                            TetrisGameTable.GameStat = 6;
                                    }
                                    if(TetrisGameTable.can_block_fall(1))
                                        TetrisGameTable.p2_block_pos.first++;
                                    else{
                                        int cleared_line_num = TetrisGameTable.place_down_and_update_score_and_next_block(1);
                                        if(cleared_line_num > 0 && TetrisGameTable.GameMode == 1)
                                            TetrisGameTable.user_attack(1, cleared_line_num - 1);
                                        else if(cleared_line_num == -1)
                                            TetrisGameTable.GameStat = 5;
                                    }
                                }
                                fall_block_cnt = TetrisGameTable.GameTime;
                                vector<uint8_t> game_data;
                                game_data.push_back(31);
                                game_data.push_back(TetrisGameTable.GameMode);
                                game_data.push_back(TetrisGameTable.Player1ELO / 256);
                                game_data.push_back(TetrisGameTable.Player1ELO % 256);
                                game_data.push_back(TetrisGameTable.Player2ELO / 256);
                                game_data.push_back(TetrisGameTable.Player2ELO % 256);
                                game_data.push_back(TetrisGameTable.GameStat);
                                game_data.push_back((TetrisGameTable.P1_score >> 24) & 0xFF);
                                game_data.push_back((TetrisGameTable.P1_score >> 16) & 0xFF);
                                game_data.push_back((TetrisGameTable.P1_score >> 8) & 0xFF);
                                game_data.push_back(TetrisGameTable.P1_score & 0xFF);
                                game_data.push_back((TetrisGameTable.P2_score >> 24) & 0xFF);
                                game_data.push_back((TetrisGameTable.P2_score >> 16) & 0xFF);
                                game_data.push_back((TetrisGameTable.P2_score >> 8) & 0xFF);
                                game_data.push_back(TetrisGameTable.P2_score & 0xFF);
                                int tmp_table[20][10];
                                // P1 Current Block UI
                                for (int i = 0; i < 20; i++)
                                    for (int j = 0; j < 10; j++)
                                        tmp_table[i][j] = TetrisGameTable.P1_Table[i][j];
                                int cnt = 0;
                                while(TetrisGameTable.can_block_fall(0)){
                                    TetrisGameTable.p1_block_pos.first++;
                                    cnt++;
                                }
                                for (int i = 0; i < 4; i++)
                                {
                                    int y_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p1_bag.front()][TetrisGameTable.p1_block_rotation][i].first + TetrisGameTable.p1_block_pos.first;
                                    int x_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p1_bag.front()][TetrisGameTable.p1_block_rotation][i].second + TetrisGameTable.p1_block_pos.second;
                                    if (y_pos >= 0)
                                        tmp_table[y_pos][x_pos] = TetrisGameTable.p1_bag.front() + 8;
                                }
                                TetrisGameTable.p1_block_pos.first -= cnt;
                                for (int i = 0; i < 4; i++)
                                {
                                    int y_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p1_bag.front()][TetrisGameTable.p1_block_rotation][i].first + TetrisGameTable.p1_block_pos.first;
                                    int x_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p1_bag.front()][TetrisGameTable.p1_block_rotation][i].second + TetrisGameTable.p1_block_pos.second;
                                    if (y_pos >= 0)
                                        tmp_table[y_pos][x_pos] = TetrisGameTable.p1_bag.front();
                                }
                                for (int i = 0; i < 20; i++)
                                    for (int j = 0; j < 10; j++)
                                        game_data.push_back(tmp_table[i][j]);
                                // P2 Current Block UI
                                for (int i = 0; i < 20; i++)
                                    for (int j = 0; j < 10; j++)
                                        tmp_table[i][j] = TetrisGameTable.P2_Table[i][j];
                                cnt = 0;
                                while(TetrisGameTable.can_block_fall(1)){
                                    TetrisGameTable.p2_block_pos.first++;
                                    cnt++;
                                }
                                for (int i = 0; i < 4; i++)
                                {
                                    int y_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p2_bag.front()][TetrisGameTable.p2_block_rotation][i].first + TetrisGameTable.p2_block_pos.first;
                                    int x_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p2_bag.front()][TetrisGameTable.p2_block_rotation][i].second + TetrisGameTable.p2_block_pos.second;
                                    if (y_pos >= 0)
                                        tmp_table[y_pos][x_pos] = TetrisGameTable.p2_bag.front() + 8;
                                }
                                TetrisGameTable.p2_block_pos.first -= cnt;
                                for (int i = 0; i < 4; i++)
                                {
                                    int y_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p2_bag.front()][TetrisGameTable.p2_block_rotation][i].first + TetrisGameTable.p2_block_pos.first;
                                    int x_pos = TetrisGameTable.IJLOSTZ[TetrisGameTable.p2_bag.front()][TetrisGameTable.p2_block_rotation][i].second + TetrisGameTable.p2_block_pos.second;
                                    if (y_pos >= 0)
                                        tmp_table[y_pos][x_pos] = TetrisGameTable.p2_bag.front();
                                }
                                for (int i = 0; i < 20; i++)
                                    for (int j = 0; j < 10; j++)
                                        game_data.push_back(tmp_table[i][j]);
                                game_data.push_back(TetrisGameTable.GameTime / 256);
                                game_data.push_back(TetrisGameTable.GameTime % 256);
                                game_data.push_back(TetrisGameTable.Player1Username.length());
                                game_data.push_back(TetrisGameTable.Player2Username.length());
                                game_data.insert(game_data.end(), TetrisGameTable.Player1Username.begin(), TetrisGameTable.Player1Username.end());
                                game_data.insert(game_data.end(), TetrisGameTable.Player2Username.begin(), TetrisGameTable.Player2Username.end());
                                for (int i = 0; i < ((int)NetConnection_list.size()) - 1; i++)
                                    NetConnection_list[i]->send_bytes(game_data);
                                if (TetrisGameTable.GameStat > 4)
                                {
                                    game_acp_.close();
                                    return_to_room(P1_user_id);
                                    return_to_room(P2_user_id);
                                    need_notify_P1uid.push(P1_user_id);
                                    need_notify_P2uid.push(P2_user_id);
                                    need_notify_P1ELO.push(TetrisGameTable.Player1ELO);
                                    need_notify_P2ELO.push(TetrisGameTable.Player2ELO);
                                    need_notify_GameStat.push(TetrisGameTable.GameStat);
                                    for (int i = 0; i < NetConnection_list.size(); i++){
                                        NetConnection_list[i]->pong_timer_.cancel();
                                        NetConnection_list[i]->socket_.close();
                                    }
                                    timer_.cancel();
                                    game_ioc_.stop();
                                }
                                else
                                    start_broadcast_game_stat();
                            } });
    }

public:
    uint16_t GamePort, P1_user_id, P2_user_id;
    NetGame(GameTable &GameInfo, uint16_t P1_uid, uint16_t P2_uid) : game_ioc_(),
                                                                     game_acp_(game_ioc_, tcp::endpoint(tcp::v4(), 0)),
                                                                     timer_(game_ioc_),
                                                                     TetrisGameTable(GameInfo),
                                                                     game_start_time(chrono::steady_clock::now()),
                                                                     P1_user_id(P1_uid),
                                                                     P2_user_id(P2_uid)
    {
        fall_block_cnt = 0;
        GamePort = game_acp_.local_endpoint().port();
        game_thread_ = thread([this]
                              { start();
                                start_broadcast_game_stat();
                                game_ioc_.run(); delete this; });
        game_thread_.detach(); // thread bye bye
    }

    ~NetGame()
    {
        for (int i = 0; i < NetConnection_list.size(); i++)
            delete NetConnection_list[i];
    }
};

asio::io_context client_ioc_;
tcp::acceptor acp(client_ioc_, tcp::endpoint(tcp::v4(), 52023));
class NetServer
{
private:
    asio::steady_timer timer_;
    asio::ip::tcp::socket socket_;

    vector<uint8_t> read_buf;
    uint32_t received_data_len;
    mutex safe_lock_;

    void StartService()
    {
        timer_.expires_after(5s);
        timer_.async_wait([this](asio::error_code ec)
                          {
                        if (!ec){
                            socket_.close();
                        } });
        asio::async_read(socket_, asio::buffer(read_buf.data(), 4), [this](asio::error_code ec, size_t)
                         {
                            bool ec2 = 0;
                            timer_.cancel();
                            if(!ec){
                                received_data_len = ntohl((read_buf[0]<<24)+(read_buf[1]<<16)+(read_buf[2]<<8)+read_buf[3]);
                                if(received_data_len>0 && received_data_len<65536)
                                    asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t)
                                                    {
                                                        unique_lock<mutex> lk(map_data_lock);
                                                        if((!need_notify_P1uid.empty()) && (!need_notify_P2uid.empty()) && (!need_notify_P1ELO.empty()) && (!need_notify_P2ELO.empty()) && (!need_notify_GameStat.empty())){
                                                            uint16_t tmp_room_code = UpdateELO(need_notify_P1uid.front(), need_notify_P2uid.front(), need_notify_P1ELO.front(), need_notify_P2ELO.front(), need_notify_GameStat.front());
                                                            need_notify_P1uid.pop();
                                                            need_notify_P2uid.pop();
                                                            need_notify_P1ELO.pop();
                                                            need_notify_P2ELO.pop();
                                                            need_notify_GameStat.pop();
                                                            if(tmp_room_code != 65535)
                                                                SendUpdatedRoomInfo(tmp_room_code);
                                                            NotifyAllUser_PublicRoomList();
                                                        }
                                                        uint8_t data_type = read_buf[0];
                                                        vector<uint8_t> data;
                                                        if (data_type == 1){
                                                            data.push_back(2);
                                                            send_bytes(data);
                                                        }
                                                        else if (data_type == 3){
                                                            vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                            MyDBserver.send_bytes(data_to_db);

                                                            int cnt = 0;
                                                            while(MyDBserver.received_data.empty() && cnt<1000){
                                                                this_thread::sleep_for(1ms);
                                                                cnt++;
                                                            }
                                                            if(!MyDBserver.received_data.empty()){
                                                                send_bytes(MyDBserver.received_data.front().bytes);
                                                                MyDBserver.received_data.pop();
                                                            }
                                                        }
                                                        else if (data_type == 8){
                                                            vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                            MyDBserver.send_bytes(data_to_db);

                                                            int cnt = 0;
                                                            while(MyDBserver.received_data.empty() && cnt<1000){
                                                                this_thread::sleep_for(1ms);
                                                                cnt++;
                                                            }
                                                            if(!MyDBserver.received_data.empty()){
                                                                if(MyDBserver.received_data.front().type == 6){
                                                                    vector<uint8_t> id_tmp(MyDBserver.received_data.front().bytes);
                                                                    MyDBserver.received_data.pop();
                                                                    ThisPlayer.UserID = (uint16_t)(id_tmp[0]<<8) + id_tmp[1];
                                                                    ThisPlayer.PlayerELO = (uint16_t)(id_tmp[2]<<8) + id_tmp[3];
                                                                    ThisPlayer.GamePlayed = (uint16_t)(id_tmp[4]<<8) + id_tmp[5];
                                                                    uint8_t name_len = id_tmp[6];
                                                                    string player_name(id_tmp.begin()+7,id_tmp.begin()+7+name_len);
                                                                    ThisPlayer.PlayerUsername = player_name;
                                                                    user_id_to_NetServer[ThisPlayer.UserID] = this;
                                                                    user_id_to_client_stat[ThisPlayer.UserID] = 1;
                                                                    data.push_back(6);
                                                                    send_bytes(data);
                                                                    SendPublicRoomList(ThisPlayer.UserID);
                                                                    NotifyAllUser_OnlinePlayersList();
                                                                }
                                                                else{
                                                                    send_bytes(MyDBserver.received_data.front().bytes);
                                                                    MyDBserver.received_data.pop();
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 9){
                                                            if(ThisPlayer.UserID != 65535){
                                                                if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                    vector<uint8_t> data_to_db;
                                                                    data_to_db.push_back(9);
                                                                    data_to_db.push_back(ThisPlayer.UserID / 256);
                                                                    data_to_db.push_back(ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);
                                                                    NotifyAllUser_OnlinePlayersList();
                                                                }
                                                                else{
                                                                    vector<uint8_t> data_to_db;
                                                                    data_to_db.push_back(11);
                                                                    data_to_db.push_back(ThisPlayer.UserID / 256);
                                                                    data_to_db.push_back(ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    uint16_t player_room_code = user_id_to_room_code[ThisPlayer.UserID];
                                                                    if(room_code_to_Room[player_room_code].HostPlayerUsername==ThisPlayer.PlayerUsername){ // host left
                                                                        if(room_code_to_Room[player_room_code].Player2ID != 65535){
                                                                            vector<uint8_t> data;
                                                                            user_id_to_client_stat[room_code_to_Room[player_room_code].Player2ID] = 1;
                                                                            data.push_back(22);
                                                                            if (user_id_to_NetServer.find(room_code_to_Room[player_room_code].Player2ID) != user_id_to_NetServer.end())
                                                                                user_id_to_NetServer[room_code_to_Room[player_room_code].Player2ID]->send_bytes(data);
                                                                            user_id_to_room_code.erase(room_code_to_Room[player_room_code].Player2ID);
                                                                        }
                                                                        room_code_to_Room.erase(player_room_code);
                                                                    }
                                                                    else{
                                                                        room_code_to_Room[player_room_code].Player2ID = 65535;
                                                                        room_code_to_Room[player_room_code].Player2ELO = 0;
                                                                        room_code_to_Room[player_room_code].Player2Username = "";
                                                                        SendUpdatedRoomInfo(player_room_code);
                                                                    }
                                                                    user_id_to_room_code.erase(ThisPlayer.UserID);
                                                                    user_id_to_client_stat[ThisPlayer.UserID] = 1;

                                                                    data_to_db[0] = 9;
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    NotifyAllUser_PublicRoomList();
                                                                    NotifyAllUser_OnlinePlayersList();
                                                                }
                                                                user_id_to_client_stat.erase(ThisPlayer.UserID);
                                                                user_id_to_NetServer.erase(ThisPlayer.UserID);
                                                                user_id_to_room_code.erase(ThisPlayer.UserID);
                                                            }
                                                            asio::error_code ig;
                                                            socket_.close(ig);
                                                            return;
                                                        }
                                                        else if (data_type == 10){ // create room
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                                data_to_db.insert(data_to_db.end(), ThisPlayer.UserID / 256);
                                                                data_to_db.insert(data_to_db.end(), ThisPlayer.UserID % 256);
                                                                MyDBserver.send_bytes(data_to_db);

                                                                user_id_to_client_stat[ThisPlayer.UserID] = 2;
                                                                Room tmp_room{};
                                                                tmp_room.PrivatePublic = read_buf[1];
                                                                tmp_room.GameMode = read_buf[2];
                                                                tmp_room.room_code = current_room_num++;
                                                                tmp_room.HostPlayerID = ThisPlayer.UserID;
                                                                tmp_room.HostPlayerELO = ThisPlayer.PlayerELO;
                                                                tmp_room.HostPlayerUsername = ThisPlayer.PlayerUsername;
                                                                room_code_to_Room[tmp_room.room_code] = tmp_room;
                                                                user_id_to_room_code[ThisPlayer.UserID] = tmp_room.room_code;
                                                                SendUpdatedRoomInfo(tmp_room.room_code);
                                                                NotifyAllUser_PublicRoomList();
                                                                NotifyAllUser_OnlinePlayersList();
                                                            }
                                                        }
                                                        else if (data_type == 11){ // leave room
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 2){
                                                                vector<uint8_t> data_to_db;
                                                                data_to_db.push_back(11);
                                                                data_to_db.push_back(ThisPlayer.UserID / 256);
                                                                data_to_db.push_back(ThisPlayer.UserID % 256);
                                                                MyDBserver.send_bytes(data_to_db);

                                                                uint16_t player_room_code = user_id_to_room_code[ThisPlayer.UserID];
                                                                if(room_code_to_Room[player_room_code].HostPlayerUsername==ThisPlayer.PlayerUsername){ // host left
                                                                    if(room_code_to_Room[player_room_code].Player2ID != 65535){
                                                                        user_id_to_client_stat[room_code_to_Room[player_room_code].Player2ID] = 1;
                                                                        data.push_back(22);
                                                                        if (user_id_to_NetServer.find(room_code_to_Room[player_room_code].Player2ID) != user_id_to_NetServer.end())
                                                                            user_id_to_NetServer[room_code_to_Room[player_room_code].Player2ID]->send_bytes(data);
                                                                        user_id_to_room_code.erase(room_code_to_Room[player_room_code].Player2ID);
                                                                    }
                                                                    room_code_to_Room.erase(player_room_code);
                                                                }
                                                                else{
                                                                    room_code_to_Room[player_room_code].Player2ID = 65535;
                                                                    room_code_to_Room[player_room_code].Player2ELO = 0;
                                                                    room_code_to_Room[player_room_code].Player2Username = "";
                                                                    SendUpdatedRoomInfo(player_room_code);
                                                                }
                                                                user_id_to_room_code.erase(ThisPlayer.UserID);
                                                                user_id_to_client_stat[ThisPlayer.UserID] = 1;
                                                                NotifyAllUser_PublicRoomList();
                                                                NotifyAllUser_OnlinePlayersList();
                                                            }
                                                        }
                                                        else if (data_type == 12){ // join room
                                                            uint16_t join_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                if(room_code_to_Room.find(join_room_code) != room_code_to_Room.end() && room_code_to_Room[join_room_code].Player2ID == 65535){
                                                                    vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID / 256);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    room_code_to_Room[join_room_code].Player2ID = ThisPlayer.UserID;
                                                                    room_code_to_Room[join_room_code].Player2ELO = ThisPlayer.PlayerELO;
                                                                    room_code_to_Room[join_room_code].Player2Username = ThisPlayer.PlayerUsername;
                                                                    user_id_to_room_code[ThisPlayer.UserID] = join_room_code;
                                                                    user_id_to_client_stat[ThisPlayer.UserID] = 2;

                                                                    data.push_back(12);
                                                                    send_bytes(data);
                                                                    SendUpdatedRoomInfo(join_room_code);
                                                                    NotifyAllUser_PublicRoomList();
                                                                    NotifyAllUser_OnlinePlayersList();
                                                                }
                                                                else send_reject("The room is unavailable or full!");
                                                            }
                                                        }
                                                        else if (data_type == 13){ // watch room game
                                                            uint16_t watch_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                if(room_code_to_Room.find(watch_room_code) != room_code_to_Room.end() && room_code_to_Room[watch_room_code].WatchingNumber != 255){
                                                                    vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID / 256);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    room_code_to_Room[watch_room_code].WatchingNumber++;
                                                                    data.push_back(13);
                                                                    data.push_back(room_code_to_Room[watch_room_code].GamePort / 256);
                                                                    data.push_back(room_code_to_Room[watch_room_code].GamePort % 256);
                                                                    send_bytes(data);
                                                                    NotifyAllUser_PublicRoomList();
                                                                }
                                                                else send_reject("The game is unavailable or ended!");
                                                            }
                                                        }
                                                        else if (data_type == 14){ // invitation
                                                            uint16_t invited_user_id = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 2){
                                                                if(user_id_to_client_stat.find(invited_user_id) != user_id_to_client_stat.end()){
                                                                    if(user_id_to_client_stat[invited_user_id] == 1){
                                                                        Room tmp_room = room_code_to_Room[user_id_to_room_code[ThisPlayer.UserID]];
                                                                        data.push_back(14);
                                                                        data.push_back(tmp_room.GameMode);
                                                                        data.push_back(tmp_room.room_code/256);
                                                                        data.push_back(tmp_room.room_code%256);
                                                                        data.push_back(ThisPlayer.PlayerELO/256);
                                                                        data.push_back(ThisPlayer.PlayerELO%256);
                                                                        data.push_back(ThisPlayer.PlayerUsername.length());
                                                                        data.insert(data.end(), ThisPlayer.PlayerUsername.begin(), ThisPlayer.PlayerUsername.end());
                                                                        if (user_id_to_NetServer.find(invited_user_id) != user_id_to_NetServer.end())
                                                                            user_id_to_NetServer[invited_user_id]->send_bytes(data);
                                                                    }
                                                                    else send_reject("The user can't be invited now!");
                                                                }
                                                                else send_reject("The invited user is offline now!");
                                                            }
                                                        }
                                                        else if (data_type == 15){ // accept invitation
                                                            uint16_t accepted_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                if(room_code_to_Room.find(accepted_room_code) != room_code_to_Room.end() && room_code_to_Room[accepted_room_code].Player2ID == 65535){
                                                                    vector<uint8_t> data_to_db(read_buf.begin(), read_buf.begin() + received_data_len);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID / 256);
                                                                    data_to_db.insert(data_to_db.end(), ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    room_code_to_Room[accepted_room_code].Player2ID = ThisPlayer.UserID;
                                                                    room_code_to_Room[accepted_room_code].Player2ELO = ThisPlayer.PlayerELO;
                                                                    room_code_to_Room[accepted_room_code].Player2Username = ThisPlayer.PlayerUsername;
                                                                    data.push_back(15);
                                                                    data.insert(data.end(), ThisPlayer.PlayerUsername.begin(), ThisPlayer.PlayerUsername.end());
                                                                    if (user_id_to_NetServer.find(room_code_to_Room[accepted_room_code].HostPlayerID) != user_id_to_NetServer.end())
                                                                        user_id_to_NetServer[room_code_to_Room[accepted_room_code].HostPlayerID]->send_bytes(data);
                                                                    user_id_to_client_stat[ThisPlayer.UserID] = 2;
                                                                    user_id_to_room_code[ThisPlayer.UserID] = accepted_room_code;
                                                                    data.clear();
                                                                    data.push_back(15);
                                                                    send_bytes(data);
                                                                    SendUpdatedRoomInfo(accepted_room_code);
                                                                    NotifyAllUser_PublicRoomList();
                                                                    NotifyAllUser_OnlinePlayersList();
                                                                }
                                                                else send_reject("The room is unavailable or full!");
                                                            }
                                                        }
                                                        else if (data_type == 16){ // decline invitation
                                                            uint16_t declined_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                                                if(room_code_to_Room.find(declined_room_code) != room_code_to_Room.end() && room_code_to_Room[declined_room_code].Player2ID == 65535){
                                                                    data.push_back(16);
                                                                    data.insert(data.end(), ThisPlayer.PlayerUsername.begin(), ThisPlayer.PlayerUsername.end());
                                                                    if (user_id_to_NetServer.find(room_code_to_Room[declined_room_code].HostPlayerID) != user_id_to_NetServer.end())
                                                                        user_id_to_NetServer[room_code_to_Room[declined_room_code].HostPlayerID]->send_bytes(data);
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 25){ // start the game
                                                            if(user_id_to_client_stat[ThisPlayer.UserID] == 2){
                                                                uint16_t player_room_code = user_id_to_room_code[ThisPlayer.UserID];
                                                                Room GameRoom = room_code_to_Room[player_room_code];
                                                                if(GameRoom.Player2ID != 65535){
                                                                    vector<uint8_t> data_to_db;
                                                                    data_to_db.push_back(25);
                                                                    data_to_db.push_back(ThisPlayer.UserID / 256);
                                                                    data_to_db.push_back(ThisPlayer.UserID % 256);
                                                                    MyDBserver.send_bytes(data_to_db);

                                                                    GameTable GameInfo{};
                                                                    GameInfo.GameMode = GameRoom.GameMode;
                                                                    GameInfo.Player1Username = GameRoom.HostPlayerUsername;
                                                                    GameInfo.Player1ELO = GameRoom.HostPlayerELO;
                                                                    GameInfo.Player2Username = GameRoom.Player2Username;
                                                                    GameInfo.Player2ELO = GameRoom.Player2ELO;
                                                                    NetGame *TheGame = new NetGame(GameInfo, GameRoom.HostPlayerID, GameRoom.Player2ID);
                                                                    uint16_t game_port = TheGame->GamePort;
                                                                    data.push_back(25);
                                                                    data.push_back(game_port / 256);
                                                                    data.push_back(game_port % 256);
                                                                    send_bytes(data);
                                                                    if (user_id_to_NetServer.find(GameRoom.Player2ID) != user_id_to_NetServer.end())
                                                                        user_id_to_NetServer[GameRoom.Player2ID]->send_bytes(data);
                                                                    user_id_to_client_stat[ThisPlayer.UserID] = 3;
                                                                    user_id_to_client_stat[GameRoom.Player2ID] = 3;
                                                                    room_code_to_Room[player_room_code].WatchingNumber = 0;
                                                                    room_code_to_Room[player_room_code].GamePort = game_port;
                                                                    NotifyAllUser_PublicRoomList();
                                                                }
                                                                else send_reject("Can't start the game. You can't play solo!");
                                                            }
                                                        }
                                                        StartService();
                                                    });
                                else ec2 = 1;
                            }
                            if(ec||ec2)
                            {
                                unique_lock<mutex> lk(map_data_lock);
                                if(ThisPlayer.UserID != 65535){
                                    if(user_id_to_client_stat[ThisPlayer.UserID] == 1){
                                        vector<uint8_t> data_to_db;
                                        data_to_db.push_back(9);
                                        data_to_db.push_back(ThisPlayer.UserID / 256);
                                        data_to_db.push_back(ThisPlayer.UserID % 256);
                                        MyDBserver.send_bytes(data_to_db);
                                        NotifyAllUser_OnlinePlayersList();
                                    }
                                    else{
                                        vector<uint8_t> data_to_db;
                                        data_to_db.push_back(11);
                                        data_to_db.push_back(ThisPlayer.UserID / 256);
                                        data_to_db.push_back(ThisPlayer.UserID % 256);
                                        MyDBserver.send_bytes(data_to_db);

                                        uint16_t player_room_code = user_id_to_room_code[ThisPlayer.UserID];
                                        if(room_code_to_Room[player_room_code].HostPlayerUsername==ThisPlayer.PlayerUsername){ // host left
                                            if(room_code_to_Room[player_room_code].Player2ID != 65535){
                                                vector<uint8_t> data;
                                                user_id_to_client_stat[room_code_to_Room[player_room_code].Player2ID] = 1;
                                                data.push_back(22);
                                                if (user_id_to_NetServer.find(room_code_to_Room[player_room_code].Player2ID) != user_id_to_NetServer.end())
                                                    user_id_to_NetServer[room_code_to_Room[player_room_code].Player2ID]->send_bytes(data);
                                                user_id_to_room_code.erase(room_code_to_Room[player_room_code].Player2ID);
                                            }
                                            room_code_to_Room.erase(player_room_code);
                                        }
                                        else{
                                            room_code_to_Room[player_room_code].Player2ID = 65535;
                                            room_code_to_Room[player_room_code].Player2ELO = 0;
                                            room_code_to_Room[player_room_code].Player2Username = "";
                                            SendUpdatedRoomInfo(player_room_code);
                                        }
                                        user_id_to_room_code.erase(ThisPlayer.UserID);
                                        user_id_to_client_stat[ThisPlayer.UserID] = 0;

                                        data_to_db[0] = 9;
                                        MyDBserver.send_bytes(data_to_db);

                                        NotifyAllUser_PublicRoomList();
                                        NotifyAllUser_OnlinePlayersList();
                                    }
                                    user_id_to_client_stat.erase(ThisPlayer.UserID);
                                    user_id_to_NetServer.erase(ThisPlayer.UserID);
                                    user_id_to_room_code.erase(ThisPlayer.UserID);
                                }
                                MyClients.erase(this);
                                delete this;
                            } });
    }

public:
    Player_info ThisPlayer;

    NetServer() : socket_(client_ioc_),
                  timer_(client_ioc_)
    {
        read_buf.resize(65536);
    }

    /*~NetServer()
    {
        socket_.close();
        timer_.cancel();
    }*/

    void stop()
    {
        timer_.cancel();
        socket_.close();
    }

    void start()
    {
        MyClients.insert(this);
        acp.async_accept(socket_, [&](auto ec)
                         {
                            if(!ec){
                                NetServer *AnotherClient;
                                AnotherClient = new NetServer();
                                AnotherClient->start();
                                StartService();
                            }
                            else
                                cerr << "[Important] Server stop accept connection"; });
    }

    void send_bytes(const vector<uint8_t> &bytes)
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

    void send_reject(const string &reason)
    {
        vector<uint8_t> data_to_send;
        data_to_send.push_back(7);
        data_to_send.insert(data_to_send.end(), reason.begin(), reason.end());
        send_bytes(data_to_send);
    }
};

void return_to_room(uint16_t user_id)
{
    unique_lock<mutex> lk2(map_data_lock);
    if (user_id_to_client_stat.find(user_id) != user_id_to_client_stat.end())
    {
        user_id_to_client_stat[user_id] = 2;
        if (room_code_to_Room.find(user_id_to_room_code[user_id]) != room_code_to_Room.end())
        {
            room_code_to_Room[user_id_to_room_code[user_id]].GamePort = 0;
            room_code_to_Room[user_id_to_room_code[user_id]].WatchingNumber = 255;
        }
    }
}

uint16_t UpdateELO(uint16_t P1_ID, uint16_t P2_ID, uint16_t P1_ELO, uint16_t P2_ELO, uint8_t GameStat)
{
    // unique_lock<mutex> lk3(map_data_lock); !!! Ensure the caller has lock properly !!!
    double E_A = 1.0 / (1.0 + pow(10.0, ((double)(P2_ELO - P1_ELO) / 400.0)));
    double E_B = 1.0 - E_A;
    int P1_delta_ELO = (int)(32 * ((GameStat == 5 ? 1.0 : (GameStat == 6 ? 0.0 : 0.5)) - E_A));
    int P2_delta_ELO = (int)(32 * ((GameStat == 5 ? 0.0 : (GameStat == 6 ? 1.0 : 0.5)) - E_B));
    uint16_t P1_new_ELO = max(0, (int)(P1_ELO) + P1_delta_ELO), P2_new_ELO = max(0, (int)(P2_ELO) + P2_delta_ELO);
    uint16_t room_code = 65535;
    if (user_id_to_NetServer.find(P1_ID) != user_id_to_NetServer.end())
    {
        if (user_id_to_NetServer.find(P1_ID) != user_id_to_NetServer.end())
            user_id_to_NetServer[P1_ID]->ThisPlayer.PlayerELO = P1_new_ELO;
        room_code = user_id_to_room_code[P1_ID];
    }
    if (user_id_to_NetServer.find(P2_ID) != user_id_to_NetServer.end())
    {
        if (user_id_to_NetServer.find(P2_ID) != user_id_to_NetServer.end())
            user_id_to_NetServer[P2_ID]->ThisPlayer.PlayerELO = P2_new_ELO;
        room_code = user_id_to_room_code[P2_ID];
    }
    if (room_code != 65535)
    {
        room_code_to_Room[room_code].HostPlayerELO = P1_new_ELO;
        if (room_code_to_Room[room_code].Player2ID != 65535)
            room_code_to_Room[room_code].Player2ELO = P2_new_ELO;
    }
    vector<uint8_t> data_to_db;
    data_to_db.push_back(50);
    data_to_db.push_back(P1_ID / 256);
    data_to_db.push_back(P1_ID % 256);
    data_to_db.push_back(P1_new_ELO / 256);
    data_to_db.push_back(P1_new_ELO % 256);
    MyDBserver.send_bytes(data_to_db);
    data_to_db[1] = P2_ID / 256;
    data_to_db[2] = P2_ID % 256;
    data_to_db[3] = P2_new_ELO / 256;
    data_to_db[4] = P2_new_ELO % 256;
    MyDBserver.send_bytes(data_to_db);
    return room_code;
}

void NotifyAllUser_PublicRoomList()
{
    vector<uint8_t> data_to_db;
    data_to_db.push_back(17);
    MyDBserver.send_bytes(data_to_db);
    int cnt = 0;
    while (MyDBserver.received_data.empty() && cnt < 1000)
    {
        this_thread::sleep_for(1ms);
        cnt++;
    }
    if (!MyDBserver.received_data.empty())
    {
        vector<uint8_t> data(MyDBserver.received_data.front().bytes);
        MyDBserver.received_data.pop();
        for (auto &NS : MyClients)
            if (user_id_to_client_stat[NS->ThisPlayer.UserID] == 1)
                NS->send_bytes(data);
    }
    else
        return;
}

void SendPublicRoomList(uint16_t user_id)
{
    if (user_id_to_NetServer.find(user_id) == user_id_to_NetServer.end())
        return;
    vector<uint8_t> data_to_db;
    data_to_db.push_back(17);
    MyDBserver.send_bytes(data_to_db);
    int cnt = 0;
    while (MyDBserver.received_data.empty() && cnt < 1000)
    {
        this_thread::sleep_for(1ms);
        cnt++;
    }
    if (!MyDBserver.received_data.empty())
    {
        vector<uint8_t> data(MyDBserver.received_data.front().bytes);
        MyDBserver.received_data.pop();
        if (user_id_to_NetServer.find(user_id) != user_id_to_NetServer.end())
            user_id_to_NetServer[user_id]->send_bytes(data);
    }
    else
        return;
}

void NotifyAllUser_OnlinePlayersList()
{
    vector<uint8_t> data_to_db;
    data_to_db.push_back(18);
    MyDBserver.send_bytes(data_to_db);
    int cnt = 0;
    while (MyDBserver.received_data.empty() && cnt < 1000)
    {
        this_thread::sleep_for(1ms);
        cnt++;
    }
    if (!MyDBserver.received_data.empty())
    {
        vector<uint8_t> data(MyDBserver.received_data.front().bytes);
        MyDBserver.received_data.pop();
        for (auto &NS : MyClients)
            if (user_id_to_client_stat[NS->ThisPlayer.UserID] == 2)
                NS->send_bytes(data);
    }
    else
        return;
}

void SendOnlinePlayersList(uint16_t user_id)
{
    if (user_id_to_NetServer.find(user_id) == user_id_to_NetServer.end())
        return;
    vector<uint8_t> data_to_db;
    data_to_db.push_back(18);
    MyDBserver.send_bytes(data_to_db);
    int cnt = 0;
    while (MyDBserver.received_data.empty() && cnt < 1000)
    {
        this_thread::sleep_for(1ms);
        cnt++;
    }
    if (!MyDBserver.received_data.empty())
    {
        vector<uint8_t> data(MyDBserver.received_data.front().bytes);
        MyDBserver.received_data.pop();
        if (user_id_to_NetServer.find(user_id) != user_id_to_NetServer.end())
            user_id_to_NetServer[user_id]->send_bytes(data);
    }
    else
        return;
}

void SendUpdatedRoomInfo(uint16_t room_code)
{
    if (room_code_to_Room.find(room_code) == room_code_to_Room.end())
        return;
    Room TheRoom = room_code_to_Room[room_code];
    vector<uint8_t> data;
    data.push_back(21);
    data.push_back(TheRoom.PrivatePublic);
    data.push_back(TheRoom.GameMode);
    data.push_back(TheRoom.WatchingNumber);
    data.push_back(TheRoom.room_code / 256);
    data.push_back(TheRoom.room_code % 256);
    data.push_back(TheRoom.HostPlayerELO / 256);
    data.push_back(TheRoom.HostPlayerELO % 256);
    data.push_back(TheRoom.Player2ELO / 256);
    data.push_back(TheRoom.Player2ELO % 256);
    data.push_back(TheRoom.HostPlayerUsername.length());
    data.push_back(TheRoom.Player2Username.length());
    data.insert(data.end(), TheRoom.HostPlayerUsername.begin(), TheRoom.HostPlayerUsername.end());
    data.insert(data.end(), TheRoom.Player2Username.begin(), TheRoom.Player2Username.end());
    if (user_id_to_NetServer.find(TheRoom.HostPlayerID) != user_id_to_NetServer.end())
        user_id_to_NetServer[TheRoom.HostPlayerID]->send_bytes(data);
    if (TheRoom.Player2ID != 65535)
        if (user_id_to_NetServer.find(TheRoom.Player2ID) != user_id_to_NetServer.end())
            user_id_to_NetServer[TheRoom.Player2ID]->send_bytes(data);
}

signed main()
{
    cout << "Lobby/Game server is booting..." << '\n';
    NetServer *MyClient = new NetServer();
    if (MyDBserver.connect("140.113.17.12", 54023))
    {
        cout << "DB server is connected.\n\nThe Tetris Lobby/Game server is running." << '\n';
        thread LobbyThread;
        LobbyThread = thread([&]
                             { MyClient->start(); client_ioc_.run(); });
        string cmd;
        cout << "=> ";
        while (cin >> cmd)
        {
            if (cmd == "stop")
            {
                cout << "Closing server..." << '\n';
                break;
            }
            else
                cout << "The only available command is \'stop\'" << '\n';
            cout << "=> ";
        }
        acp.close();
        for (auto &NS : MyClients)
            NS->stop();
        LobbyThread.join();
        for (auto &NS : MyClients)
            delete NS;
        MyDBserver.stop();
    }
    else
        cerr << "[Fatal] Can't reach DB server." << '\n';
    return 0;
}
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
// Windows: g++ DB-server.cpp -I asio -std=c++20 -lws2_32 -lmswsock -o DB-server.exe
// Linux: g++ DB-server.cpp -I asio -std=c++20 -o DB-server

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

class NetServer;
map<uint16_t, int> user_id_to_client_stat;
map<uint16_t, Room> room_code_to_Room;
map<uint16_t, uint16_t> user_id_to_room_code;

mutex map_data_lock;

class User
{
private:
    static uint16_t id_cnt;

public:
    string account, name, password;
    uint16_t ELO = 1000, GamePlayed = 0, uid = id_cnt++;
};
uint16_t User::id_cnt = 0;

map<uint16_t, string> user_id_to_account;
map<string, User> total_userlist;
map<string, bool> used_account, used_username, online_account;

void save_users(string filename, vector<User> &us) // Save users to gamedb
{
    ofstream out(filename + ".gamedb");
    for (auto v : us)
    {
        out << v.account << '\n';
        out << v.name << '\n';
        out << v.password << '\n';
        out << v.ELO << '\n';
        out << v.GamePlayed << '\n';
    }
    out.close();
}

void read_db(string filename) // Read users from gamedb
{
    ifstream in(filename + ".gamedb");
    string tmp;
    while (getline(in, tmp))
    {
        User player;
        player.account = tmp;
        getline(in, player.name);
        getline(in, player.password);
        getline(in, tmp);
        player.ELO = stoi(tmp);
        getline(in, tmp);
        player.GamePlayed = stoi(tmp);
        used_account[player.account] = 1;
        used_username[player.name] = 1;
        total_userlist[player.account] = player;
        user_id_to_account[player.uid] = player.account;
    }
}

void write_log(string filename, string content) // Game Log
{
    ofstream out(filename + ".gamelog", std::ios::app);
    out << content << '\n'
        << endl;
    out.close();
}

/*
client_stat definition:
    0: Unqualified
    1: Lobby
    2: In room
    3: In game
*/

asio::io_context lobby_server_ioc_;
tcp::acceptor acp(lobby_server_ioc_, tcp::endpoint(tcp::v4(), 54023));
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
                            if(!ec){
                                timer_.cancel();
                                received_data_len = ntohl((read_buf[0]<<24)+(read_buf[1]<<16)+(read_buf[2]<<8)+read_buf[3]);
                                if(received_data_len>0 && received_data_len<65536)
                                    asio::async_read(socket_, asio::buffer(read_buf.data(), received_data_len), [&](asio::error_code ec, size_t)
                                                    {
                                                        unique_lock<mutex> lk(map_data_lock);
                                                        uint8_t data_type = read_buf[0];
                                                        vector<uint8_t> data;
                                                        if (data_type == 1){
                                                            data.push_back(2);
                                                            send_bytes(data);
                                                        }
                                                        else if (data_type == 3){
                                                            uint8_t usr_len = read_buf[1];
                                                            uint8_t acc_len = read_buf[2];
                                                            uint8_t pwd_len = read_buf[3];
                                                            string username(read_buf.begin() + 4, read_buf.begin() + 4 + usr_len);
                                                            string account(read_buf.begin() + 4 + usr_len, read_buf.begin() + 4 + usr_len + acc_len);
                                                            string password(read_buf.begin() + 4 + usr_len + acc_len, read_buf.begin() + 4 + usr_len + acc_len + pwd_len);
                                                            if(used_username[username])
                                                                send_double_reject("Username has been used!");
                                                            else if(used_account[account])
                                                                send_double_reject("Account has been used!");
                                                            else{
                                                                User new_user;
                                                                new_user.name = username;
                                                                new_user.account = account;
                                                                new_user.password = password;
                                                                used_username[username] = 1;
                                                                used_account[account] = 1;
                                                                total_userlist[account] = new_user;
                                                                user_id_to_account[new_user.uid] = account;
                                                                data.push_back(6); // forward by lobby server
                                                                data.push_back(6);
                                                                send_bytes(data);
                                                                write_log("Tetris", account + " registered.");
                                                            }
                                                        }
                                                        else if (data_type == 8){
                                                            uint8_t acc_len = read_buf[1];
                                                            uint8_t pwd_len = read_buf[2];
                                                            string account(read_buf.begin() + 3, read_buf.begin() + 3 + acc_len);
                                                            string password(read_buf.begin() + 3 + acc_len, read_buf.begin() + 3 + acc_len + pwd_len);
                                                            if(total_userlist.find(account) == total_userlist.end() || total_userlist[account].password != password)
                                                                send_double_reject("Account/Password is incorrect.");
                                                            else if(online_account[account])
                                                                send_double_reject("You are NOT ALLOW to login twice in different process!");
                                                            else{
                                                                User the_user = total_userlist[account];
                                                                online_account[account] = 1;
                                                                data.push_back(6);
                                                                data.push_back(the_user.uid / 256);
                                                                data.push_back(the_user.uid % 256);
                                                                data.push_back(the_user.ELO / 256);
                                                                data.push_back(the_user.ELO % 256);
                                                                data.push_back(the_user.GamePlayed / 256);
                                                                data.push_back(the_user.GamePlayed % 256);
                                                                data.push_back(the_user.name.length());
                                                                data.insert(data.end(), the_user.name.begin(), the_user.name.end());
                                                                send_bytes(data);
                                                                write_log("Tetris", account + " login.");
                                                                user_id_to_client_stat[the_user.uid] = 1;
                                                            }
                                                        }
                                                        else if (data_type == 9){
                                                            uint16_t uid = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            write_log("Tetris", user_id_to_account[uid] + " logout.");
                                                            online_account.erase(user_id_to_account[uid]);
                                                            user_id_to_client_stat.erase(uid);
                                                            user_id_to_room_code.erase(uid);
                                                        }
                                                        else if (data_type == 10){ // create room
                                                            uint16_t uid = (uint16_t)(read_buf[3]<<8) + read_buf[4];
                                                            if(user_id_to_client_stat[uid] == 1){
                                                                user_id_to_client_stat[uid] = 2;
                                                                Room tmp_room{};
                                                                tmp_room.PrivatePublic = read_buf[1];
                                                                tmp_room.GameMode = read_buf[2];
                                                                tmp_room.room_code = current_room_num++;
                                                                tmp_room.HostPlayerID = uid;
                                                                tmp_room.HostPlayerELO = total_userlist[user_id_to_account[uid]].ELO;
                                                                tmp_room.HostPlayerUsername = total_userlist[user_id_to_account[uid]].name;
                                                                room_code_to_Room[tmp_room.room_code] = tmp_room;
                                                                user_id_to_room_code[uid] = tmp_room.room_code;
                                                                write_log("Tetris", user_id_to_account[uid] + " create a room. Room code: " + to_string(tmp_room.room_code));
                                                            }
                                                        }
                                                        else if (data_type == 11){ // leave room
                                                            uint16_t uid = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[uid] == 2){
                                                                uint16_t player_room_code = user_id_to_room_code[uid];
                                                                if(room_code_to_Room[player_room_code].HostPlayerUsername==total_userlist[user_id_to_account[uid]].name){ // host left
                                                                    if(room_code_to_Room[player_room_code].Player2ID != 65535){
                                                                        user_id_to_client_stat[room_code_to_Room[player_room_code].Player2ID] = 1;
                                                                        user_id_to_room_code.erase(room_code_to_Room[player_room_code].Player2ID);
                                                                    }
                                                                    room_code_to_Room.erase(player_room_code);
                                                                    write_log("Tetris", "The host " + user_id_to_account[uid] + " leave a room. The room is closed. Room code: " + to_string(player_room_code));
                                                                }
                                                                else{
                                                                    room_code_to_Room[player_room_code].Player2ID = 65535;
                                                                    room_code_to_Room[player_room_code].Player2ELO = 0;
                                                                    room_code_to_Room[player_room_code].Player2Username = "";
                                                                    write_log("Tetris", user_id_to_account[uid] + " leave a room. Room code: " + to_string(player_room_code));
                                                                }
                                                                user_id_to_room_code.erase(uid);
                                                                user_id_to_client_stat[uid] = 1;
                                                            }
                                                        }
                                                        else if (data_type == 12){ // join room
                                                            uint16_t uid = (uint16_t)(read_buf[3]<<8) + read_buf[4];
                                                            uint16_t join_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[uid] == 1){
                                                                if(room_code_to_Room.find(join_room_code) != room_code_to_Room.end() && room_code_to_Room[join_room_code].Player2ID == 65535){
                                                                    room_code_to_Room[join_room_code].Player2ID = uid;
                                                                    room_code_to_Room[join_room_code].Player2ELO = total_userlist[user_id_to_account[uid]].ELO;
                                                                    room_code_to_Room[join_room_code].Player2Username = total_userlist[user_id_to_account[uid]].name;
                                                                    user_id_to_room_code[uid] = join_room_code;
                                                                    user_id_to_client_stat[uid] = 2;

                                                                    write_log("Tetris", user_id_to_account[uid] + " join a room. Room code: " + to_string(join_room_code));
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 13){ // watch room game
                                                            uint16_t uid = (uint16_t)(read_buf[3]<<8) + read_buf[4];
                                                            uint16_t watch_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[uid] == 1){
                                                                if(room_code_to_Room.find(watch_room_code) != room_code_to_Room.end() && room_code_to_Room[watch_room_code].WatchingNumber != 255){
                                                                    room_code_to_Room[watch_room_code].WatchingNumber++;
                                                                    write_log("Tetris", user_id_to_account[uid] + " watch a room game. Room code: " + to_string(watch_room_code));
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 15){ // accept invitation
                                                            uint16_t uid = (uint16_t)(read_buf[3]<<8) + read_buf[4];
                                                            uint16_t accepted_room_code = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[uid] == 1){
                                                                if(room_code_to_Room.find(accepted_room_code) != room_code_to_Room.end() && room_code_to_Room[accepted_room_code].Player2ID == 65535){
                                                                    room_code_to_Room[accepted_room_code].Player2ID = uid;
                                                                    room_code_to_Room[accepted_room_code].Player2ELO = total_userlist[user_id_to_account[uid]].ELO;
                                                                    room_code_to_Room[accepted_room_code].Player2Username = total_userlist[user_id_to_account[uid]].name;
                                                                    user_id_to_client_stat[uid] = 2;
                                                                    user_id_to_room_code[uid] = accepted_room_code;
                                                                    write_log("Tetris", user_id_to_account[uid] + " accept a invitaion and join a room. Room code: " + to_string(accepted_room_code));
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 17){ // get public room list
                                                            data.push_back(17); // forward by lobby server
                                                            data.push_back(19);
                                                            uint8_t data_num = 0;
                                                            for(auto &room_tmp : room_code_to_Room){
                                                                if(room_tmp.second.PrivatePublic == 1){
                                                                    data_num++;
                                                                    data.push_back(room_tmp.second.PrivatePublic);
                                                                    data.push_back(room_tmp.second.GameMode);
                                                                    data.push_back(room_tmp.second.WatchingNumber);
                                                                    data.push_back(room_tmp.second.room_code / 256);
                                                                    data.push_back(room_tmp.second.room_code % 256);
                                                                    data.push_back(room_tmp.second.HostPlayerELO / 256);
                                                                    data.push_back(room_tmp.second.HostPlayerELO % 256);
                                                                    data.push_back(room_tmp.second.Player2ELO / 256);
                                                                    data.push_back(room_tmp.second.Player2ELO % 256);
                                                                    data.push_back(room_tmp.second.HostPlayerUsername.length());
                                                                    data.push_back(room_tmp.second.Player2Username.length());
                                                                    data.insert(data.end(), room_tmp.second.HostPlayerUsername.begin(), room_tmp.second.HostPlayerUsername.end());
                                                                    data.insert(data.end(), room_tmp.second.Player2Username.begin(), room_tmp.second.Player2Username.end());
                                                                }
                                                            }
                                                            data.insert(data.begin() + 2, data_num);
                                                            send_bytes(data);
                                                        }
                                                        else if (data_type == 18){ // get online players list
                                                            data.push_back(18); // forward by lobby server
                                                            data.push_back(20);
                                                            uint8_t data_num = 0;
                                                            for(auto &uid_tmp : user_id_to_client_stat){
                                                                if(uid_tmp.second == 1){
                                                                    data_num++;
                                                                    User &user_tmp = total_userlist[user_id_to_account[uid_tmp.first]];
                                                                    data.push_back(user_tmp.uid / 256);
                                                                    data.push_back(user_tmp.uid % 256);
                                                                    data.push_back(user_tmp.ELO / 256);
                                                                    data.push_back(user_tmp.ELO % 256);
                                                                    data.push_back(user_tmp.GamePlayed / 256);
                                                                    data.push_back(user_tmp.GamePlayed % 256);
                                                                    data.push_back(user_tmp.name.length());
                                                                    data.insert(data.end(), user_tmp.name.begin(), user_tmp.name.end());
                                                                }
                                                            }
                                                            data.insert(data.begin() + 2, data_num);
                                                            send_bytes(data);
                                                        }
                                                        else if (data_type == 25){ // start the game
                                                            uint16_t uid = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            if(user_id_to_client_stat[uid] == 2){
                                                                uint16_t player_room_code = user_id_to_room_code[uid];
                                                                Room GameRoom = room_code_to_Room[player_room_code];
                                                                if(GameRoom.Player2ID != 65535){
                                                                    user_id_to_client_stat[uid] = 3;
                                                                    user_id_to_client_stat[GameRoom.Player2ID] = 3;
                                                                    room_code_to_Room[player_room_code].WatchingNumber = 0;
                                                                    write_log("Tetris", (string)"Start a " + (GameRoom.GameMode == 0 ? "Timed" : "Survival") + " room game. Room code: " + to_string(player_room_code));
                                                                }
                                                            }
                                                        }
                                                        else if (data_type == 50){ // update ELO
                                                            uint16_t uid = (uint16_t)(read_buf[1]<<8) + read_buf[2];
                                                            uint16_t new_elo = (uint16_t)(read_buf[3]<<8) + read_buf[4];
                                                            total_userlist[user_id_to_account[uid]].ELO = new_elo;
                                                            total_userlist[user_id_to_account[uid]].GamePlayed++;
                                                            if(user_id_to_client_stat.find(uid) != user_id_to_client_stat.end()){
                                                                user_id_to_client_stat[uid] = 2;
                                                                room_code_to_Room[user_id_to_room_code[uid]].WatchingNumber = 255;
                                                            }
                                                        }
                                                        StartService();
                                                    });
                                else ec2 = 1;
                            }
                            if(ec||ec2)
                            {
                                write_log("Tetris", "[Important] Game server is offline.");
                                cerr << "[Important] Game server is offline. Please stop server.";
                                socket_.close();
                                timer_.cancel();
                            } });
    }

public:
    NetServer() : socket_(lobby_server_ioc_),
                  timer_(lobby_server_ioc_)
    {
        read_buf.resize(65536);
    }

    ~NetServer()
    {
        socket_.close();
        timer_.cancel();
    }

    void start()
    {
        acp.async_accept(socket_, [&](auto ec)
                         {
                            if(!ec){
                                StartService();
                            } });
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

    void send_double_reject(const string &reason) // forward to user by lobby server
    {
        vector<uint8_t> data_to_send;
        data_to_send.push_back(7);
        data_to_send.push_back(7);
        data_to_send.insert(data_to_send.end(), reason.begin(), reason.end());
        send_bytes(data_to_send);
    }
};

signed main()
{
    cout << "Tetris DB server is booting..." << '\n';
    read_db("userlist");
    NetServer *MyLobbyServer = new NetServer();
    thread DBThread;
    DBThread = thread([&]
                      { MyLobbyServer->start(); lobby_server_ioc_.run(); });
    cout << "\nDB server is running." << '\n';
    cout << "\n[NOTE] For add/modify/delete operations, please ensure there is no online user." << '\n';
    write_log("Tetris", "[Important] DB server start.");
    string cmd;
    cout << "=> ";
    while (cin >> cmd)
    {
        if (cmd == "stop")
        {
            cout << "Closing server..." << '\n';
            break;
        }
        else if (cmd == "total")
        {
            for (auto v : total_userlist)
                cout << v.second.name << ' ' << v.second.account << ' ' << v.second.password << ' ' << v.second.ELO << ' ' << v.second.GamePlayed << '\n';
        }
        else if (cmd == "online")
        {
            for (auto v : online_account)
                cout << "   " << v.first << '\n';
        }
        else if (cmd == "add")
        {
            string usr, acc, pwd;
            cin >> usr >> acc >> pwd;
            User new_user;
            new_user.name = usr;
            new_user.account = acc;
            new_user.password = pwd;
            used_username[usr] = 1;
            used_account[acc] = 1;
            total_userlist[acc] = new_user;
            user_id_to_account[new_user.uid] = acc;
        }
        else if (cmd == "modify")
        {
            string target_acc, usr, acc, pwd;
            cin >> target_acc >> usr >> acc >> pwd;
            used_username[usr] = 1;
            used_account[acc] = 1;
            used_username.erase(total_userlist[target_acc].name);
            used_account.erase(total_userlist[target_acc].account);
            total_userlist[acc] = total_userlist[target_acc];
            total_userlist[acc].name = usr;
            total_userlist[acc].account = acc;
            total_userlist[acc].password = pwd;
            if (acc != target_acc)
                total_userlist.erase(target_acc);
            user_id_to_account[total_userlist[acc].uid] = acc;
        }
        else if (cmd == "delete")
        {
            string target_acc;
            cin >> target_acc;
            used_username.erase(total_userlist[target_acc].name);
            used_account.erase(total_userlist[target_acc].account);
            user_id_to_account.erase(total_userlist[target_acc].uid);
            total_userlist.erase(target_acc);
        }
        else
            cout << "No such command." << '\n'
                 << "The available commands are total/online/add/modify/delete." << '\n';
        cout << "=> ";
    }
    acp.close();
    lobby_server_ioc_.stop();
    DBThread.join();
    delete MyLobbyServer;
    vector<User> save_userlist;
    for (auto tmp : total_userlist)
        save_userlist.push_back(tmp.second);
    save_users("userlist", save_userlist);
    write_log("Tetris", "[Important] DB server stop.");
    return 0;
}

/*
data.push_back(19);
for (auto it = room_code_to_Room.begin(); it != room_code_to_Room.end(); it++)
{
    if (it->second.PrivatePublic == 1)
    {
        data.push_back(it->second.PrivatePublic);
        data.push_back(it->second.GameMode);
        data.push_back(it->second.WatchingNumber);
        data.push_back(it->second.room_code / 256);
        data.push_back(it->second.room_code % 256);
        data.push_back(it->second.HostPlayerELO / 256);
        data.push_back(it->second.HostPlayerELO % 256);
        data.push_back(it->second.Player2ELO / 256);
        data.push_back(it->second.Player2ELO % 256);
        data.push_back(it->second.HostPlayerUsername.length());
        data.push_back(it->second.Player2Username.length());
        data.insert(data.end(), it->second.HostPlayerUsername.begin(), it->second.HostPlayerUsername.end());
        data.insert(data.end(), it->second.Player2Username.begin(), it->second.Player2Username.end());
    }
}
*/
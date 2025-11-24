#include <functional>
#include <iostream>
#include <fstream>
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <asio.hpp>

using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

// Initialize io context and the main connection socket declaration
asio::io_context io;
tcp::socket connect_builder(io);

/*
Data Type Definition:
1 ping
2 pong

3 registration username
4 registration account name
5 registration password

6 Accept
7 Reject

8 login account
9 login password
10 logout

31 win a game
32 lose a game

// 50 get online players ip list. (Unused)
*/

// Compile command
// Windows: g++ server.cpp -I include -std=c++20 -lws2_32 -lmswsock -o server.exe
// Linux: g++ server.cpp -I include -std=c++20 -o server

// Standard responses declaration
const uint8_t pong[] = {2, 0, 0};
const uint8_t server_accept_it[] = {6, 0, 0};
string no_such_account = "No such account!";
string This_name_has_been_used = "This name has been used!";
string You_are_not_ego = "Why do you login twice in different process?\nYou must be egotist...";

vector<uint8_t> no_such_account_bytes(no_such_account.begin(), no_such_account.end());

vector<uint8_t> This_name_has_been_used_bytes(This_name_has_been_used.begin(), This_name_has_been_used.end());

vector<uint8_t> You_are_not_ego_bytes(You_are_not_ego.begin(), You_are_not_ego.end());

void phrase_init() // Standard responses initialization
{
    no_such_account_bytes.insert(no_such_account_bytes.begin(), no_such_account.length() % 256);
    no_such_account_bytes.insert(no_such_account_bytes.begin(), no_such_account.length() / 256);
    no_such_account_bytes.insert(no_such_account_bytes.begin(), 7);
    no_such_account_bytes.resize(no_such_account.length() + 3);

    This_name_has_been_used_bytes.insert(This_name_has_been_used_bytes.begin(), This_name_has_been_used.length() % 256);
    This_name_has_been_used_bytes.insert(This_name_has_been_used_bytes.begin(), This_name_has_been_used.length() / 256);
    This_name_has_been_used_bytes.insert(This_name_has_been_used_bytes.begin(), 7);
    This_name_has_been_used_bytes.resize(This_name_has_been_used.length() + 3);

    You_are_not_ego_bytes.insert(You_are_not_ego_bytes.begin(), You_are_not_ego.length() % 256);
    You_are_not_ego_bytes.insert(You_are_not_ego_bytes.begin(), You_are_not_ego.length() / 256);
    You_are_not_ego_bytes.insert(You_are_not_ego_bytes.begin(), 7);
    You_are_not_ego_bytes.resize(You_are_not_ego.length() + 3);
}

// User data structure
class User
{
public:
    string account, name, password;
    int win_game = 0, lose_game = 0;
};

// Sockets Pool for multiple users
vector<tcp::socket> avail_socket;
vector<asio::steady_timer> avail_socket_timer;
vector<tcp::acceptor> avail_socket_acp;
vector<vector<uint8_t>> receive_header_buf(1000, vector<uint8_t>(3));
vector<vector<uint8_t>> receive_buf(1000, vector<uint8_t>(1024));

// User and session lists
map<string, User> total_userlist; // account to user
map<uint16_t, string> session_port_to_current_account, session_port_to_reg_usrname, session_port_to_reg_account, session_port_to_reg_password, session_port_to_login_account, session_port_to_login_password;
map<string, bool> used_account, used_username, online_account;
// vector<User> online_userlist;

void save_users(string filename, vector<User> &us) // Save users to gamedb
{
    ofstream out(filename + ".gamedb");
    for (auto v : us)
    {
        out << v.account << '\n';
        out << v.name << '\n';
        out << v.password << '\n';
        out << v.win_game << '\n';
        out << v.lose_game << '\n';
        used_account[v.account] = 1;
        used_username[v.name] = 1;
    }
    out.close();
}

void read_db(string filename) // Read users from gamedb
{
    ifstream in(filename + ".gamedb");
    string tmp;
    User player;
    while (getline(in, player.account))
    {
        getline(in, player.name);
        getline(in, player.password);
        getline(in, tmp);
        player.win_game = stoi(tmp);
        getline(in, tmp);
        player.lose_game = stoi(tmp);
        used_account[player.account] = 1;
        used_username[player.name] = 1;
        total_userlist[player.account] = player;
    }
}

// Temporarily acceptor and available sessions queue
tcp::acceptor acp(io, tcp::endpoint(tcp::v4(), 54023));
queue<uint16_t> avail_port;
uint16_t ap_buf;

atomic<bool> server_stop;

void recycle(uint16_t snap_shot) // Remove a online user by session number
{
    online_account.erase(session_port_to_current_account[snap_shot]);
    session_port_to_reg_usrname.erase(snap_shot);
    session_port_to_reg_account.erase(snap_shot);
    session_port_to_reg_password.erase(snap_shot);
    session_port_to_current_account.erase(snap_shot);
    session_port_to_login_account.erase(snap_shot);
    session_port_to_login_password.erase(snap_shot);
}

void start_receive(uint16_t snap_shot) // Receive message from a online user by session number
{
    if (server_stop)
        return;
    avail_socket_timer[snap_shot].expires_after(5s);
    avail_socket_timer[snap_shot].async_wait([&, snap_shot](auto ec)
                                             {
                                                if(!ec){
                                                    avail_socket_acp[snap_shot].close();
                                                    avail_socket[snap_shot].close();
                                                    recycle(snap_shot);
                                                    avail_socket_acp[snap_shot].open(tcp::v4());
                                                    avail_socket_acp[snap_shot].set_option(asio::socket_base::reuse_address(true));
                                                    avail_socket_acp[snap_shot].bind({tcp::v4(), (uint16_t)(snap_shot+55000)});
                                                    avail_socket_acp[snap_shot].listen();
                                                    avail_port.push((uint16_t)(snap_shot+55000));
                                                } });
    asio::async_read(avail_socket[snap_shot], asio::buffer(receive_header_buf[snap_shot].data(), 3), [&, snap_shot](auto ec, size_t n)
                     {  if(!ec){
                            avail_socket_timer[snap_shot].cancel();
                            if(receive_header_buf[snap_shot][0]==1){ // ping -> pong
                                asio::async_write(avail_socket[snap_shot], asio::buffer(pong, 3), [&](auto ec, size_t n){});
                                start_receive(snap_shot);
                            }
                            else
                                asio::async_read(avail_socket[snap_shot], asio::buffer(receive_buf[snap_shot].data(), ((((uint16_t)receive_header_buf[snap_shot][1])<<8)+receive_header_buf[snap_shot][2])), [&, snap_shot](auto ec, size_t n){
                                    string msg(receive_buf[snap_shot].begin(),receive_buf[snap_shot].end());
                                    int len = ((((uint16_t)receive_header_buf[snap_shot][1]) << 8) + receive_header_buf[snap_shot][2]);
                                    if(receive_header_buf[snap_shot][0]==3){
                                        session_port_to_reg_usrname[snap_shot]=msg.substr(0, len);
                                        if(used_username[session_port_to_reg_usrname[snap_shot]])
                                            asio::async_write(avail_socket[snap_shot], asio::buffer(This_name_has_been_used_bytes.data(), This_name_has_been_used_bytes.size()), [&](auto ec, size_t n){});
                                        else
                                            asio::async_write(avail_socket[snap_shot], asio::buffer(server_accept_it, 3), [&](auto ec, size_t n){});
                                    }
                                    else if(receive_header_buf[snap_shot][0]==4){
                                        session_port_to_reg_account[snap_shot]=msg.substr(0, len);
                                        if(used_account[session_port_to_reg_account[snap_shot]])
                                            asio::async_write(avail_socket[snap_shot], asio::buffer(This_name_has_been_used_bytes.data(), This_name_has_been_used_bytes.size()), [&](auto ec, size_t n){});
                                        else
                                            asio::async_write(avail_socket[snap_shot], asio::buffer(server_accept_it, 3), [&](auto ec, size_t n){});
                                    }
                                    else if(receive_header_buf[snap_shot][0]==5){
                                        session_port_to_reg_password[snap_shot]=msg.substr(0, len);
                                        used_account[session_port_to_reg_account[snap_shot]]=1;
                                        used_username[session_port_to_reg_usrname[snap_shot]]=1;
                                        User tmp;
                                        tmp.account = session_port_to_reg_account[snap_shot];
                                        tmp.name = session_port_to_reg_usrname[snap_shot];
                                        tmp.password = session_port_to_reg_password[snap_shot];
                                        tmp.lose_game = 0;
                                        tmp.win_game = 0;
                                        total_userlist[session_port_to_reg_account[snap_shot]] = tmp;
                                        asio::async_write(avail_socket[snap_shot], asio::buffer(server_accept_it, 3), [&](auto ec, size_t n){});
                                    }
                                    else if(receive_header_buf[snap_shot][0]==8){
                                        session_port_to_login_account[snap_shot]=msg.substr(0, len);
                                    }
                                    else if(receive_header_buf[snap_shot][0]==9){
                                        session_port_to_login_password[snap_shot]=msg.substr(0, len);
                                        if(total_userlist[session_port_to_login_account[snap_shot]].password==session_port_to_login_password[snap_shot]){
                                            if(online_account[session_port_to_login_account[snap_shot]]==1){
                                                asio::async_write(avail_socket[snap_shot], asio::buffer(You_are_not_ego_bytes.data(), You_are_not_ego_bytes.size()), [&](auto ec, size_t n){});
                                            }
                                            else{
                                                //asio::async_write(avail_socket[snap_shot], asio::buffer(server_accept_it, 3), [&](auto ec, size_t n){});
                                                string login_name = total_userlist[session_port_to_login_account[snap_shot]].name;
                                                User login_user = total_userlist[session_port_to_login_account[snap_shot]];
                                                vector<uint8_t> welcome(login_name.begin(),login_name.end());
                                                welcome.insert(welcome.begin(),login_user.lose_game%256);
                                                welcome.insert(welcome.begin(),login_user.lose_game/256);
                                                welcome.insert(welcome.begin(),login_user.win_game%256);
                                                welcome.insert(welcome.begin(),login_user.win_game/256);
                                                welcome.insert(welcome.begin(),(login_name.length()+4)%256);
                                                welcome.insert(welcome.begin(),(login_name.length()+4)/256);
                                                welcome.insert(welcome.begin(),6);
                                                welcome.resize(login_name.length()+7);
                                                asio::async_write(avail_socket[snap_shot], asio::buffer(welcome, login_name.length()+7), [&](auto ec, size_t n){});
                                                session_port_to_current_account[snap_shot]=session_port_to_login_account[snap_shot];
                                                online_account[session_port_to_current_account[snap_shot]] = 1;
                                            }
                                        }
                                        else{
                                            if(total_userlist[session_port_to_login_account[snap_shot]].account=="")
                                                total_userlist.erase(session_port_to_login_account[snap_shot]);
                                            asio::async_write(avail_socket[snap_shot], asio::buffer(no_such_account_bytes.data(), no_such_account_bytes.size()), [&](auto ec, size_t n){});
                                        }
                                    }
                                    else if(receive_header_buf[snap_shot][0]==10){
                                        asio::async_write(avail_socket[snap_shot], asio::buffer(server_accept_it, 3), [&, snap_shot](auto ec, size_t n){
                                            avail_socket_acp[snap_shot].close();
                                            avail_socket[snap_shot].close();
                                            recycle(snap_shot);
                                            avail_socket_acp[snap_shot].open(tcp::v4());
                                            avail_socket_acp[snap_shot].set_option(asio::socket_base::reuse_address(true));
                                            avail_socket_acp[snap_shot].bind({tcp::v4(), (uint16_t)(snap_shot+55000)});
                                            avail_socket_acp[snap_shot].listen();
                                            avail_port.push((uint16_t)(snap_shot+55000));
                                        });
                                        return;
                                    }
                                    else if(receive_header_buf[snap_shot][0]==31){
                                        total_userlist[session_port_to_current_account[snap_shot]].win_game++;
                                    }
                                    else if(receive_header_buf[snap_shot][0]==32){
                                        total_userlist[session_port_to_current_account[snap_shot]].lose_game++;
                                    }
                                    start_receive(snap_shot);
                                }); 
                        } });
}

void start_accept() // Accept connections to build session
{
    acp.async_accept(connect_builder, [&](auto ec)
                     { if (!ec){
                        if(avail_port.empty()){
                            connect_builder.close();
                            start_accept();
                            return;
                        }
                        ap_buf = htons(avail_port.front());
                        uint16_t snap_shot = (uint16_t)(avail_port.front()-55000);
                        avail_socket_timer[snap_shot].expires_after(2s);
                        avail_socket_timer[snap_shot].async_wait([&, snap_shot](auto ec) {
                            if(!ec){
                                avail_socket_acp[snap_shot].close();
                                avail_socket[snap_shot].close();
                                avail_socket_acp[snap_shot].open(tcp::v4());
                                avail_socket_acp[snap_shot].set_option(asio::socket_base::reuse_address(true));
                                avail_socket_acp[snap_shot].bind({tcp::v4(), (uint16_t)(snap_shot+55000)});
                                avail_socket_acp[snap_shot].listen();
                                avail_port.push((uint16_t)(snap_shot+55000));
                            }
                        });
                        avail_socket_acp[snap_shot].async_accept(avail_socket[snap_shot], [&, snap_shot] (auto ec){
                            if(!ec){
                                avail_socket_timer[snap_shot].cancel();
                                start_receive(snap_shot);
                            }
                        });
                        avail_port.pop();
                        asio::async_write(connect_builder, asio::buffer(&ap_buf, 2), [&](auto ec, size_t n){
                            connect_builder.close();
                            start_accept();
                        });
                    } });
}

int main()
{
    // Initialization
    cout << "connect 4 server is booting..." << '\n';
    read_db("connect4");
    phrase_init();

    avail_socket.reserve(1000);
    avail_socket_timer.reserve(1000);
    avail_socket_acp.reserve(1000);

    for (int i = 55000; i < 56000; i++)
    {
        avail_socket.emplace_back(io);
        avail_socket_timer.emplace_back(io);
        avail_socket_acp.emplace_back(io);
        avail_socket_acp.back().open(tcp::v4());
        avail_socket_acp.back().set_option(asio::socket_base::reuse_address(true));
        avail_socket_acp.back().bind({tcp::v4(), ((uint16_t)i)});
        avail_socket_acp.back().listen();
        avail_port.push(i);
    }

    // Start Network in Background
    thread pingpong([&]
                    { start_accept(); io.run(); });
    cout << "\n\nThe Connect 4 server is running." << '\n';

    // Server UI
    string cmd;
    cout << "=> ";
    while (cin >> cmd)
    {
        if (cmd == "stop")
        {
            cout << "Closing and saving data..." << '\n';
            vector<User> save_userlist;
            for (auto tmp : total_userlist)
                save_userlist.push_back(tmp.second);
            server_stop = 1;
            acp.close();
            save_users("connect4", save_userlist);
            break;
        }
        else if (cmd == "total")
        {
            for (auto v : total_userlist)
                cout << v.second.name << ' ' << v.second.account << ' ' << v.second.password << ' ' << v.second.win_game << ' ' << v.second.lose_game << '\n';
        }
        else if (cmd == "online")
        {
            for (auto v : online_account)
                cout << "   " << v.first << '\n';
        }
        cout << "=> ";
    }
    pingpong.join();
}
//
//     ,-----.                                      ,--.      ,------.
//    '  .--./ ,---. ,--,--, ,--,--,  ,---.  ,---.,-'  '-.    |  .---',---. ,--.,--.,--.--.
//    |  |    | .-. ||      \|      \| .-. :| .--''-.  .-'    |  `--,| .-. ||  ||  ||  .--'
//    '  '--'\' '-' '|  ||  ||  ||  |\   --.\ `--.  |  |      |  |`  ' '-' ''  ''  '|  |
//     `-----' `---' `--''--'`--''--' `----' `---'  `--'      `--'    `---'  `----' `--'
//     ,-----.,--.,--.                 ,--.
//    '  .--./|  |`--' ,---. ,--,--, ,-'  '-.
//    |  |    |  |,--.| .-. :|      \'-.  .-'
//    '  '--'\|  ||  |\   --.|  ||  |  |  |
//     `-----'`--'`--' `----'`--''--'  `--'
//
//                                                                   v0.1 by 113550153
//
#include <functional>
#include <iostream>
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <asio.hpp>

using namespace std;
using namespace std::chrono_literals;
using asio::ip::tcp;
using asio::ip::udp;

// Initialize io context and socket declaration
asio::io_context io;
tcp::resolver r(io);
tcp::socket socket1(io);
udp::socket socket_udp1(io); //(io, udp::endpoint(udp::v4(), 52023));
tcp::socket socket2(io);
// Global endpoint declaration
auto lobby_server_ep = r.resolve("linux1.cs.nycu.edu.tw", "54023"); //(asio::ip::make_address("140.113.235.151"), 54023);
tcp::endpoint remote_player_ep;
udp::endpoint remote_endpoint;
udp::endpoint remote_endpoint2;
// Global timer declaration
asio::steady_timer timer1(io);
asio::steady_timer pingpong_timer(io);
asio::steady_timer pingpong_timer2(io);
asio::steady_timer rplayer_pingpong_timer(io);
asio::steady_timer rplayer_pingpong_timer2(io);
asio::steady_timer invitation_timer(io);
asio::steady_timer game_timer(io);

// Lobby Server Communication Header Example: This program primarily uses this structure, but I implement it byte by byte
struct Frame_header
{
    uint8_t type;
    uint16_t data_len;
};

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

11 You there?
12 Here I am.
13 invitation
14 accept invitation
15 tcp ip and port connect info
16 declined
17 I am choosing

21 place on x

31 win a game
32 lose a game

// 50 get online players ip list. (Unused)

*/

// Compile command
// Windows: g++ client.cpp -I include -std=c++20 -lws2_32 -lmswsock -o connect4game_client.exe
//          g++ client.cpp -I include -std=c++20 -lws2_32 -lmswsock -o client.exe
// Linux: g++ client.cpp -I include -std=c++20 -o client

// P2P States Header declaration
const uint8_t b11[] = {11};
// const uint8_t b13[] = {13};
// const uint8_t b15[] = {15};
// const uint8_t b12[] = {12};
const uint8_t b17[] = {17};
const uint8_t b14[] = {14};
const uint8_t b16[] = {16};

// Buffer and inter-thread data structure declaration
vector<uint8_t>
    rcv_buf(1024), rcv_data(1024), rcv_buf2(1024), udp_rcv_buf(1024);

queue<pair<uint8_t, vector<uint8_t>>>
    fetched_data;

queue<pair<uint8_t, uint8_t>>
    fetched_data2;

vector<tuple<udp::endpoint, string, uint8_t, uint8_t>> userlist;

string current_user;
atomic<uint16_t> user_win, user_lose;

atomic<int> invitation_stat;

atomic<uint8_t> invitation_player_xplevel_and_win_rate[2];
string invitation_player;
atomic<uint8_t> remote_player_tcp_info[6];
atomic<bool> waiting;
atomic<bool> ready;
atomic<bool> too_late;
atomic<bool> game_start;

// Flag for game session
atomic<bool> too_late2;

// Inter-thread mutex locker
mutex safe_locker;
mutex queue_locker;

atomic<bool>
    fatal = 0;

atomic<bool>
    fatal2 = 0;

atomic<bool>
    logout = 0;

uint16_t connect_server() // Try to reach lobby server and get a port for building tcp
{
    uint16_t npbuf = 0;
    timer1.expires_after(3s);
    timer1.async_wait([&](auto ec)
                      {
        if (!ec) { asio::error_code ig; socket1.close(ig); } });
    socket1.open(asio::ip::tcp::v4());
    socket1.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 0)); // 0 = let os assign
    asio::async_connect(socket1, lobby_server_ep, [&](asio::error_code ec, const tcp::endpoint &)
                        {
        timer1.cancel();
        if (ec)
        {
            cerr << "Unable to connect to the server: " << ec.message() << "\n";
            fatal = 1;
            return;
        }

        timer1.expires_after(3s);
        timer1.async_wait([&](auto ec)
                          {
        if (!ec) { asio::error_code ig;  socket1.close(ig); } });

        asio::async_read(socket1, asio::buffer(&npbuf, 2), [&](asio::error_code ec, size_t)
                         {
            timer1.cancel();
            if (ec) {
                cerr << "Unable to read port data from the server: " << ec.message() << "\n";
                fatal = 1;
                return; } }); });
    io.run();
    socket1.close();
    return ntohs(npbuf);
}

void start_heartbeat() // Heartbeat with lobby server
{
    pingpong_timer.expires_after(1s);
    pingpong_timer.async_wait([&](asio::error_code ec)
                              {
                        if (ec)
                            return;
                        static const uint8_t ping[] = {1,0,0}; // 1 = ping 2 = pong
                        asio::error_code ec2;
                        asio::write(socket1, asio::buffer(ping, sizeof(ping)), ec2);
                        if(ec2)
                            return;
                        start_heartbeat(); });
}

uint16_t data_len;
void data_reader() // Fetch data from lobby server
{
    pingpong_timer2.expires_after(5s);
    pingpong_timer2.async_wait([&](asio::error_code ec)
                               {
                        if (ec)
                            return;
                        else{
                            asio::error_code ig;
                            socket1.close(ig);
                            pingpong_timer.cancel();
                            fatal = 1;
                            return;
                        } });
    asio::async_read(socket1, asio::buffer(rcv_buf.data(), 3), [&](asio::error_code ec, size_t)
                     {
            if (!ec) {
                pingpong_timer2.cancel();
                uint8_t rcv_type = rcv_buf[0];
                data_len = ((((uint16_t)rcv_buf[1]) << 8) | rcv_buf[2]);
                //cout << "DATALEN: " << data_len << '\n';
                asio::async_read(socket1, asio::buffer(rcv_data.data(), data_len), [&, rcv_type](auto ec, size_t n){
                    if(!ec){
                        if(rcv_type!=2){
                            vector<uint8_t> payload(rcv_data.begin(), rcv_data.begin() + data_len);
                            unique_lock<std::mutex> lk(queue_locker);
                            fetched_data.push(make_pair(rcv_type,move(payload)));
                        }
                        data_reader();
                    }
                });
                
            } });
}

void send_data(uint8_t data_type, string data) // Send data to lobby server
{
    vector<uint8_t> send_buf(data.length() + 3);
    send_buf[0] = data_type;
    send_buf[1] = data.length() / 256;
    send_buf[2] = data.length() % 256;
    for (int i = 0; i < data.length(); i++)
        send_buf[i + 3] = (uint8_t)(data[i]);
    asio::error_code ec2;
    asio::write(socket1, asio::buffer(send_buf.data(), send_buf.size()), ec2);
    if (ec2)
        return;
    //            ,[&](asio::error_code, std::size_t) {}
}

void udp_waiting_player() // Receive and respond while waiting for the players
{
    socket_udp1.async_receive_from(asio::buffer(udp_rcv_buf), remote_endpoint, [&](asio::error_code ec, size_t n)
                                   { if (!ec){
                                    vector<uint8_t> b12;
                                    b12.push_back(12);
                                    b12.push_back((uint8_t)((user_win + user_lose) / 8));
                                    b12.push_back((uint8_t)(user_win != 0 ? (user_win * 100) / (user_win + user_lose) : 0));
                                    b12.insert(b12.end(),current_user.begin(),current_user.end());
                                    if(udp_rcv_buf[0]==11)
                                        socket_udp1.send_to(asio::buffer(b12.data(),3+current_user.length()),remote_endpoint);
                                    else if(udp_rcv_buf[0]==13){
                                        if(waiting)
                                            socket_udp1.send_to(asio::buffer(b17,1),remote_endpoint);
                                        // unique_lock<std::mutex> lk(safe_locker);
                                        remote_endpoint2 = remote_endpoint;
                                        invitation_player_xplevel_and_win_rate[0] = udp_rcv_buf[1];
                                        invitation_player_xplevel_and_win_rate[1] = udp_rcv_buf[2];
                                        string msg(udp_rcv_buf.begin(), udp_rcv_buf.end());
                                        invitation_player=msg.substr(3,n-3);
                                        invitation_timer.expires_after(6s); // 6s but claiming 5s
                                        waiting = 1;
                                        ready = 0;
                                        too_late = 0;
                                        invitation_timer.async_wait([&](asio::error_code ec){
                                            if(!ec){
                                                too_late = 1;
                                            }
                                        });
                                    }
                                    else if(udp_rcv_buf[0]==15&&remote_endpoint==remote_endpoint2){
                                        for (int i = 0; i < 6;i++)
                                            remote_player_tcp_info[i]=udp_rcv_buf[i+1];
                                        ready = 1;
                                        return;
                                    }
                                    udp_waiting_player();
                                   } });
}

void receive_avail_player() // Receive message while waiting for the player
{
    socket_udp1.async_receive_from(asio::buffer(udp_rcv_buf), remote_endpoint, [&](asio::error_code ec, size_t n)
                                   {if(!ec){
                                        if(udp_rcv_buf[0]==12){
                                            unique_lock<mutex> lk(safe_locker);
                                            string msg(udp_rcv_buf.begin(),udp_rcv_buf.end());
                                            userlist.push_back(make_tuple(remote_endpoint,msg.substr(3,n-3),udp_rcv_buf[1],udp_rcv_buf[2]));
                                        }
                                        else if(udp_rcv_buf[0]==14){
                                            invitation_stat = 1;
                                        }
                                        else if(udp_rcv_buf[0]==16){
                                            invitation_stat = 2;
                                        }
                                        else if(udp_rcv_buf[0]==17){
                                            invitation_stat = 3;
                                        }
                                        receive_avail_player();
                                    } });
}

void start_heartbeat2() // Heartbeat with remote player
{
    rplayer_pingpong_timer.expires_after(1s);
    rplayer_pingpong_timer.async_wait([&](asio::error_code ec)
                                      {
                        if (ec)
                            return;
                        static const uint8_t ping[] = {1,0}; // 1 = pingpong
                        asio::error_code ec2;
                        asio::write(socket2, asio::buffer(ping, sizeof(ping)), ec2);
                        if(ec2)
                            return;
                        start_heartbeat2(); });
}

void data_reader2() // Fetch data from remote player
{
    rplayer_pingpong_timer2.expires_after(5s);
    rplayer_pingpong_timer2.async_wait([&](asio::error_code ec)
                                       {
                        if (ec)
                            return;
                        else{
                            asio::error_code ig;
                            socket2.close(ig);
                            rplayer_pingpong_timer.cancel();
                            fatal2 = 1;
                            return;
                        } });
    asio::async_read(socket2, asio::buffer(rcv_buf2.data(), 2), [&](asio::error_code ec, size_t)
                     {
            if (!ec) {
                rplayer_pingpong_timer2.cancel();
                if(rcv_buf2[0]!=1){
                    unique_lock<std::mutex> lk(queue_locker);
                    fetched_data2.push(make_pair(rcv_buf2[0],rcv_buf2[1]));
                }
                data_reader2();
            } });
}

void draw(char table[6][7]) // Game table painting
{
    cout << "\x1b[2J\x1b[H" << flush;
    cout << "Opponent - " << invitation_player << '\n';
    cout << "LV. " << (int)invitation_player_xplevel_and_win_rate[0] << "   " << "Win rate: " << (int)invitation_player_xplevel_and_win_rate[1] << "%\n";
    cout << '\n';
    cout << "You: #       Opponent: X" << '\n';
    cout << '\n';
    cout << "| 1 | 2 | 3 | 4 | 5 | 6 | 7 |" << '\n';
    for (int i = 0; i < 29; i++)
        cout << '_';
    cout << '\n';
    for (int i = 0; i < 6; i++)
    {
        cout << "|";
        for (int j = 0; j < 7; j++)
            cout << " " << (table[i][j] != 0 ? (char)(table[i][j]) : '0') << " |";
        cout << '\n';
        for (int i = 0; i < 29; i++)
            cout << '-';
        cout << '\n';
    }
}

int check_table(char table[6][7]) // Check game winner
{
    char test = '#';
    for (int i = 0; i < 6; i++) //----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i][j + 1] == test && table[i][j + 2] == test && table[i][j + 3] == test)
                return 1;
    for (int i = 0; i < 3; i++) // vertical ----
        for (int j = 0; j < 7; j++)
            if (table[i][j] == test && table[i + 1][j] == test && table[i + 2][j] == test && table[i + 3][j] == test)
                return 1;
    for (int i = 0; i < 3; i++) // skew ----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i + 1][j + 1] == test && table[i + 2][j + 2] == test && table[i + 3][j + 3] == test)
                return 1;
    for (int i = 5; i > 2; i--) // skew ----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i - 1][j + 1] == test && table[i - 2][j + 2] == test && table[i - 3][j + 3] == test)
                return 1;
    test = 'X';
    for (int i = 0; i < 6; i++) //----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i][j + 1] == test && table[i][j + 2] == test && table[i][j + 3] == test)
                return 2;
    for (int i = 0; i < 3; i++) // vertical ----
        for (int j = 0; j < 7; j++)
            if (table[i][j] == test && table[i + 1][j] == test && table[i + 2][j] == test && table[i + 3][j] == test)
                return 2;
    for (int i = 0; i < 3; i++) // skew ----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i + 1][j + 1] == test && table[i + 2][j + 2] == test && table[i + 3][j + 3] == test)
                return 2;
    for (int i = 5; i > 2; i--) // skew ----
        for (int j = 0; j < 4; j++)
            if (table[i][j] == test && table[i - 1][j + 1] == test && table[i - 2][j + 2] == test && table[i - 3][j + 3] == test)
                return 2;
    return 0;
}

int main()
{
    // Game title UI and connecting to server
    cout << "\x1b[2J\x1b[H" << flush; // 80 * '-'
    string game_title = "--------------------------------------------------------------------------------\n                                                                                      \n ,-----.                                      ,--.      ,------.                      \n\'  .--./ ,---. ,--,--, ,--,--,  ,---.  ,---.,-\'  \'-.    |  .---\',---. ,--.,--.,--.--. \n|  |    | .-. ||      \\|      \\| .-. :| .--\'\'-.  .-\'    |  `--,| .-. ||  ||  ||  .--\' \n\'  \'--\'\\\' \'-\' \'|  ||  ||  ||  |\\   --.\\ `--.  |  |      |  |`  \' \'-\' \'\'  \'\'  \'|  |    \n `-----\' `---\' `--\'\'--\'`--\'\'--\' `----\' `---\'  `--\'      `--\'    `---\'  `----\' `--\'    \n ,-----.,--.,--.                 ,--.                                                 \n\'  .--./|  |`--\' ,---. ,--,--, ,-\'  \'-.                                               \n|  |    |  |,--.| .-. :|      \\\'-.  .-\'                                               \n\'  \'--\'\\|  ||  |\\   --.|  ||  |  |  |                                                 \n `-----\'`--\'`--\' `----\'`--\'\'--\'  `--\'                                                 \n                                                                                      \n                                                               v0.1 by 113550153\n--------------------------------------------------------------------------------\n";
    for (int i = 0; i < game_title.length(); i++)
    {
        cout << game_title[i];
        if (i % 6 == 0)
            this_thread::sleep_for(1ms);
    }
    this_thread::sleep_for(1500ms);

    cout << "Connecting ..." << '\n';
    uint16_t new_port = connect_server();
    if (fatal)
        return 1;

    if (new_port == 0)
    {
        cerr << "[Server Error] Server is busy now. Can't provide game service." << "\n";
        return 1;
    }

    // Tcp connection endpoint of the lobby server
    auto lobby_server_ep2 = r.resolve("linux1.cs.nycu.edu.tw", to_string(new_port));

    // Connect to server specified port
    socket1.open(asio::ip::tcp::v4());
    socket1.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 0));
    timer1.expires_after(3s);
    timer1.async_wait([&](auto ec)
                      {
                    if (!ec) { asio::error_code ig; socket1.close(ig); } });
    asio::async_connect(socket1, lobby_server_ep2, [&](asio::error_code ec, const tcp::endpoint &)
                        {
                            timer1.cancel();
                            if (ec)
                            {
                                cerr << "[Server Error] Unable to connect to the server specified port: " << ec.message() << "\n";
                                fatal = 1;
                                return;
                            } });
    io.restart();
    io.run();
    if (fatal)
        return 1;

    // Start Network in Background (Including Login process and Heartbeating)
    thread pingpong([&]
                    {
                    start_heartbeat();
                    data_reader();
                    io.restart();
                    io.run();
                    if(fatal && !logout) {cerr << "[Server Error] Lost connection with the lobby server." << "\n"; std::exit(1);} });

    // UI block
    while (true) // Home UI
    {
        int opt, ui_stat = 0;
        while (true) // Home Page
        {
            if (ui_stat == -1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "No such option, please try again." << '\n';
                this_thread::sleep_for(2000ms);
            }
            cout << "\x1b[2J\x1b[H" << flush;
            cout << "Please choose an option:" << '\n';
            cout << "1. Login" << '\n';
            cout << "2. Register an new account" << '\n';
            cout << "=> ";
            string tmps;
            getline(cin, tmps);
            try
            {
                opt = stoi(tmps);
            }
            catch (...)
            {
                opt = 0;
            }
            this_thread::sleep_for(700ms);
            if (opt == 1)
            {
                ui_stat = 1;
                break;
            }
            else if (opt == 2)
            {
                ui_stat = 2;
                break;
            }
            else
                ui_stat = -1;
        }
        if (ui_stat == 1) // Login Page
        {
            cout << "\x1b[2J\x1b[H" << flush;
            cout << "Login Page" << '\n';
            cout << "Account:";
            string account, password;
            getline(cin, account);
            cout << "Password:";
            getline(cin, password);
            send_data(8, account);
            send_data(9, password);
            pair<uint8_t, vector<uint8_t>> stat;
            while (1)
            {
                this_thread::sleep_for(100ms);
                unique_lock<std::mutex> lk(queue_locker);
                if (!fetched_data.empty())
                {
                    stat = fetched_data.front();
                    fetched_data.pop();
                    break;
                }
            }
            if (stat.first == 6)
            {
                string msg(stat.second.begin(), stat.second.end());
                user_win = (((uint16_t)((stat.second)[0]) << 8) | ((stat.second)[1]));
                user_lose = (((uint16_t)((stat.second)[2]) << 8) | ((stat.second)[3]));
                current_user = msg.substr(4);
                cout << "Account has been verified. Welcome, " << current_user << " !" << '\n';
                this_thread::sleep_for(1000ms);
                break;
            }
            else
            {
                string msg(stat.second.begin(), stat.second.end());
                cout << "Server Reject: " << msg << '\n';
                this_thread::sleep_for(1500ms);
            }
        }
        else if (ui_stat == 2) // Registration Page
        {
            string s1, s2, s3;
            while (1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "Registration (1/3)" << '\n';
                cout << "The length should not exceed 64 characters" << '\n';
                cout << "Please enter your player username, only alphabets, numbers, and \"_\" are allowed: ";
                cin.clear();
                getline(cin, s1);
                bool ok = 1;
                for (int i = 0; i < s1.length(); i++)
                    if (s1.length() > 64 || !(s1[i] >= '0' && s1[i] <= '9' || s1[i] >= 'a' && s1[i] <= 'z' || s1[i] == '_' || s1[i] >= 'A' && s1[i] <= 'Z'))
                    {
                        cout << "This name is invalid, please try again." << '\n';
                        this_thread::sleep_for(1000ms);
                        ok = 0;
                        break;
                    }
                if (!ok)
                    continue;
                if (s1 != "")
                {
                    send_data(3, s1);
                    pair<uint8_t, vector<uint8_t>> stat;
                    while (1)
                    {
                        this_thread::sleep_for(100ms);
                        unique_lock<std::mutex> lk(queue_locker);
                        if (!fetched_data.empty())
                        {
                            stat = fetched_data.front();
                            fetched_data.pop();
                            break;
                        }
                    }
                    if (stat.first == 6)
                    {
                        cout << "Username accepted." << '\n';
                        this_thread::sleep_for(700ms);
                        break;
                    }
                    else
                    {
                        string msg(stat.second.begin(), stat.second.end());
                        cout << "Server Reject: " << msg << '\n';
                        this_thread::sleep_for(1500ms);
                    }
                }
            }
            while (1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "Registration (2/3)" << '\n';
                cout << "The length should not exceed 64 characters" << '\n';
                cout << "Please enter your account name, only alphabets, numbers, and \"_\" are allowed: ";
                cin.clear();
                getline(cin, s2);
                bool ok = 1;
                for (int i = 0; i < s2.length(); i++)
                    if (s2.length() > 64 || !(s2[i] >= '0' && s2[i] <= '9' || s2[i] >= 'a' && s2[i] <= 'z' || s2[i] == '_' || s2[i] >= 'A' && s2[i] <= 'Z'))
                    {
                        cout << "This name is invalid, please try again." << '\n';
                        this_thread::sleep_for(1000ms);
                        ok = 0;
                        break;
                    }
                if (!ok)
                    continue;
                if (s2 != "")
                {
                    send_data(4, s2);
                    pair<uint8_t, vector<uint8_t>> stat;
                    while (1)
                    {
                        this_thread::sleep_for(100ms);
                        unique_lock<std::mutex> lk(queue_locker);
                        if (!fetched_data.empty())
                        {
                            stat = fetched_data.front();
                            fetched_data.pop();
                            break;
                        }
                    }
                    if (stat.first == 6)
                    {
                        cout << "Account name accepted." << '\n';
                        this_thread::sleep_for(700ms);
                        break;
                    }
                    else
                    {
                        string msg(stat.second.begin(), stat.second.end());
                        cout << "Server Reject: " << msg << '\n';
                        this_thread::sleep_for(1500ms);
                    }
                }
            }
            while (1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "Registration (3/3)" << '\n';
                cout << "The length should not exceed 64 characters" << '\n';
                cout << "Please enter your account password, it should be at least 4 characters: ";
                cin.clear();
                getline(cin, s3);
                if (s3 != "" && s3.length() >= 4 && s3.length() <= 64)
                {
                    send_data(5, s3);
                    pair<uint8_t, vector<uint8_t>> stat;
                    while (1)
                    {
                        this_thread::sleep_for(100ms);
                        unique_lock<std::mutex> lk(queue_locker);
                        if (!fetched_data.empty())
                        {
                            stat = fetched_data.front();
                            fetched_data.pop();
                            break;
                        }
                    }
                    if (stat.first == 6)
                    {
                        cout << "Password accepted." << '\n';
                        this_thread::sleep_for(700ms);
                        break;
                    }
                    else
                    {
                        string msg(stat.second.begin(), stat.second.end());
                        cout << "Server Reject: " << msg << '\n';
                        this_thread::sleep_for(1500ms);
                    }
                }
                if (s3 != "")
                {
                    cout << "Your password should be 4 ~ 64 characters long, please try again." << '\n';
                    this_thread::sleep_for(1000ms);
                }
            }
            cout << "\x1b[2J\x1b[H" << flush;
            this_thread::sleep_for(500ms);
            cout << "Account has been registered successfully!" << '\n';
            this_thread::sleep_for(1500ms);
            cout << "Please log in with your account on the home page, have fun!" << '\n';
            this_thread::sleep_for(3000ms);
        }
    }

    while (true) // Lobby UI
    {
        int opt, ui_stat = 0;
        while (true) // Lobby Page
        {
            if (ui_stat == -1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "No such option, please try again." << '\n';
                this_thread::sleep_for(2000ms);
            }
            cout << "\x1b[2J\x1b[H" << flush;
            cout << "Hi, " << current_user << '\n';
            cout << "LV. " << (int)((user_win + user_lose) / 8) << '\n';
            cout << "Win rate: " << (int)(user_win != 0 ? (user_win * 100) / (user_win + user_lose) : 0) << "%" << '\n';
            cout << '\n';
            cout << "Please choose an option:" << '\n';
            cout << "1. Join a Game" << '\n';
            cout << "2. Create a Room" << '\n';
            cout << "3. Exit" << '\n';
            cout << "=> ";
            string tmps;
            getline(cin, tmps);
            try
            {
                opt = stoi(tmps);
            }
            catch (...)
            {
                opt = 0;
            }
            this_thread::sleep_for(700ms);
            if (opt == 1)
            {
                ui_stat = 1;
                break;
            }
            else if (opt == 2)
            {
                ui_stat = 2;
                break;
            }
            else if (opt == 3)
            {
                ui_stat = 3;
                break;
            }
            else
                ui_stat = -1;
        }
        if (ui_stat == 1) // Join a game Plyer UI
        {
            socket_udp1.open(udp::v4());
            socket_udp1.set_option(asio::socket_base::reuse_address(true));
            socket_udp1.bind({udp::v4(), 52023});
            receive_avail_player();
            while (1)
            {
                unique_lock<mutex> lk(safe_locker);
                userlist.clear();
                lk.unlock();
                // send_data(50, "");
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "Refreshing..." << '\n';
                /*vector<uint8_t> bytelist = {
                    140,
                    113,
                    235,
                    151,
                    140,
                    113,
                    235,
                    152,
                    140,
                    113,
                    235,
                    153,
                    140,
                    113,
                    235,
                    154};*/
                vector<uint8_t> bytelist;
                {
                    udp::resolver ur(io);
                    const char *hosts[] = {
                        "linux1.cs.nycu.edu.tw",
                        "linux2.cs.nycu.edu.tw",
                        "linux3.cs.nycu.edu.tw",
                        "linux4.cs.nycu.edu.tw"};
                    std::set<uint32_t> seen;
                    for (auto h : hosts)
                    {
                        asio::error_code ec;
                        auto results = ur.resolve(udp::v4(), h, "52023", ec);
                        if (ec)
                            continue;

                        for (auto &r : results)
                        {
                            auto addr = r.endpoint().address();
                            if (!addr.is_v4())
                                continue;

                            auto v4 = addr.to_v4().to_bytes(); // array<uint8_t,4>
                            uint32_t key = (uint32_t(v4[0]) << 24) | (uint32_t(v4[1]) << 16) | (uint32_t(v4[2]) << 8) | uint32_t(v4[3]);
                            if (seen.insert(key).second)
                            {
                                bytelist.insert(bytelist.end(), v4.begin(), v4.end());
                            }
                            break;
                        }
                    }
                }
                /*while (1)
                {
                    this_thread::sleep_for(100ms);
                    unique_lock<std::mutex> lk(queue_locker);
                    if (!fetched_data.empty())
                    {
                        bytelist = fetched_data.front().second;
                        fetched_data.pop();
                        break;
                    }
                }*/
                for (int i = 0; i < bytelist.size() / 4; i++)
                {
                    asio::ip::address_v4::bytes_type bytes_addr{{bytelist[i * 4 + 0], bytelist[i * 4 + 1], bytelist[i * 4 + 2], bytelist[i * 4 + 3]}};
                    asio::ip::address_v4 addr(bytes_addr);
                    udp::endpoint online_player_ep(addr, 52023);
                    socket_udp1.send_to(asio::buffer(b11, 1), online_player_ep);
                }
                this_thread::sleep_for(1000ms);
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "This is the available players list:" << '\n';
                unique_lock<mutex> lk2(safe_locker);
                vector<tuple<udp::endpoint, string, uint8_t, uint8_t>> userlist_snap(userlist);
                lk2.unlock();
                for (int i = 0; i < userlist_snap.size(); i++)
                {
                    // unique_lock<mutex> lk(safe_locker);
                    cout << i + 1 << ".   " << get<1>(userlist_snap[i]) << "   LV." << (int)(get<2>(userlist_snap[i])) << "   Win rate: " << (int)(get<3>(userlist_snap[i])) << '%' << '\n';
                }
                // lk.unlock();
                cout << "Enter a number to send invitation to that player." << '\n';
                cout << "\'x\' to exit this page, or just press enter to refresh the player list." << '\n';
                cout << "=> ";
                string tmps;
                getline(cin, tmps);
                if (tmps == "x")
                {
                    opt = -100;
                    break;
                }
                else
                {
                    try
                    {
                        opt = stoi(tmps);
                    }
                    catch (...)
                    {
                        opt = 0;
                    }
                    if (opt != 0 && opt >= 1 && opt <= userlist_snap.size())
                    {
                        vector<uint8_t> b13;
                        b13.push_back(13);
                        b13.push_back((uint8_t)((user_win + user_lose) / 8));
                        b13.push_back((uint8_t)((user_win != 0 ? (user_win * 100) / (user_win + user_lose) : 0)));
                        b13.insert(b13.end(), current_user.begin(), current_user.end());
                        invitation_stat = 0;
                        socket_udp1.send_to(asio::buffer(b13.data(), 3 + current_user.length()), get<0>(userlist_snap[opt - 1]));
                        cout << "inviting \'" << get<1>(userlist_snap[opt - 1]) << "\'..." << '\n';
                        invitation_player = get<1>(userlist_snap[opt - 1]);
                        invitation_player_xplevel_and_win_rate[0] = get<2>(userlist_snap[opt - 1]);
                        invitation_player_xplevel_and_win_rate[1] = get<3>(userlist_snap[opt - 1]);
                        int cnt = 0;
                        while (invitation_stat == 0 && cnt < 70)
                        {
                            this_thread::sleep_for(100ms);
                            cnt++;
                        }
                        if (invitation_stat == 1)
                        {
                            cout << "The player " << get<1>(userlist_snap[opt - 1]) << " accept your invitaion!" << '\n';
                            game_start = 0;
                            array<uint8_t, 4> myip_arr = socket1.local_endpoint().address().to_v4().to_bytes();
                            vector<uint8_t> mytcp_info(myip_arr.begin(), myip_arr.end()); // b15=mytcp_info
                            uint16_t myport = 23023;
                            while (1)
                            {
                                try
                                {
                                    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), myport));
                                    break;
                                }
                                catch (...)
                                {
                                    cout << "Can't use " << myport << " port, try to use " << ++myport << " instead." << '\n';
                                }
                            }
                            tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), myport));
                            mytcp_info.insert(mytcp_info.begin(), 15);
                            mytcp_info.push_back(myport / 256);
                            mytcp_info.push_back(myport % 256);
                            invitation_timer.expires_after(2s);
                            invitation_timer.async_wait([&](auto ec)
                                                        { if (!ec) acceptor.close(); });
                            acceptor.async_accept(socket2, [&](auto ec)
                                                  {
                                                    if(!ec){
                                                        invitation_timer.cancel();
                                                        game_start = 1;
                                                    } });
                            socket_udp1.send_to(asio::buffer(mytcp_info.data(), mytcp_info.size()), get<0>(userlist_snap[opt - 1]));
                            int times = 0;
                            while (game_start == 0 && times < 25)
                            {
                                this_thread::sleep_for(100ms);
                                times++;
                            }
                            socket_udp1.close();
                            opt = 0;
                            if (!game_start)
                            {
                                cout << "Unexpected error, connection time out." << '\n';
                                this_thread::sleep_for(1000ms);
                                cout << "Return to the lobby." << '\n';
                                this_thread::sleep_for(1500ms);
                                opt = -100;
                                break;
                            }
                            acceptor.close();
                            fatal2 = 0;
                            start_heartbeat2();
                            data_reader2();
                            break;
                        }
                        else if (invitation_stat == 2)
                        {
                            cout << "The player " << get<1>(userlist_snap[opt - 1]) << " reject your invitaion." << '\n';
                            this_thread::sleep_for(1000ms);
                            cout << "Maybe you should look for other players." << '\n';
                            this_thread::sleep_for(1500ms);
                        }
                        else if (invitation_stat == 3)
                        {
                            cout << "The player " << get<1>(userlist_snap[opt - 1]) << " has received an invitation from someone else." << '\n';
                            this_thread::sleep_for(1000ms);
                            cout << "Maybe you should look for other players or wait a while." << '\n';
                            this_thread::sleep_for(1500ms);
                        }
                        else
                        {
                            cout << "Time out, player " << get<1>(userlist_snap[opt - 1]) << " didn\'t respond to your invitation." << '\n';
                            this_thread::sleep_for(1000ms);
                            cout << "Maybe you should look for other players." << '\n';
                            this_thread::sleep_for(1500ms);
                        }
                    }
                }
            }
            socket_udp1.close();
            if (opt == -100)
                continue;
            if (game_start)
            {
                int round = 0;
                too_late2 = 0;
                char game_table[6][7];
                memset(game_table, 0, sizeof(game_table));
                cout << "\x1b[2J\x1b[H" << flush;
                draw(game_table);
                while (1)
                {
                    // Your turn
                    game_timer.expires_after(30s);
                    game_timer.async_wait([&](asio::error_code ec)
                                          { if(!ec) too_late2=1; });
                    cout << '\n';
                    cout << "You have 30 seconds.";
                    cout << "It\'s your turn! Please choose a position to put token (1~7):" << '\n';
                    cout << "=> ";
                    int opt = 0;
                    string pos;
                    while (getline(cin, pos))
                    {
                        try
                        {
                            opt = stoi(pos);
                        }
                        catch (...)
                        {
                            opt = 0;
                        }
                        if (opt != 0 && opt >= 1 && opt <= 7 && game_table[0][opt - 1] == 0)
                            break;
                        cout << "Invalid position, please try again." << '\n';
                        cout << "=> ";
                    }
                    if (too_late2)
                    {
                        cout << "!!! More than 30 seconds !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "Did you just AFK? You don\'t need to meditate in this game!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    else
                    {
                        opt--;
                        game_timer.cancel();
                        too_late2 = 0;
                        uint8_t buf[] = {21, ((uint8_t)opt)};
                        asio::error_code ec2;
                        asio::write(socket2, asio::buffer(buf, sizeof(buf)), ec2);
                        if (ec2)
                        {
                            cout << "Unexpected error, connection failure occurs." << '\n';
                            this_thread::sleep_for(1000ms);
                            cout << "Return to the lobby." << '\n';
                            this_thread::sleep_for(1500ms);
                            break;
                        }
                    }
                    int y = 0;
                    while (y < 6 && game_table[y][opt] == 0)
                    {
                        game_table[y][opt] = '#';
                        draw(game_table);
                        game_table[y][opt] = 0;
                        y++;
                        this_thread::sleep_for(300ms);
                    }
                    game_table[y - 1][opt] = '#';
                    draw(game_table);
                    // Check
                    int win = check_table(game_table);
                    if (win == 1)
                    {
                        cout << "Connect 4 \'#\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else if (win == 2)
                    {
                        cout << "Connect 4 \'X\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    // Opponent's turn
                    game_timer.expires_after(35s);
                    game_timer.async_wait([&](asio::error_code ec)
                                          { if(!ec) too_late2=1; });
                    int wt_cnt = 0;
                    while (1)
                    {
                        this_thread::sleep_for(300ms);
                        unique_lock<std::mutex> lk(queue_locker);
                        if (fetched_data2.empty() && (!fatal2) && (!too_late2))
                        {
                            draw(game_table);
                            cout << "Opponent\'s turn";
                            for (int i = 0; i < wt_cnt; i++)
                                cout << '.';
                            cout << '\n';
                            wt_cnt = (wt_cnt + 1) % 4;
                        }
                        else
                            break;
                    }
                    if (too_late2)
                    {
                        cout << "Your opponent can\'t against you. Waste more than 30 seconds!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else
                    {
                        game_timer.cancel();
                        too_late2 = 0;
                    }
                    if (fatal2)
                    {
                        cout << "Unexpected error, connection time out." << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "Return to the lobby." << '\n';
                        this_thread::sleep_for(1500ms);
                        break;
                    }
                    unique_lock<std::mutex> lk(queue_locker);
                    pair<uint8_t, uint8_t> rplayer_op = fetched_data2.front();
                    fetched_data2.pop();
                    lk.unlock();
                    y = 0;
                    while (game_table[y][rplayer_op.second] == 0 && y < 6)
                    {
                        game_table[y][rplayer_op.second] = 'X';
                        draw(game_table);
                        game_table[y][rplayer_op.second] = 0;
                        y++;
                        this_thread::sleep_for(300ms);
                    }
                    game_table[y - 1][rplayer_op.second] = 'X';
                    draw(game_table);
                    // Check
                    win = check_table(game_table);
                    if (win == 1)
                    {
                        cout << "Connect 4 \'#\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else if (win == 2)
                    {
                        cout << "Connect 4 \'X\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    // round end
                    round += 2;
                    if (round == 42)
                    {
                        cout << "Both players didn\'t win this game." << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "It\'s TIE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        break;
                    }
                }
                socket2.close();
            }
        }
        else if (ui_stat == 2) // Create a room Plyer UI
        {
            socket_udp1.open(udp::v4());
            socket_udp1.set_option(asio::socket_base::reuse_address(true));
            socket_udp1.bind({udp::v4(), 52023});
            invitation_player = "";
            udp_waiting_player();
            int cnt = 0;
            while (1)
            {
                cout << "\x1b[2J\x1b[H" << flush;
                cout << "Waiting for player..." << '\n';
                cout << "Player 1: " << current_user << '\n';
                cout << "Player 2: ";
                cout << "waiting";
                for (int i = 0; i < cnt; i++)
                    cout << '.';
                cout << '\n';
                cnt = (cnt + 1) % 4;
                if (invitation_player != "")
                {
                    cout << "Invitation from " << invitation_player << '\n';
                    cout << invitation_player << "\'s Level: " << (int)invitation_player_xplevel_and_win_rate[0] << '\n';
                    cout << invitation_player << "\'s Win rate: " << (int)invitation_player_xplevel_and_win_rate[1] << '%' << '\n';
                    cout << "You have 5 seconds to make your choice." << '\n';
                    cout << "Do you want to accept it? (y/n)";
                    cout << "=> ";
                    string choice;
                    getline(cin, choice);
                    if (choice == "yes" || choice == "y")
                    {
                        if (too_late)
                        {
                            invitation_player = "";
                            too_late = 0;
                            cout << "Sorry, invitation is expired." << '\n';
                            this_thread::sleep_for(700ms);
                            waiting = 0;
                        }
                        else
                        {
                            game_start = 0;
                            invitation_timer.cancel();
                            socket_udp1.send_to(asio::buffer(b14, 1), remote_endpoint2);
                            cout << "Preparing game..." << '\n';
                            int times = 0;
                            while (!ready && times < 15)
                            {
                                this_thread::sleep_for(100ms);
                                times++;
                            }
                            if (!ready)
                            {
                                cout << "Unexpected error, connection time out." << '\n';
                                this_thread::sleep_for(1000ms);
                                cout << "Return to the lobby." << '\n';
                                this_thread::sleep_for(1500ms);
                                socket_udp1.close();
                                invitation_player = "";
                                too_late = 0;
                                waiting = 0;
                                break;
                            }
                            else
                            {
                                socket_udp1.close();
                                asio::ip::address_v4::bytes_type bytes_addr{{remote_player_tcp_info[0], remote_player_tcp_info[1], remote_player_tcp_info[2], remote_player_tcp_info[3]}};
                                asio::ip::address_v4 addr(bytes_addr);
                                remote_player_ep = tcp::endpoint(addr, ((((uint16_t)remote_player_tcp_info[4]) << 8) | remote_player_tcp_info[5]));
                                invitation_timer.expires_after(2s); // remote_player_tcp_info;
                                invitation_timer.async_wait([&](auto ec)
                                                            { if (!ec) socket2.close(); });
                                socket2.async_connect(remote_player_ep, [&](asio::error_code ec)
                                                      { if (!ec){
                                                        invitation_timer.cancel();
                                                        game_start = 1;
                                                      } });
                                times = 0;
                                while (!game_start && times < 25)
                                {
                                    this_thread::sleep_for(100ms);
                                    times++;
                                }
                                if (!game_start)
                                {
                                    cout << "Unexpected error, connection time out." << '\n';
                                    this_thread::sleep_for(1000ms);
                                    cout << "Return to the lobby." << '\n';
                                    this_thread::sleep_for(1500ms);
                                    invitation_player = "";
                                    too_late = 0;
                                    waiting = 0;
                                    break;
                                }
                                fatal2 = 0;
                                start_heartbeat2();
                                data_reader2();
                                break;
                            }
                        }
                    }
                    else if (choice == "no" || choice == "n")
                    {
                        socket_udp1.send_to(asio::buffer(b16), remote_endpoint2);
                        invitation_player = "";
                        waiting = 0;
                    }
                }
                this_thread::sleep_for(300ms);
            }
            socket_udp1.close();
            if (game_start)
            {
                int round = 0;
                too_late2 = 0;
                char game_table[6][7];
                memset(game_table, 0, sizeof(game_table));
                draw(game_table);
                while (1)
                {
                    // Opponent's turn
                    game_timer.expires_after(35s);
                    game_timer.async_wait([&](asio::error_code ec)
                                          { if(!ec) too_late2=1; });
                    int wt_cnt = 0;
                    while (1)
                    {
                        this_thread::sleep_for(300ms);
                        unique_lock<std::mutex> lk(queue_locker);
                        if (fetched_data2.empty() && (!fatal2) && (!too_late2))
                        {
                            draw(game_table);
                            cout << "Opponent\'s turn";
                            for (int i = 0; i < wt_cnt; i++)
                                cout << '.';
                            cout << '\n';
                            wt_cnt = (wt_cnt + 1) % 4;
                        }
                        else
                            break;
                    }
                    if (too_late2)
                    {
                        cout << "Your opponent can\'t against you. Waste more than 30 seconds!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else
                    {
                        game_timer.cancel();
                        too_late2 = 0;
                    }
                    if (fatal2)
                    {
                        cout << "Unexpected error, connection time out." << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "Return to the lobby." << '\n';
                        this_thread::sleep_for(1500ms);
                        invitation_player = "";
                        too_late = 0;
                        waiting = 0;
                        break;
                    }
                    unique_lock<std::mutex> lk(queue_locker);
                    pair<uint8_t, uint8_t> rplayer_op = fetched_data2.front();
                    fetched_data2.pop();
                    lk.unlock();
                    int y = 0;
                    while (game_table[y][rplayer_op.second] == 0 && y < 6)
                    {
                        game_table[y][rplayer_op.second] = 'X';
                        draw(game_table);
                        game_table[y][rplayer_op.second] = 0;
                        y++;
                        this_thread::sleep_for(300ms);
                    }
                    game_table[y - 1][rplayer_op.second] = 'X';
                    draw(game_table);
                    // Check
                    int win = check_table(game_table);
                    if (win == 1)
                    {
                        cout << "Connect 4 \'#\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else if (win == 2)
                    {
                        cout << "Connect 4 \'X\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    // Your turn
                    game_timer.expires_after(30s);
                    game_timer.async_wait([&](asio::error_code ec)
                                          { if(!ec) too_late2=1; });
                    cout << '\n';
                    cout << "You have 30 seconds.";
                    cout << "It\'s your turn! Please choose a position to put token (1~7):" << '\n';
                    cout << "=> ";
                    int opt = 0;
                    string pos;
                    while (getline(cin, pos))
                    {
                        try
                        {
                            opt = stoi(pos);
                        }
                        catch (...)
                        {
                            opt = 0;
                        }
                        if (opt != 0 && opt >= 1 && opt <= 7 && game_table[0][opt - 1] == 0)
                            break;
                        cout << "Invalid position, please try again." << '\n';
                        cout << "=> ";
                    }
                    if (too_late2)
                    {
                        cout << "!!! More than 30 seconds !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "Did you just AFK? You don\'t need to meditate in this game!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    else
                    {
                        opt--;
                        game_timer.cancel();
                        too_late2 = 0;
                        uint8_t buf[] = {21, ((uint8_t)opt)};
                        asio::error_code ec2;
                        asio::write(socket2, asio::buffer(buf, sizeof(buf)), ec2);
                        if (ec2)
                        {
                            cout << "Unexpected error, connection failure occurs." << '\n';
                            this_thread::sleep_for(1000ms);
                            cout << "Return to the lobby." << '\n';
                            this_thread::sleep_for(1500ms);
                            break;
                        }
                    }
                    y = 0;
                    while (y < 6 && game_table[y][opt] == 0)
                    {
                        game_table[y][opt] = '#';
                        draw(game_table);
                        game_table[y][opt] = 0;
                        y++;
                        this_thread::sleep_for(300ms);
                    }
                    game_table[y - 1][opt] = '#';
                    draw(game_table);
                    // Check
                    win = check_table(game_table);
                    if (win == 1)
                    {
                        cout << "Connect 4 \'#\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU WIN!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_win++;
                        send_data(31, "");
                        break;
                    }
                    else if (win == 2)
                    {
                        cout << "Connect 4 \'X\' !!!" << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "YOU LOSE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        user_lose++;
                        send_data(32, "");
                        break;
                    }
                    // round end
                    round += 2;
                    if (round == 42)
                    {
                        cout << "Both players didn\'t win this game." << '\n';
                        this_thread::sleep_for(1000ms);
                        cout << "It\'s TIE!" << '\n';
                        this_thread::sleep_for(1500ms);
                        break;
                    }
                }
                socket2.close();
            }
        }
        else if (ui_stat == 3) // Logout UI
        {
            cout << "Logging out..." << '\n';
            logout = 1;
            send_data(10, "");
            while (1)
            {
                this_thread::sleep_for(100ms);
                unique_lock<std::mutex> lk(queue_locker);
                if (!fetched_data.empty())
                    break;
            }
            socket1.close();
            pingpong.join();
            cout << "Bye Bye!" << '\n';
            return 0;
        }
    }
    return 0;
}
/*
_______
|0|0|0|
-------
|0|#|0|
-------
|X|#|0|
-------
*/
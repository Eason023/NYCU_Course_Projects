Eason's Game Store system - Server
---
#### To get start:

  Download the game store server by the following command:
  > ```git clone ```

  In the downloaded folder, there are files as follow:
  ```
  Server
    |  README.md
    │  server
    └─ games
        ├─ EasonGS_API
        ├─ (GameXXX)
        ├─ (GameXXX)
        └─ (GameXXX)...
  ```
  Run this command to start your server, first command allow server to modify files:
  > ```chmod -R 755 Server/```
  > ```cd Server/```
  > ```g++ server.cpp -I asio -std=c++20 -o server```
  > ```.\server```
  
  Games will be store in `games/`. Whenever a game is uploaded, server will compile the server.cpp in the `games/GameXXX/tmp/`.
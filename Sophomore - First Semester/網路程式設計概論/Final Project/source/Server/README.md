Eason's Game Store system - Server
---
### To get started:

  Download the game store server by the following commands:
  ``` bash
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set --prefix=Server "Sophomore - First Semester/網路程式設計概論/Final Project/source/Server"
  git checkout
  ```

  In the downloaded folder, there are files as follow:
  ```
  Server
    |  README.md
    │  server
    ├─ subprocess
    ├─ asio
    └─ games
        ├─ EasonGS_API
        ├─ (GameXXX)
        ├─ (GameXXX)
        └─ (GameXXX)...
  ```
  Run these commands to start your server, first command allow server to modify files:
  ```bash
  chmod -R 755 Server/
  cd Server/
  g++ server.cpp -I asio -std=c++20 -o server
  ./server
  ```
  
  Games will be store in `games/`. Whenever a game is uploaded, server will compile the server.cpp in the `games/GameXXX/tmp/`.

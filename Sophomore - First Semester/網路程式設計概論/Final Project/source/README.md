Eason's Game Store system
---

This code provides the basic programs for game players, developers and game server. The default server is running at: `140.113.17.11:52023`. You can modify these by [***building from source***](#build-from-source). The connection information defined as constants.

### Quick Start
**To get started with our system, you should download the source code corresponding to your role:**

- #### [Player for Windows](./Player)
  Download the player client by the following commands:
  ``` bash
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set --prefix=Player "Sophomore - First Semester/網路程式設計概論/Final Project/source/Player"
  git checkout
  mv "Sophomore - First Semester/網路程式設計概論/Final Project/source/Player" ./
  rm -rf "Sophomore - First Semester"
  ```

  In the downloaded folder, there are files as follow:
  ```
  Player
    |  README.md
    │  GameStore.exe
    ├─ fonts
    └─ downloads
  ```
  Double click `GameStore.exe` to run the GameStore.

- #### [Game Developer for Windows](./Developer)
  Download the developer client by the following commands:
  ``` bash
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set --prefix=Developer "Sophomore - First Semester/網路程式設計概論/Final Project/source/Developer"
  git checkout
  mv "Sophomore - First Semester/網路程式設計概論/Final Project/source/Developer" ./
  rm -rf "Sophomore - First Semester"
  ```

  In the downloaded folder, there are files as follow:
  ```
  Developer
    |  README.md
    │  DevGameStore.exe
    ├─ fonts
    ├─ Games_in_development
    │   ├─ EasonGS_API
    │   │   │ README.md
    │   │   │ EasonGS_Client.cpp
    │   │   │ EasonGS_Client.hpp
    │   │   │ EasonGS_Server.cpp
    │   │   │ EasonGS_Server.hpp
    │   │   └─asio
    │   ├─ glfw
    │   └─ imgui
    └─ DevGameTemplate
         templateClient.cpp
         templateServer.cpp
  ```
  Double click `DevGameStore.exe` to run the client for developer.

  You can design your game server and game client following to our [***APIs***](./Developer/Games_in_development/EasonGS_API). While developing the game server and client, please follow the instruction in the documents.

- #### [Server](./Server)
  Download the game store server by the following commands:
  ``` bash
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set --prefix=Server "Sophomore - First Semester/網路程式設計概論/Final Project/source/Server"
  git checkout
  mv "Sophomore - First Semester/網路程式設計概論/Final Project/source/Server" ./
  rm -rf "Sophomore - First Semester"
  ```

  In the downloaded folder, there are files as follow:
  ```
  Server
    |  README.md
    │  server.cpp
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

### Note
  1. This system is designed for Windows client. However, it is possible to use different ImGUI backend for other OS.
  2. To replace the backend of the GameStore or DevGameStore, you should try to [***build from source***](#build-from-source).
  3. The server and client do not have any encryption or security connection, you can add it on your own by [***building from source***](#build-from-source)
  4. **Be patient, don't exit the program while there are any game or downloading/uploading process is in progress.**

### Build from source
This project provide the very basic codebase for server, player, and developer to communicate throught TCP. It cannot be deployed for a real use case without security and data encryption in place.

However, you can add these capabilities by **building from the source code**. Here's how to build the client and server:

1. Download source code by the following commands:
    ``` bash
    git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
    cd NYCU_Course_Projects
    git sparse-checkout init --cone
    git sparse-checkout set --prefix=source "Sophomore - First Semester/網路程式設計概論/Final Project/source"
    git checkout
    mv "Sophomore - First Semester/網路程式設計概論/Final Project/sourcer" ./
    rm -rf "Sophomore - First Semester"
    cd source/
    ```
2. Build server: (Linux)
    ``` bash
    g++ server.cpp -I asio -std=c++20 -o server
    ```
3. Build Player client: (Windows)
    ``` bash
    g++ GameStore.cpp imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I asio -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -std=c++20 -lws2_32 -lmswsock -lopengl32 -lgdi32 -o GameStore.exe -mwindows
    ```
4. Build Developer client: (Windows)
    ``` bash
    g++ DevGameStore.cpp imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I asio -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -std=c++20 -lws2_32 -lmswsock -lopengl32 -lgdi32 -o DevGameStore.exe -mwindows
    ```

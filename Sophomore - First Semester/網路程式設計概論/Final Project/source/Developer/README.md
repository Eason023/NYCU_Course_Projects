Eason's Game Store system - Developer
---
### To get started:

  Download the developer client by the following commands:
  ``` cmd
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set "Sophomore - First Semester/網路程式設計概論/Final Project/source/Developer"
  git checkout
  move "Sophomore - First Semester/網路程式設計概論/Final Project/source/Developer" ./
  rd /s /q "Sophomore - First Semester"
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
    ├─ Examples
    └─ DevGameTemplate
         templateClient.cpp
         templateServer.cpp
  ```
  Double click `DevGameStore.exe` to run the client for developer.

  You can design your game server and game client following to our [***APIs***](./Games_in_development/EasonGS_API). While developing the game server and client, please follow the instruction in the documents.

  #### Usage Notes
  1. You have to take down the game from the store before updating your game.
  2. For uploading a game, the file path is relative path to your DevGameStore.exe.

  #### Development Notes
  1. Try statically compiling the player.exe to avoid dll file error.
  2. For GUI games, we use Dear ImGui by default. Note: set `io.IniFilename = nullptr;` in your GUI game to prevent the use of an `.ini` file.

### Examples:
Let's use [***GuessNumber***](./Examples/GuessNumber-CLI_Game) (Multi-player CLI game), [***TicTacToe***](./Examples/TicTacToe-GUI_Game) (Two-player GUI game), and [***CoinTake***](./Examples/CoinTake-GUI_Game) (Multi-player GUI game) as examples, Assume that the corresponding `.cpp` files are located in the directory `\Developer\Games_in_development\`, which is also the current working directory (CWD).

  1. To build the GuessNumber CLI game client on Windows:
     ``` bash
     g++ "GuessNumberClient.cpp" "EasonGS_API/EasonGS_Client.cpp" -I EasonGS_API/asio -I EasonGS_API -std=c++20 -static -static-libgcc -static-libstdc++ -o "GuessNumberClient.exe" -lws2_32 -lwinpthread
     ```

  2. To build the TicTacToe GUI game client on Windows:
      ``` bash
      g++ "TicTacToeClient.cpp" "EasonGS_API/EasonGS_Client.cpp" imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I EasonGS_API/asio -I EasonGS_API -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -lopengl32 -lgdi32 -lws2_32 -lmswsock -std=c++20 -o "TicTacToeClient.exe" -mwindows
      ```

  3. To build the CoinTake GUI game client on Windows:
      ``` bash
     g++ "CoinTakeClient.cpp" "EasonGS_API/EasonGS_Client.cpp" imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I EasonGS_API/asio -I EasonGS_API -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -lopengl32 -lgdi32 -lws2_32 -lmswsock -std=c++20 -o "CoinTakeClient.exe" -mwindows
      ```

The server will compile the `server.cpp` using `-I EasonGS_API/asio -I EasonGS_API -std=c++20`. Please refer to our [***template***](./DevGameTemplate) design file for `server.cpp`.

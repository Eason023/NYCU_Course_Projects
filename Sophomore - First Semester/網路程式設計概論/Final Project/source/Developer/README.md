Eason's Game Store system - Developer
---
#### To get start:

  Download the developer client by the following command:
  > ```git clone ```
  
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

  You can design your game server and game client following to our [***APIs***](). While developing the game server and client, please follow the instruction in the documents.

  ###### You have to take down the game from the store before updating your game.
  ###### For uploading a game, the file path is relative path to your DevGameStore.exe.
  ###### Try static compiling the player.exe to avoid dll file error.

  ###### For GUI game, we use Dear ImGui by default. Note: set `io.IniFilename = nullptr;` in the GUI game to avoid using .ini file.

#### Example
Let's use [***GuessNumber***]() (Multi-player CLI game), [***TicTacToe***]() (Two-player GUI game), and [***CoinTake***]() (Multi-player GUI game) as example, assume the cpp file in the directory `\Developer\Games_in_development\`. (So does CWD)

  1. To build the GuessNumber CLI game client.exe on Windows:
     > ```g++ "GuessNumberClient.cpp" "EasonGS_API/EasonGS_Client.cpp" -I EasonGS_API/asio -I EasonGS_API -std=c++20 -static -static-libgcc -static-libstdc++ -o "GuessNumberClient.exe" -lws2_32 -lwinpthread```

  2. To build the TicTacToe GUI game client.exe on Windows:
      > ```g++ "TicTacToeClient.cpp" "EasonGS_API/EasonGS_Client.cpp" imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I EasonGS_API/asio -I EasonGS_API -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -lopengl32 -lgdi32 -lws2_32 -lmswsock -std=c++20 -o "TicTacToeClient.exe" -mwindows```

  3. To build the CoinTake GUI game client.exe on Windows:
      > ```g++ "CoinTakeClient.cpp" "EasonGS_API/EasonGS_Client.cpp" imgui/imgui.cpp imgui/imgui_stdlib.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp -I EasonGS_API/asio -I EasonGS_API -I imgui -I glfw/include glfw/lib-mingw-w64/libglfw3.a -Lglfw/lib-mingw-w64 -lopengl32 -lgdi32 -lws2_32 -lmswsock -std=c++20 -o "CoinTakeClient.exe" -mwindows```

The server will compile the server.cpp using -I EasonGS_API/asio -I EasonGS_API -std=c++20. Please refer to our template design file for server.cpp.
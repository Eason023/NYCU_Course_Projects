EasonGS API Documentation
---
EasonGS is a lightweight C++ networking SDK that facilitates communication between your game logic and the GameStore platform. It abstracts the complex networking required to tunnel traffic through the GameStore client to the central server, allowing you to focus on gameplay logic.

The API is divided into two distinct modules:

  1. **Client-Side API (EasonGS_Client):**
     Used in your game executable (Player.exe) to communicate with the local GameStore proxy.
  2. **Server-Side API (EasonGS_Server):**
     Used in your game server logic (Server.cpp) to handle room management and player messages.

You have to implement each function in the `.hpp` interfaces. This api use asynchronous io structure, **do not process data sequentially for the interface function**.

Since the client-server connectivity and data routing via the GameStore client and API are stable, we can **shift our focus entirely to game design**. Networking is now largely abstracted by the API.

For client side, please make sure all the UIs are in the main thread. **This api is not thread safe.**

**Please design your program refer to the provided template.**
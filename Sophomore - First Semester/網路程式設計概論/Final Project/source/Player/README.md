Eason's Game Store system - Player
---
### To get started:

  Download the player client by the following command:
  ``` cmd
  git clone --no-checkout https://github.com/Eason023/NYCU_Course_Projects.git
  cd NYCU_Course_Projects
  git sparse-checkout init --cone
  git sparse-checkout set "Sophomore - First Semester/網路程式設計概論/Final Project/source/Player"
  git checkout
  cd ..
  move "NYCU_Course_Projects/Sophomore - First Semester/網路程式設計概論/Final Project/source/Player" ./
  rd /s /q "NYCU_Course_Projects"
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

  #### Note: Always close the game process before the GameStore process.

  All the user downloaded games will be store in `downloads/user/`. Thus, multiple users have their own individual files.

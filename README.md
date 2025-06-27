
# Capybara

A CPP game made using native POSIX sockets.

## Gameplay

### Start
On starting the game, it will ask you for your name and color. Use the keyboard to enter name and arrows to select color. You can press ";" to save your settings.
Press enter to start.

### Movement
Use WASD to move around.

### Shooting
Click in the direction you want to shoot to fire a bullet. You can use the minimap to see player locations.

## Setup

1. Clone the repo
   ```sh
   git clone https://github.com/AndrewGalvez/capybara
   ```

2. Compile
   ```sh
   # server
   g++ -o bin/server src/server.cpp -lraylib

   # client
   g++ -o bin/client src/client.cpp -lraylib
   ```

### Server
```sh
bin/server
```

### Client
```sh
# to connect to localhost:50000
bin/client

# to connect to 192.168.68.68:50000
bin/client "192.168.68.68"

# on windows powershell
./game.exe "192.168.68.68"
```


# Capybara

A CPP game made using native POSIX sockets.

## Gameplay

### Start
On starting the game, it will ask you for your name and color. Use the keyboard to enter name and arrows to select color. You can press ";" to save your settings.
Press enter to start.

### Movement
Use WASD to move around.

### Shooting
Click in the direction you want to shoot to shoot. This will fire a bullet.

## Setup

1. Clone the repo
   ```sh
   git clone https://github.com/AndrewGalvez/capybara
   ```

2. Compile
   ```sh
   # server
   g++ -o bin/server src/server.cpp

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
```

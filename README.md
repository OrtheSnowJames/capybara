
# Capybara

A CPP game made using native POSIX sockets.

## Gameplay

### Start
On starting the game, it will ask you for your name and color. Use the keyboard to enter name and arrows to select color.
Press enter to start.

### Movement
Use WASD to move around.

### Shooting
(not implemented yet)
Click in the direction you want to shoot to shoot. This will fire a bullet.

## Setup

1. Clone the repo
   ```sh
   git clone https://github.com/AndrewGalvez/capybara
   ```

2. Compile
   ```sh
   # server
   g++ -o server server.cpp

   # client
   g++ -o client client.cpp -lraylib
   ```

### Server
```sh
./server
```

### Client
```sh
# to connect to localhost:50000
./client
```

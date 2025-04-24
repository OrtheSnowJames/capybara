#include "player.hpp"
#include "raylib.h"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <list>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

int sock = socket(AF_INET, SOCK_STREAM, 0);

std::mutex packets_mutex;
std::list<std::string> packets = {};

std::atomic<bool> running = true;

void do_recv() {
  char buffer[1024];

  while (running) {
    int bytes = recv(sock, &buffer, sizeof(buffer), 0);

    if (bytes == 0) {
      std::cout << "Server disconnected.\n";
      running = false;
      break;
    } else if (bytes < 0) {
      perror("Error receiving packet");
      running = false;
      break;
    }

    std::lock_guard<std::mutex> lock(packets_mutex);
    std::string packet(buffer, bytes);

    packets.push_back(packet);
  }
}

int main() {
  sockaddr_in sock_addr;
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(50000);
  sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("Could not connect to server");
    close(sock);
    return -1;
  }

  std::thread recv_thread(do_recv);

  InitWindow(800, 600, "Multipalyer Gmae");

  SetTargetFPS(60);

  std::unordered_map<int, Player> players;
  int my_id;

  while (!WindowShouldClose() && running) {
    std::lock_guard<std::mutex> lock(packets_mutex);
    while (!packets.empty()) {
      std::string packet = packets.front();

      int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
      std::string payload = packet.substr(packet.find('\n') + 1);

      std::cout << "Pkt type: " << packet_type << std::endl;
      std::cout << "Payload: " << payload << std::endl;
      std::istringstream in(payload);

      switch (packet_type) {
      case 0:
        std::cout << "case 0" << std::endl;
        int id, x, y;
        in >> id >> x >> y;

        players.insert({id, Player(x, y)});
        std::cout << "Inserted player\n";

        break;
      case 1:
        std::cout << "case 1" << std::endl;
        in >> my_id;

        std::cout << "my id: " << my_id << std::endl;
        break;
      }

      packets.pop_front();
    }

    if (IsKeyDown(KEY_W)) {
      std::cout << "moving up\n";
      players.at(my_id).y -= 5;
      std::string msg("1\n");
      msg.append(std::to_string(players.at(my_id).x));
      msg.append(" ");
      msg.append(std::to_string(players.at(my_id).y));

      const char *msg_str(msg.c_str());

      send(sock, msg_str, msg.size(), 0);
    }

    BeginDrawing();

    ClearBackground(WHITE);

    for (auto &[id, p] : players) {
      Color clr = BLACK;
      if (id == my_id)
        clr = RED;
      DrawRectangle(p.x, p.y, 100, 100, clr);
    }

    EndDrawing();
  }

  CloseWindow();

  running = false;

  shutdown(sock, SHUT_RDWR);

  close(sock);

  recv_thread.join();
}

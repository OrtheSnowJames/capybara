#include "player.hpp"
#include "raylib.h"
#include "utils.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

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

void handle_packet(int packet_type, std::string payload,
                   std::map<int, Player> *players, int *my_id) {
  std::istringstream in(payload);
  std::vector<std::string> msg_split;

  switch (packet_type) {
  case 0:
    split(payload, std::string(":"), msg_split);
    for (std::string i : msg_split) {
      if (i == std::string(""))
        continue;

      std::istringstream j(i);
      int id, x, y;
      std::string username;
      j >> id >> x >> y >> username;
      (*players)[id] = Player(x, y);
      (*players)[id].username = username;
    }

    break;
  case 1:
    in >> *my_id;
    break;
  case 2: {
    std::istringstream i(payload);
    int id, x, y;
    i >> id >> x >> y;

    if ((*players).find(id) != (*players).end()) {
      (*players).at(id).x = x;
      (*players).at(id).y = y;
    }

  } break;
  case 3:
    split(payload, " ", msg_split);
    {
      int id = std::stoi(msg_split.at(0));
      int x = std::stoi(msg_split.at(1));
      int y = std::stoi(msg_split.at(2));
      std::string username = msg_split[3];

      (*players)[id] = Player(x, y);
      (*players)[id].username = username;
    }

    break;
  case 4:
    if ((*players).find(std::stoi(payload)) != (*players).end())
      (*players).erase(std::stoi(payload));
    break;

  case 5: {
    split(payload, std::string(" "), msg_split);
    (*players).at(std::stoi(msg_split[0])).username = msg_split[1];
  } break;
  }
}

void handle_packets(std::map<int, Player> *players, int *my_id) {
  std::lock_guard<std::mutex> lock(packets_mutex);
  while (!packets.empty()) {
    std::string packet = packets.front();

    int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
    std::string payload = packet.substr(packet.find('\n') + 1);

    handle_packet(packet_type, payload, players, my_id);

    packets.pop_front();
  }
}

void do_username_prompt(std::string *usernameprompt, bool *usernamechosen,
                        std::map<int, Player> *players, int my_id) {
  BeginDrawing();
  ClearBackground(BLACK);
  DrawText("Pick a username", 50, 50, 64, WHITE);

  DrawText(std::to_string(10 - (*usernameprompt).length())
               .append(" characters left.")
               .c_str(),
           50, 290, 32, (*usernameprompt).length() < 10 ? WHITE : RED);
  DrawRectangle(50, 200, 500, 75, RED);

  DrawText((*usernameprompt).c_str(), 75, 225, 48, BLACK);

  char k = GetCharPressed();

  if (k != 0 && k != '\n' && k != ':' && k != ' ' &&
      (*usernameprompt).length() < 10) {
    (*usernameprompt).push_back(k);
  }
  if (IsKeyPressed(KEY_BACKSPACE) && !(*usernameprompt).empty())
    (*usernameprompt).pop_back();

  if (IsKeyPressed(KEY_ENTER) && !(*usernameprompt).empty() && my_id != -1) {
    (*players)[my_id].username = *usernameprompt;
    *usernamechosen = true;
    send_message(std::string("5\n").append(*usernameprompt), sock);
  }

  EndDrawing();
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

  InitWindow(800, 600, "Multi Ludens");

  SetTargetFPS(60);

  std::map<int, Player> players;
  int my_id = -1;
  int server_update_counter = 0;
  bool hasmoved = false;
  bool usernamechosen = false;
  std::string usernameprompt;

  while (!WindowShouldClose() && running) {
    handle_packets(&players, &my_id);

    if (my_id == -1) {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawText("waiting for server...", 50, 50, 48, GREEN);
      EndDrawing();
      continue;
    }

    if (!usernamechosen) {
      do_username_prompt(&usernameprompt, &usernamechosen, &players, my_id);
      continue;
    }

    server_update_counter++;

    hasmoved = players.at(my_id).move();

    if (server_update_counter >= 5 && hasmoved) {
      std::string msg =
          std::string("2\n" + std::to_string(players.at(my_id).x) + " " +
                      std::to_string(players.at(my_id).y));
      send_message(msg, sock);

      server_update_counter = 0;
    }

    // ------------------------------------------------------------------------------

    float dt = GetFrameTime();

    BeginDrawing();

    ClearBackground(WHITE);

    DrawFPS(2, 2);

    for (auto &[id, p] : players) {
      if (p.username == "unset")
        continue;
      Color clr = BLACK;
      if (id == my_id)
        clr = RED;
      DrawRectangle(p.x, p.y, 100, 100, clr);
      DrawRectangle(p.x + 45 - MeasureText(p.username.c_str(), 32) / 2,
                    p.y - 55, MeasureText(p.username.c_str(), 32) + 10, 42,
                    BLACK);
      DrawText(p.username.c_str(),
               p.x + 50 - MeasureText(p.username.c_str(), 32) / 2, p.y - 50, 32,
               GREEN);
    }

    DrawRectangle(0, 500, 300, 100, GREEN);
    DrawRectangle(10, 510, 80, 80, RED);
    DrawText(players[my_id].username.c_str(), 100, 515, 24, BLACK);

    EndDrawing();
  }

  std::cout << "Closing.\n";

  running = false;

  shutdown(sock, SHUT_RDWR);
  close(sock);

  recv_thread.join();

  CloseWindow();
}

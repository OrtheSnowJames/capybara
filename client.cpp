#include "bullet.hpp"
#include "math.h"
#include "player.hpp"
#include "raylib.h"
#include "raymath.h"
#include "utils.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <cstring>
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

Color stc(std::string s) {
  Color output;
  const char *sc = s.c_str();
  if (strcmp(sc, "RED") == 0)
    output = RED;
  else if (strcmp(sc, "GREEN") == 0)
    output = GREEN;
  else if (strcmp(sc, "YELLOW") == 0)
    output = YELLOW;
  else if (strcmp(sc, "PURPLE") == 0)
    output = PURPLE;
  else if (strcmp(sc, "ORANGE") == 0)
    output = ORANGE;

  return output;
}

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
      std::string username, clr;
      j >> id >> x >> y >> username >> clr;
      (*players)[id] = Player(x, y);
      (*players)[id].username = username;
      std::cout << clr;
      (*players)[id].color = stc(clr);
    }
    break;
  case 1:
    in >> *my_id;
    break;
  case 2: {
    std::istringstream i(payload);
    int id, x, y;
    float rot;
    i >> id >> x >> y >> rot;

    if ((*players).find(id) != (*players).end()) {
      (*players).at(id).nx = x;
      (*players).at(id).ny = y;
      (*players).at(id).rot = rot;
    }

  } break;
  case 3:
    split(payload, " ", msg_split);
    {
      int id = std::stoi(msg_split.at(0));
      int x = std::stoi(msg_split.at(1));
      int y = std::stoi(msg_split.at(2));
      std::string username = msg_split.at(3);

      (*players)[id] = Player(x, y);
      (*players)[id].username = username;
      (*players)[id].color = Color{
          (unsigned char)std::stoi(std::string(msg_split[4])),
          (unsigned char)std::stoi(msg_split[5]),
          (unsigned char)std::stoi(msg_split[6]),
          (unsigned char)std::stoi(msg_split[7]),
      };
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
  case 6: {
    split(payload, std::string(" "), msg_split);
    std::cout << payload << '\n';
    std::cout << "6\n";
    if (players->find(std::stoi(msg_split[0])) != players->end()) {
      (*players).at(std::stoi(msg_split[0])).color = Color{
          (unsigned char)std::stoi(msg_split[1]),
          (unsigned char)std::stoi(msg_split[2]),
          (unsigned char)std::stoi(msg_split[3]),
          (unsigned char)std::stoi(msg_split[4]),
      };
    }
  } break;
  }
}

void handle_packets(std::map<int, Player> *players, int *my_id) {
  std::lock_guard<std::mutex> lock(packets_mutex);
  while (!packets.empty()) {
    std::string packet = packets.front();

    int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
    std::string payload =
        packet.substr(packet.find('\n') + 1, packet.find_first_of(';') - 2);

    handle_packet(packet_type, payload, players, my_id);

    packets.pop_front();
  }
}

void do_username_prompt(std::string *usernameprompt, bool *usernamechosen,
                        std::map<int, Player> *players, int my_id, int *mycolor,
                        Color options[5]) {
  BeginDrawing();
  ClearBackground(BLACK);
  DrawText("Pick a username", 50, 50, 64, WHITE);

  DrawText(std::to_string(10 - (*usernameprompt).length())
               .append(" characters left.")
               .c_str(),
           50, 215, 32, (*usernameprompt).length() < 10 ? WHITE : RED);
  DrawRectangle(50, 125, 500, 75, BLUE);

  DrawText((*usernameprompt).c_str(), 75, 150, 48, BLACK);

  char k = GetCharPressed();

  if (k != 0 && k != '\n' && k != ':' && k != ' ' && k != ';' &&
      (*usernameprompt).length() < 10) {
    (*usernameprompt).push_back(k);
  }
  if (IsKeyPressed(KEY_BACKSPACE) && !(*usernameprompt).empty())
    (*usernameprompt).pop_back();

  if (IsKeyPressed(KEY_ENTER) && !(*usernameprompt).empty() && my_id != -1) {
    (*players)[my_id].username = *usernameprompt;
    *usernamechosen = true;
    send_message(std::string("5\n").append(*usernameprompt), sock);
    send_message("6\n" + std::to_string(options[*mycolor].r) + ' ' +
                     std::to_string(options[*mycolor].g) + ' ' +
                     std::to_string(options[*mycolor].b) + ' ' +
                     std::to_string(options[*mycolor].a),
                 sock);
  }

  // Color Pick
  DrawText("Choose color:", 50, 300, 32, WHITE);

  int x = 50;

  for (int i = 0; i < 6; i++) {
    Color cc = options[i];
    if (cc.r == options[*mycolor].r && cc.g == options[*mycolor].g &&
        cc.b == options[*mycolor].b && cc.a == options[*mycolor].a) {
      DrawRectangle(x - 10, 340, 70, 70, WHITE);
    }
    DrawRectangle(x, 350, 50, 50, cc);

    x += 100;
  }

  if (IsKeyPressed(KEY_LEFT))
    (*mycolor)--;
  if (IsKeyPressed(KEY_RIGHT))
    (*mycolor)++;
  if (*mycolor < 0)
    *mycolor = 4;
  if (*mycolor > 4)
    *mycolor = 0;

  EndDrawing();
}

void draw_ui(Color mycolor, std::map<int, Player> players, int my_id, int bd) {
  DrawRectangle(0, 500, 300, 100, DARKBLUE);
  DrawRectangle(10, 510, 80, 80, mycolor);
  DrawText(players[my_id].username.c_str(), 100, 515, 24, BLACK);
  if (bd != 0)
    DrawRectangle(100, 540, bd * 2, 10, GREEN);
}

void draw_players(std::map<int, Player> players,
                  std::map<Color, Texture2D, ColorCompare> player_textures) {
  for (auto &[id, p] : players) {
    if (p.username == "unset")
      continue;
    Color clr = p.color;
    if (player_textures.find(clr) != player_textures.end())
      DrawTexture(player_textures[clr], p.x, p.y, WHITE);

    DrawText(p.username.c_str(),
             p.x + 50 - MeasureText(p.username.c_str(), 32) / 2, p.y - 50, 32,
             BLACK);

    DrawRectanglePro({(float)p.x + 50, (float)p.y + 50, 50, 20},
                     {(float)100, (float)0}, p.rot, BLACK);
  }
}

bool move_gun(float *rot, int cx, int cy) {
  Vector2 delta = Vector2Subtract((Vector2){400, 300}, GetMousePosition());
  float angle = atan2f(delta.y, delta.x) * RAD2DEG;
  float oldrot = *rot;
  *rot = fmod(angle + 360.0f, 360.0f);

  return *rot == oldrot;
}

void move_players(std::map<int, Player> *players, int skip) {
  for (auto &[k, v] : *players) {
    if (k == skip)
      continue;
    if (v.x != v.nx) {
      if (v.x < v.nx)
        v.x += v.speed;
      if (v.x > v.nx)
        v.x -= v.speed;
    }
    if (v.y != v.ny) {
      if (v.y < v.ny)
        v.y += v.speed;
      if (v.y > v.ny)
        v.y -= v.speed;
    }
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

  InitWindow(800, 600, "Multi Ludens");

  SetTargetFPS(60);

  Image floorImage = LoadImage("floor_tile.png");
  ImageResizeNN(&floorImage, 100, 100);
  Texture2D floorTexture = LoadTextureFromImage(floorImage);
  std::map<int, Player> players;
  std::vector<Bullet> bullets;

  std::map<Color, Texture2D, ColorCompare> player_textures;
  player_textures[RED] = LoadTexture("player_red.png");
  player_textures[GREEN] = LoadTexture("player_green.png");
  player_textures[YELLOW] = LoadTexture("player_yellow.png");
  player_textures[PURPLE] = LoadTexture("player_purple.png");
  player_textures[ORANGE] = LoadTexture("player_orangle.png");

  for (auto &[_, v] : player_textures) {
    Image a = LoadImageFromTexture(v);
    ImageResizeNN(&a, 100, 100);
    v = LoadTextureFromImage(a);
  }

  int my_id = -1;
  int server_update_counter = 0;
  bool hasmoved = false;
  bool usernamechosen = false;
  std::string usernameprompt;
  Color mycolor = BLACK;
  int colorindex = 0;
  int bdelay = 20;
  int canshoot = false;

  Color options[5] = {RED, GREEN, YELLOW, PURPLE, ORANGE};

  Camera2D cam;
  cam.zoom = 1.0f;
  cam.rotation = 0.0f;
  cam.offset = {0.0f, 0.0f};

  while (!WindowShouldClose() && running) {
    int cx = players[my_id].x;
    int cy = players[my_id].y;

    cam.target = {(float)cx - 350, (float)cy - 250};

    handle_packets(&players, &my_id);

    if (my_id == -1) {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawText("waiting for server...", 50, 50, 48, GREEN);
      if (!players.empty())
        DrawText("loading...", 50, 100, 32, GREEN);
      else if (rand() % 2 == 1)
        DrawText("loading player data...", 50, 100, 32, GREEN);
      EndDrawing();
      continue;
    }

    if (!usernamechosen) {
      do_username_prompt(&usernameprompt, &usernamechosen, &players, my_id,
                         &colorindex, options);
      continue;
    }

    if (mycolor.r == 0) {
      mycolor = options[colorindex];
      players[my_id].color = mycolor;
    }

    server_update_counter++;

    bool moved = players.at(my_id).move();
    bool moved_gun = move_gun(&players[my_id].rot, cx, cy);

    hasmoved = moved || moved_gun;

    if (server_update_counter >= 5 && hasmoved) {
      std::string msg =
          std::string("2\n" + std::to_string(players.at(my_id).x) + " " +
                      std::to_string(players.at(my_id).y) + " " +
                      std::to_string(players.at(my_id).rot));
      send_message(msg, sock);

      server_update_counter = 0;
    }

    move_players(&players, my_id);

    for (Bullet &b : bullets)
      b.move();

    if (!canshoot)
      bdelay--;
    if (!canshoot && bdelay == 0) {
      canshoot = true;
    }

    if (IsMouseButtonDown(0) && canshoot) {
      canshoot = false;
      bdelay = 20;
      float bspeed = 10;
      float angleRad = (-players[my_id].rot + 5) * DEG2RAD;
      Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);
      Vector2 spawnOffset =
          Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -120);
      Vector2 origin = {(float)players[my_id].x + 50,
                        (float)players[my_id].y + 50};
      Vector2 spawnPos = Vector2Add(origin, spawnOffset);
      bullets.push_back(Bullet(spawnPos.x, spawnPos.y, dir));

      send_message(
          std::string("10\n ").append(std::to_string(players[my_id].rot)),
          sock);
    }

    // ------------------------------------------------------------------------------

    float dt = GetFrameTime();

    cam.target = {(float)players[my_id].x - 350, (float)players[my_id].y - 250};

    BeginDrawing();

    ClearBackground(BLUE);

    DrawFPS(2, 2);

    BeginMode2D(cam);

    for (int i = 0; i < 200; i++) {
      for (int j = 0; j < 200; j++) {
        DrawTexture(floorTexture, i * 100, j * 100, WHITE);
      }
    }

    draw_players(players, player_textures);

    for (Bullet &b : bullets)
      b.show();

    EndMode2D();

    draw_ui(mycolor, players, my_id, bdelay);

    EndDrawing();
  }

  std::cout << "Closing.\n";

  running = false;

  shutdown(sock, SHUT_RDWR);
  close(sock);

  recv_thread.join();

  CloseWindow();
}

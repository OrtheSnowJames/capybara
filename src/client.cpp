#include "bullet.hpp"
#include "codes.hpp"
#include "constants.hpp"
#include "drawScale.hpp"
#include "game.hpp"
#include "game_config.hpp"
#include "math.h"
#include "player.hpp"
#include "raylib.h"
#include "raymath.h"
#include "resource_manager.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <ostream>
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
static std::string network_buffer = "";
static Color my_true_color = RED;

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

    network_buffer.append(buffer, bytes);

    size_t separator_pos;
    while ((separator_pos = network_buffer.find(';')) != std::string::npos) {
      std::string packet = network_buffer.substr(0, separator_pos);
      network_buffer.erase(0, separator_pos + 1);

      if (!packet.empty()) {
        std::lock_guard<std::mutex> lock(packets_mutex);
        packets.push_back(packet);
      }
    }
  }
}

void handle_packet(int packet_type, std::string payload, Game *game,
                   int *my_id) {
  std::istringstream in(payload);
  std::vector<std::string> msg_split;

  switch (packet_type) {
  case MSG_GAME_STATE:
    std::cout << "Received game state: " << payload << std::endl;
    // remove trailing semicolon if present
    if (!payload.empty() && payload.back() == ';') {
      payload.pop_back();
    }
    split(payload, std::string(":"), msg_split);
    for (std::string i : msg_split) {
      if (i == std::string(""))
        continue;

      // split each player entry by spaces
      std::vector<std::string> player_parts;
      split(i, std::string(" "), player_parts);

      if (player_parts.size() != 6) {
        std::cerr << "Invalid player data: " << i << std::endl;
        continue;
      }

      try {
        int id = std::stoi(player_parts[0]);
        int x = std::stoi(player_parts[1]);
        int y = std::stoi(player_parts[2]);
        std::string username = player_parts[3];
        unsigned int color_code = (unsigned int)std::stoi(player_parts[4]);
        int weapon_id = std::stoi(player_parts[5]);

        (*game).players[id] = Player(x, y);
        (*game).players[id].username = username;
        (*game).players[id].color = uint_to_color(color_code);
        (*game).players[id].weapon_id = weapon_id;

        std::cout << "Player " << id << " color code: " << color_code
                  << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "Error parsing player data: " << i << " - " << e.what()
                  << std::endl;
      }
    }
    break;
  case MSG_CLIENT_ID:
    in >> *my_id;
    break;
  case MSG_PLAYER_MOVE: {
    std::istringstream i(payload);
    int id, x, y;
    float rot;
    i >> id >> x >> y >> rot;

    if (id == *my_id)
      break;

    if ((*game).players.find(id) != (*game).players.end()) {
      (*game).players.at(id).nx = x;
      (*game).players.at(id).ny = y;
      (*game).players.at(id).rot = rot;
    }

  } break;
  case MSG_PLAYER_NEW:
    std::cout << "Received player new: " << payload << std::endl;
    split(payload, " ", msg_split);
    if (msg_split.size() != 6) {
      std::cerr << "Invalid MSG_PLAYER_NEW packet: " << payload << std::endl;
      break;
    }
    {
      int id = std::stoi(msg_split.at(0));
      int x = std::stoi(msg_split.at(1));
      int y = std::stoi(msg_split.at(2));
      std::string username = msg_split.at(3);
      unsigned int color_code = (unsigned int)std::stoi(msg_split.at(4));
      int weapon_id = std::stoi(msg_split.at(5));

      (*game).players[id] = Player(x, y);
      (*game).players[id].username = username;
      (*game).players[id].color = uint_to_color(color_code);
      (*game).players[id].weapon_id = weapon_id;
    }
    break;
  case MSG_PLAYER_LEFT:
    if ((*game).players.find(std::stoi(payload)) != (*game).players.end())
      (*game).players.erase(std::stoi(payload));
    break;
  case MSG_PLAYER_UPDATE: {
    std::istringstream iss(payload);
    int id;
    std::string username;
    unsigned int color_code;
    iss >> id >> username >> color_code;

    if (iss.fail()) {
      std::cerr << "Invalid MSG_PLAYER_UPDATE packet: " << payload << std::endl;
      break;
    }

    if (game->players.count(id)) {
      game->players.at(id).username = username;
      Color new_color = uint_to_color(color_code);
      game->players.at(id).color = new_color;

      // Only update the "true" color if the change is for the local player
      // and the new color isn't INVISIBLE.
      if (id == *my_id && !color_equal(new_color, INVISIBLE)) {
        my_true_color = new_color;
      }
    }
  } break;
  case MSG_BULLET_SHOT: {
    std::istringstream j(payload);
    int from_id, x, y;
    float rot;

    j >> from_id >> x >> y >> rot;

    // if (from_id == *my_id)
    //   break;

    float angleRad = (-rot + 5) * DEG2RAD;
    float bspeed = 10;

    Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);

    std::cout << from_id << ' ' << x << ' ' << y << ' ' << ' ' << dir.x << ' '
              << dir.y << std::endl;

    (*game).bullets.push_back(Bullet(x, y, dir, from_id));

    std::cout << "added bullet at " << x << ' ' << y << " with dir " << dir.x
              << ' ' << dir.y << std::endl;

    break;
  }
  }
}

void handle_packets(Game *game, int *my_id) {
  std::lock_guard<std::mutex> lock(packets_mutex);
  while (!packets.empty()) {
    std::string packet = packets.front();
    packets.pop_front();

    size_t newline_pos = packet.find('\n');
    if (newline_pos == std::string::npos) {
      std::cerr << "Malformed packet (no newline): " << packet << std::endl;
      continue;
    }

    int packet_type = std::stoi(packet.substr(0, newline_pos));
    std::string payload = packet.substr(newline_pos + 1);

    handle_packet(packet_type, payload, game, my_id);
  }
}

bool acceptable_username(std::string username) {
  return username.length() > 0 && username.length() <= 10 &&
         std::all_of(username.begin(), username.end(), [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9');
         }); // only allow alphanumeric characters
}

void do_username_prompt(std::string *usernameprompt, bool *usernamechosen,
                        int my_id, int *mycolor, Color options[5],
                        GameConfig *game_conf,
                        int *settings_saved_indicator_timer) {

  BeginDrawing();
  ClearBackground(BLACK);

  BeginUiDrawing();
  DrawTextScaleCentered("Pick a username", 50, 50, 64, WHITE);

  DrawTextScaleCentered((std::to_string(10 - (*usernameprompt).length())
                             .append(" characters left."))
                            .c_str(),
                        50, 215, 32,
                        (*usernameprompt).length() < 10 ? WHITE : RED);
  DrawRectangleScaleCentered(50, 125, 500, 75, BLUE);

  DrawTextScaleCentered((*usernameprompt).c_str(), 75, 150, 48, BLACK);

  // Color Pick
  DrawTextScaleCentered("Choose color:", 50, 300, 32, WHITE);

  DrawTextScaleCentered(
      *settings_saved_indicator_timer == 0 ? "Press ';' to save settings"
                                           : "Settings saved successfully",
      50, 450, 32, *settings_saved_indicator_timer == 0 ? WHITE : GREEN);

  int x = 50;

  for (int i = 0; i < 5; i++) {
    Color cc = options[i];
    if (color_equal(cc, options[*mycolor])) {
      DrawSquareScaleCentered(x - 10, 340, 70, WHITE);
    }
    DrawSquareScaleCentered(x, 350, 50, cc);

    x += 100;
  }

  char k = GetCharPressed();

  if (*settings_saved_indicator_timer == 0 && k == ';') {
    game_conf->colorindex = *mycolor;
    game_conf->username = *usernameprompt;
    game_conf->save();

    *settings_saved_indicator_timer = 120;
  }

  EndUiDrawing();

  if (acceptable_username(std::string(1, k)) && (*usernameprompt).length() < 10) {
    (*usernameprompt).push_back(k);
  }
  if (IsKeyPressed(KEY_BACKSPACE) && !(*usernameprompt).empty())
    (*usernameprompt).pop_back();

  if (IsKeyPressed(KEY_ENTER) && !(*usernameprompt).empty() && my_id != -1) {
    if (acceptable_username(*usernameprompt)) {
      *usernamechosen = true;
      game_conf->username = *usernameprompt;
      game_conf->colorindex = *mycolor;
      my_true_color = options[*mycolor];

      std::string payload =
          *usernameprompt + " " + std::to_string(color_to_uint(options[*mycolor]));
      send_message(std::string("5\n").append(payload), sock);
    }
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

void manage_username_prompt(playermap *players, int my_id, Color options[5],
                            GameConfig *game_conf) {
  std::string usernameprompt = game_conf->username;
  bool usernamechosen = false;
  int clrindex = game_conf->colorindex;
  int settings_saved_indicator_timer = 0;

  while (!usernamechosen && running && !WindowShouldClose()) {
    if (settings_saved_indicator_timer != 0)
      settings_saved_indicator_timer--;
    do_username_prompt(&usernameprompt, &usernamechosen, my_id, &clrindex,
                       options, game_conf, &settings_saved_indicator_timer);
  }
}

void draw_ui(Color my_ui_color, playermap players,
             std::vector<Bullet> bullets, int my_id, int shoot_cooldown,
             Camera2D cam, float scale) {
  BeginUiDrawing();

  DrawFPS(0, 0);

  // Base dimensions for UI elements
  float boxHeight = 50;
  float padding = 5;
  float squareSize = 40;
  float textSize = 24;
  float textPadding = 50;
  float textWidth = MeasureText(players[my_id].username.c_str(), textSize);
  float boxWidth = textPadding + textWidth + (padding * 2);

  // fix name clipping out of box
  if (boxWidth < squareSize + padding * 2) {
    boxWidth = squareSize + padding * 2;
  }

  // position at bottom of render texture (not screen)
  float y = window_size.y - boxHeight;

  // background
  DrawRectangle(0, y, boxWidth, boxHeight, DARKGRAY);

  // player info
  DrawRectangle(padding, y + (boxHeight - squareSize) / 2, squareSize,
                squareSize, my_ui_color);
  DrawText(players[my_id].username.c_str(), textPadding,
           y + (boxHeight - textSize) / 2, textSize, WHITE);

  // cooldown bar
  if (shoot_cooldown != 0) {
    float barHeight = 10;
    float barY = y + boxHeight - padding - barHeight;
    DrawRectangle(textPadding, barY, shoot_cooldown * 2, barHeight, GREEN);
  }

  // minimap
  DrawRectangle(window_size.x - 100, 0, 100, 100, GRAY);

  for (auto &[id, p] : players) {
    if (id == my_id) {
      DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                    p.y / (PLAYING_AREA.height / 100), 10, 10, my_ui_color);
    } else if (!color_equal(p.color, INVISIBLE)) {
      DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                    p.y / (PLAYING_AREA.height / 100), 10, 10, p.color);
    }
  }

  for (Bullet b : bullets)
    DrawCircle(window_size.x - 100 + b.x / (PLAYING_AREA.width / 100),
               b.y / (PLAYING_AREA.height / 100),
               b.r / (PLAYING_AREA.width / 100), BLACK);

  EndUiDrawing();
}

void draw_players(playermap players, ResourceManager *res_man, int my_id) {
  for (auto &[id, p] : players) {
    if (p.username == "unset" || (color_equal(p.color, INVISIBLE) && id != my_id))
      continue;
    Color clr = p.color;
    if (id == my_id && color_equal(p.color, INVISIBLE)) {
      // draw with transparency
      DrawTextureAlpha(res_man->load_player_texture_from_color(my_true_color), p.x, p.y, 128);
    } else {
      DrawTexture(res_man->load_player_texture_from_color(clr), p.x, p.y, WHITE);
    }

    DrawText(p.username.c_str(),
             p.x + 50 - MeasureText(p.username.c_str(), 32) / 2, p.y - 50, 32,
             BLACK);

    DrawRectanglePro({(float)p.x + 50, (float)p.y + 50, 50, 20},
                     {(float)100, (float)0}, p.rot, BLACK);
  }
}

bool move_gun(float *rot, int cx, int cy, Camera2D cam, float scale,
              float offsetX, float offsetY) {
  // mouse position in window
  Vector2 windowMouse = GetMousePosition();

  // mouse position in render texture
  Vector2 renderMouse = {(windowMouse.x - offsetX) / scale,
                         (windowMouse.y - offsetY) / scale};

  // mouse position in world space
  Vector2 mousePos = GetScreenToWorld2D(renderMouse, cam);
  Vector2 playerCenter = {(float)cx + 50, (float)cy + 50};
  Vector2 delta = Vector2Subtract(playerCenter, mousePos);
  float angle = atan2f(delta.y, delta.x) * RAD2DEG;
  float oldrot = *rot;
  *rot = fmod(angle + 360.0f, 360.0f);

  return *rot == oldrot;
}

void move_camera(Camera2D *cam, int cx, int cy) {
  // Center the camera on the player
  cam->target = (Vector2){(float)cx + 50, (float)cy + 50};
  
  // Constrain camera to playing field boundaries
  // Use the render texture dimensions (window_size) for boundary calculations
  // since that's what the game world is rendered to
  float viewWidth = window_size.x / cam->zoom;
  float viewHeight = window_size.y / cam->zoom;
  
  if (cam->target.x < viewWidth / 2)
    cam->target.x = viewWidth / 2;
  if (cam->target.x > PLAYING_AREA.width - viewWidth / 2)
    cam->target.x = PLAYING_AREA.width - viewWidth / 2;
  if (cam->target.y < viewHeight / 2)
    cam->target.y = viewHeight / 2;
  if (cam->target.y > PLAYING_AREA.height - viewHeight / 2)
    cam->target.y = PLAYING_AREA.height - viewHeight / 2;
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
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  
  InitWindow(800, 600, "Multi Ludens");

  SetTargetFPS(60);

  ResourceManager res_man;

  Game game;
  GameConfig g_conf;
  g_conf.load();

  int my_id = -1;
  int server_update_counter = 0;
  bool hasmoved = false;
  int bdelay = 20;
  int canshoot = false;

  Color options[5] = {RED, GREEN, YELLOW, PURPLE, ORANGE};

  bool usernamechosen = false;

  Camera2D cam;
  cam.zoom = 1.0f;
  cam.rotation = 0.0f;
  cam.offset = (Vector2){window_size.x / 2.0f, window_size.y / 2.0f};
  // create render texture to draw game at normal res
  RenderTexture2D target = LoadRenderTexture(window_size.x, window_size.y);

  while (!WindowShouldClose() && running) {
    // calculate zoom based on actual window size relative to normal window size
    float widthRatio = (float)GetScreenWidth() / window_size.x;
    float heightRatio = (float)GetScreenHeight() / window_size.y;
    float scale = (widthRatio < heightRatio) ? widthRatio : heightRatio;

    handle_packets(&game, &my_id);

    if (my_id == -1) {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawTextScale("waiting for server...", 50, 50, 48, GREEN);
      EndDrawing();
      continue;
    };

    // Put here to make sure id exists
    int cx = game.players[my_id].x;
    int cy = game.players[my_id].y;

    move_camera(&cam, cx, cy);

    if (!usernamechosen) {
      manage_username_prompt(&(game.players), my_id, options, &g_conf);
      game.players[my_id].username = g_conf.username;
      game.players[my_id].color = options[g_conf.colorindex];
      usernamechosen = true;
    }

    server_update_counter++;

    bool moved = game.players.at(my_id).move();

    float scaledWidth = window_size.x * scale;
    float scaledHeight = window_size.y * scale;
    float offsetX = (GetScreenWidth() - scaledWidth) * 0.5f;
    float offsetY = (GetScreenHeight() - scaledHeight) * 0.5f;

    bool moved_gun = move_gun(&game.players[my_id].rot, cx, cy, cam, scale,
                              offsetX, offsetY);

    hasmoved = moved || moved_gun;

    if (server_update_counter >= 5 && hasmoved) {
      std::string msg =
          std::string("2\n" + std::to_string(game.players.at(my_id).x) + " " +
                      std::to_string(game.players.at(my_id).y) + " " +
                      std::to_string(game.players.at(my_id).rot));
      send_message(msg, sock);

      server_update_counter = 0;
    }

    game.update(my_id, cam);

    if (!canshoot)
      bdelay--;
    if (!canshoot && bdelay == 0) {
      canshoot = true;
    }

    if (IsMouseButtonDown(0) && canshoot) {
      canshoot = false;
      bdelay = 20;
      float bspeed = 10;
      float angleRad = (-game.players[my_id].rot + 5) * DEG2RAD;
      Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);
      Vector2 spawnOffset =
          Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -120);
      Vector2 origin = {(float)game.players[my_id].x + 50,
                        (float)game.players[my_id].y + 50};
      Vector2 spawnPos = Vector2Add(origin, spawnOffset);
      game.bullets.push_back(Bullet(spawnPos.x, spawnPos.y, dir, my_id));

      send_message(
          std::string("10\n ").append(std::to_string(game.players[my_id].rot)),
          sock);
    }

    // draw to render texture
    BeginTextureMode(target);
    ClearBackground(BLUE);

    BeginMode2D(cam);

    // Draw floor tiles based on PLAYING_AREA
    for (int i = 0; i < PLAYING_AREA.width / TILE_SIZE; i++) {
      for (int j = 0; j < PLAYING_AREA.height / TILE_SIZE; j++) {
        if (isInViewport(i * TILE_SIZE, j * TILE_SIZE, TILE_SIZE, TILE_SIZE,
                         cam))
          DrawTexture(res_man.getTex("assets/floor_tile.png"), i * TILE_SIZE,
                      j * TILE_SIZE, WHITE);
      }
    }

    draw_players(game.players, &res_man, my_id);

    for (Bullet &b : game.bullets)
      b.show();

    EndMode2D();

    // Draw HUD after EndMode2D but still in render texture
    draw_ui(my_true_color, game.players, game.bullets, my_id,
            bdelay, cam, scale);

    EndTextureMode();

    // draw the scaled render texture to the window
    BeginDrawing();
    ClearBackground(BLACK);

    // render texture to window
    Rectangle source = {0, 0, (float)target.texture.width,
                        (float)-target.texture.height};
    Rectangle dest = {offsetX, offsetY, scaledWidth, scaledHeight};
    DrawTexturePro(target.texture, source, dest, (Vector2){0, 0}, 0.0f, WHITE);

    EndDrawing();
  }

  // Cleanup
  UnloadRenderTexture(target);
  std::cout << "Closing.\n";

  running = false;

  shutdown(sock, SHUT_RDWR);
  close(sock);

  recv_thread.join();

  CloseWindow();
}

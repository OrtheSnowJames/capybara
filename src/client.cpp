#include "raylib.h"
#include "raymath.h"
#include "bullet.hpp"
#include "codes.hpp"
#include "constants.hpp"
#include "drawScale.hpp"
#include "game.hpp"
#include "game_config.hpp"
#include "math.h"
#include "networking.hpp"
#include "player.hpp"
#include "resource_manager.hpp"
#include "rainanimation.hpp"
#include "utils.hpp"
#include "umbrella.hpp"
#include <atomic>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

// Charging station constants and definitions
const int CHARGE_SIZE = 64;
const int CHARGE_OFFSET = 32;
const int PLAYER_SIZE = 50;

struct ChargingPoint {
  int x, y;
};

ChargingPoint charging_points[4] = {
  {CHARGE_OFFSET, (int)(PLAYING_AREA.height / 2) - CHARGE_OFFSET},
  {(int)PLAYING_AREA.width - CHARGE_OFFSET - CHARGE_SIZE, (int)(PLAYING_AREA.height / 2) - CHARGE_OFFSET},
  {(int)(PLAYING_AREA.width / 2) - CHARGE_OFFSET, CHARGE_OFFSET},
  {(int)(PLAYING_AREA.width / 2) - CHARGE_OFFSET, (int)PLAYING_AREA.height - CHARGE_OFFSET - CHARGE_SIZE} 
};

// Remove global socket declaration - will be created in main()
int sock = -1; // Will be initialized in main()

std::mutex packets_mutex;
std::list<std::string> packets = {};

std::atomic<bool> running = true;
std::string network_buffer = "";
Color my_true_color = RED;

// assassin event tracking
int my_target_id = -1;
bool is_assassin = false;

// assassin proximity audio
static Sound assassin_sound;
static bool assassin_sound_loaded = false;
static bool assassin_nearby = false;

// darkness effect tracking
static bool darkness_active = false;
static Vector2 darkness_offset = {0, 0};
static std::chrono::steady_clock::time_point last_darkness_update;
static float light_weakness = 0.3f; 

// flashlight battery
static float flashlight_time_left = 15.0f;  // 15 seconds of battery
static bool flashlight_usable = true;

// acid rain
static AcidRainEvent acid_rain;

// umbrella
static Umbrella player_umbrella;
static bool umbrella_usable = true;

// umbrella barrel
const int BARREL_SIZE = 50;
const int BARREL_COLLISION_SIZE = BARREL_SIZE * 2;
static Rectangle umbrella_barrel = {
  (PLAYING_AREA.width / 2) - (BARREL_COLLISION_SIZE / 2),  // Center the larger collision box
  (PLAYING_AREA.height / 2) - (BARREL_COLLISION_SIZE / 2),
  BARREL_COLLISION_SIZE,
  BARREL_COLLISION_SIZE
}; // middle of the playing field


void do_recv() {
  char buffer[1024];

  while (running) {
    int bytes = recv_data(sock, &buffer, sizeof(buffer), 0);

    if (bytes == 0) {
      std::cout << "Server disconnected.\n";
      running = false;
      break;
    } else if (bytes < 0) {
      print_socket_error("Error receiving packet");
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

enum EventType {
  Darkness = 0,
  Assasin = 1,
  Clear = 2,
  AcidRain = 3,
  NOTHING = 100
};

void handle_packet(int packet_type, std::string payload, Game *game,
                   int *my_id) {
  std::istringstream in(payload);
  std::vector<std::string> msg_split;

  switch (packet_type) {
  case MSG_GAME_STATE:
    std::cout << "Received game state: " << payload << std::endl;
    if (!payload.empty() && payload.back() == ';') {
      payload.pop_back();
    }
    split(payload, std::string(":"), msg_split);
    for (std::string i : msg_split) {
      if (i == std::string(""))
        continue;

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
      if (color_equal((*game).players.at(id).color, INVISIBLE) && id != *my_id) {
        // std::cout << "Client " << *my_id << ": Assassin " << id << " moved to (" << x << ", " << y << ") with rotation " << rot << std::endl;
      }
      
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
  case MSG_EVENT_SUMMON: {
    std::istringstream iss(payload);
    int event_type;
    iss >> event_type;

    if (iss.fail()) {
      std::cerr << "Invalid MSG_EVENT_SUMMON packet: " << payload << std::endl;
      break;
    }

    if (event_type == Darkness) {
      std::cout << "Received darkness event: " << payload << std::endl;
      darkness_active = true;
      darkness_offset = {0, 0};
      last_darkness_update = std::chrono::steady_clock::now();
    } else if (event_type == Assasin) {
      std::cout << "Received assasin event: " << payload << std::endl;
    } else if (event_type == AcidRain) {
      std::cout << "Received acid rain event: " << payload << std::endl;
      acid_rain.start(0.0f);
    } else if (event_type == Clear) {
      std::cout << "Received clear event: " << payload << std::endl;

      // clear all events
      darkness_active = false;
      acid_rain.stop();
    }
    break;
  }
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
      Color old_color = game->players.at(id).color;
      game->players.at(id).username = username;
      Color new_color = uint_to_color(color_code);
      game->players.at(id).color = new_color;

      std::cout << "Player " << id << " color changed from " << color_to_string(old_color) << " to " << color_to_string(new_color) << std::endl;

      // only update true color for local player when not invisible
      if (id == *my_id && !color_equal(new_color, INVISIBLE)) {
        Color old_true_color = my_true_color;
        my_true_color = new_color;
        
        std::cout << "Client: Local player true color changed from " << color_to_string(old_true_color) << " to " << color_to_string(my_true_color) << std::endl;
        
        // reset assassin state when visible again
        if (is_assassin) {
          is_assassin = false;
          my_target_id = -1;
          std::cout << "Assassin event ended - you are visible again." << std::endl;
        }
      }
    }
  } break;
  case MSG_BULLET_SHOT: {
    std::istringstream j(payload);
    int from_id, bullet_id, x, y;
    float rot;

    j >> from_id >> bullet_id >> x >> y >> rot;

    float angleRad = (-rot + 5) * DEG2RAD;
    float bspeed = 10;

    Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);

    std::cout << from_id << ' ' << x << ' ' << y << ' ' << ' ' << dir.x << ' '
              << dir.y << std::endl;

    Bullet new_bullet(x, y, dir, from_id);
    new_bullet.bullet_id = bullet_id;
    game->bullets.push_back(new_bullet);

    std::cout << "added bullet " << bullet_id << " at " << x << ' ' << y << " with dir " << dir.x
              << ' ' << dir.y << std::endl;

    break;
  }
  case MSG_BULLET_DESPAWN: {
    int bullet_id = std::stoi(payload);
    
    game->bullets.erase(
      std::remove_if(game->bullets.begin(), game->bullets.end(),
                    [bullet_id](const Bullet& b) { return b.bullet_id == bullet_id; }),
      game->bullets.end());
    
    std::cout << "removed bullet " << bullet_id << std::endl;
    break;
  }
  case MSG_ASSASSIN_CHANGE: {
    std::cout << "Received assassin change packet: " << payload << std::endl;
    std::istringstream assassin_stream(payload);
    int assassin_id, target_id;
    assassin_stream >> assassin_id >> target_id;
    
    if (assassin_stream.fail()) {
      std::cerr << "Invalid MSG_ASSASSIN_CHANGE packet: " << payload << std::endl;
      break;
    }
    
    std::cout << "Assassin event: Player " << assassin_id << " is targeting player " << target_id << std::endl;

    if (assassin_id == *my_id) {
      game->players[assassin_id].color = INVISIBLE;
      is_assassin = true;
      my_target_id = target_id;
      
      std::cout << "Client: You are now the assassin! Your target is player ID: " << target_id << std::endl;
    }
    
    break;
  }
  case MSG_SWITCH_WEAPON: {
    std::istringstream iss(payload);
    int player_id, weapon_id;
    iss >> player_id >> weapon_id;
    
    if (iss.fail()) {
      std::cerr << "Invalid MSG_SWITCH_WEAPON packet: " << payload << std::endl;
      break;
    }
    
    if (player_id != *my_id) {
      game->players[player_id].weapon_id = weapon_id;
    }
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

      std::cout << "Client: Selected color index: " << *mycolor << std::endl;
      std::cout << "Client: Selected color: " << color_to_string(options[*mycolor]) << std::endl;
      
      std::cout << "Client: Color code being sent: " << color_to_uint(options[*mycolor]) << std::endl;

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

void update_darkness_effect() {
  if (!darkness_active) return;
  
  auto current_time = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      current_time - last_darkness_update).count();
  
  if (elapsed >= 1) {
    // Generate random offset within 25x25 area
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-12, 12); // -12 to +12 gives us 25x25 area
    
    darkness_offset.x = dis(gen);
    darkness_offset.y = dis(gen);
    last_darkness_update = current_time;
  }
}

void draw_flashlight_cone(Vector2 playerScreenPos, float player_rotation) {
  // flashlight parameters
  float flashlight_range = 600.0f;
  float flashlight_angle = 45.0f; // cone angle in degrees
  
  // convert rotation to radians
  float angle_rad = (player_rotation - 90.0f) * DEG2RAD; // -90 to align with "up" direction
  
  // create flashlight cone using triangles
  Vector2 player_center = {playerScreenPos.x, playerScreenPos.y};
  
  // calculate cone endpoints
  float half_angle = (flashlight_angle * 0.5f) * DEG2RAD;
  
  Vector2 cone_left = {
    player_center.x + cosf(angle_rad - half_angle) * flashlight_range,
    player_center.y + sinf(angle_rad - half_angle) * flashlight_range
  };
  
  Vector2 cone_right = {
    player_center.x + cosf(angle_rad + half_angle) * flashlight_range,
    player_center.y + sinf(angle_rad + half_angle) * flashlight_range
  };
  
  Vector2 cone_center = {
    player_center.x + cosf(angle_rad) * flashlight_range,
    player_center.y + sinf(angle_rad) * flashlight_range
  };
  
  // draw the main cone with high intensity (very transparent for strong light)
  unsigned char main_intensity = (unsigned char)(255 * light_weakness * 0.2f); // Very strong light
  Color main_cone_color = {255, 255, 255, main_intensity};
  
  // draw center triangle (strongest light)
  DrawTriangle(player_center, cone_left, cone_center, main_cone_color);
  DrawTriangle(player_center, cone_center, cone_right, main_cone_color);
  
  // draw outer edges with medium intensity
  unsigned char edge_intensity = (unsigned char)(255 * light_weakness * 0.6f);
  Color edge_cone_color = {255, 255, 255, edge_intensity};
  
  // add some width to the cone edges for smoother falloff
  Vector2 outer_left = {
    player_center.x + cosf(angle_rad - half_angle * 1.2f) * flashlight_range,
    player_center.y + sinf(angle_rad - half_angle * 1.2f) * flashlight_range
  };
  
  Vector2 outer_right = {
    player_center.x + cosf(angle_rad + half_angle * 1.2f) * flashlight_range,
    player_center.y + sinf(angle_rad + half_angle * 1.2f) * flashlight_range
  };
  
  DrawTriangle(player_center, outer_left, cone_left, edge_cone_color);
  DrawTriangle(player_center, cone_right, outer_right, edge_cone_color);
}

void draw_ui(Color my_ui_color, playermap players,
             std::vector<Bullet> bullets, int my_id, int shoot_cooldown,
             Camera2D cam, float scale) {
  BeginUiDrawing();

  DrawFPS(0, 0);

  if (is_assassin && my_target_id != -1) {
    // draw assassin status box at top center
    DrawRectangle(300, 10, 200, 60, DARKGRAY);
    DrawText("ASSASSIN MODE", 320, 20, 16, RED);
    DrawText("Target:", 330, 35, 12, WHITE);
    if (my_target_id == my_id) {
      DrawText("Pending...", 330, 50, 12, YELLOW);
    } else if (players.count(my_target_id)) {
      DrawText(players[my_target_id].username.c_str(), 330, 50, 12, players[my_target_id].color);
    } else {
      DrawText("Unknown", 330, 50, 12, RED);
    }
  }

  // Base dimensions for UI elements
  const float boxHeight = 50;
  const float padding = 5;
  const float squareSize = 40;
  const float textSize = 24;
  const float textPadding = 50;
  const float textWidth = MeasureText(players[my_id].username.c_str(), textSize);
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

  // Draw charging stations on minimap (always visible)
  Color light_yellow = {255, 255, 200, 255};
  for (int i = 0; i < 4; i++) {
    float map_x = window_size.x - 100 + ((charging_points[i].x + CHARGE_SIZE/2) / (PLAYING_AREA.width / 100));
    float map_y = (charging_points[i].y + CHARGE_SIZE/2) / (PLAYING_AREA.height / 100);
    DrawCircle(map_x, map_y, 3, light_yellow);
  }

  if (darkness_active) {
    if (is_assassin) {
      // Assassins see everyone normally during darkness
      for (auto &[id, p] : players) {
        if (id == my_id) {
          DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                        p.y / (PLAYING_AREA.height / 100), 10, 10, my_ui_color);
        } else if (!color_equal(p.color, INVISIBLE)) {
          DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                        p.y / (PLAYING_AREA.height / 100), 10, 10, p.color);
        }
      }
    } else {
      // Non-assassins only see themselves with randomized position
      if (players.count(my_id)) {
        Player& local_player = players[my_id];
        float map_x = window_size.x - 100 + (local_player.x / (PLAYING_AREA.width / 100)) + darkness_offset.x;
        float map_y = (local_player.y / (PLAYING_AREA.height / 100)) + darkness_offset.y;
        
        map_x = std::max(window_size.x - 100.0f, std::min(window_size.x - 10.0f, map_x));
        map_y = std::max(0.0f, std::min(90.0f, map_y));
        
        DrawRectangle(map_x, map_y, 10, 10, my_ui_color);
      }
    }
  } else {
    // Normal visibility (no darkness)
    for (auto &[id, p] : players) {
      if (id == my_id) {
        DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                      p.y / (PLAYING_AREA.height / 100), 10, 10, my_ui_color);
      } else if (!color_equal(p.color, INVISIBLE)) {
        DrawRectangle(window_size.x - 100 + p.x / (PLAYING_AREA.width / 100),
                      p.y / (PLAYING_AREA.height / 100), 10, 10, p.color);
      }
    }
  }

  EndUiDrawing();
}

void draw_players(playermap players, std::vector<Bullet> bullets, ResourceManager *res_man, int my_id) {
  for (auto &[id, p] : players) {
    // Only skip unset players and invisible players that aren't the local player and aren't visible due to range
    if (p.username == "unset")
      continue;
      
    if (!color_equal(p.color, INVISIBLE) || id == my_id) {
    Color clr = p.color;
      if (id == my_id && is_assassin) {
        DrawTextureAlpha(res_man->load_player_texture_from_color(my_true_color), p.x, p.y, 128);
      } else if (id == my_id && color_equal(p.color, INVISIBLE)) {
        // draw with transparency using the original color
        DrawTextureAlpha(res_man->load_player_texture_from_color(my_true_color), p.x, p.y, 128);
      } else {
        DrawTexture(res_man->load_player_texture_from_color(clr), p.x, p.y, WHITE);
      }

      DrawText(p.username.c_str(),
               p.x + 50 - MeasureText(p.username.c_str(), 32) / 2, p.y - 50, 32,
               BLACK);
    }

    // weapon drawing based on weapon_id
    switch (static_cast<Weapon>(p.weapon_id)) {
      case gun_or_knife: {
        if (id == my_id && is_assassin) {
          Vector2 player_center = {(float)p.x + 50, (float)p.y + 50};
          float knife_offset = 80.0f;
          
          DrawTexturePro(res_man->getTex("assets/assassin_knife.png"), 
                         {(float)0, (float)0, 16, 16},
                         {player_center.x, player_center.y, 80, 80}, 
                         {(float)40 + knife_offset, (float)40}, 
                         p.rot, WHITE);
        } else if (color_equal(p.color, INVISIBLE) && id != my_id) {
          if (players.count(my_id)) {
            float distance = sqrtf(powf(p.x - players[my_id].x, 2) + powf(p.y - players[my_id].y, 2));
            float close_distance = 300.0f; 
            
            if (distance <= close_distance) {
              Vector2 player_center = {(float)p.x + 50, (float)p.y + 50};
              float knife_offset = 80.0f;
              
              // Calculate alpha based on distance - more transparent when further away
              float alpha_scale = 1.0f - (distance / close_distance);
              unsigned char alpha = (unsigned char)(alpha_scale * 192);
              Color knife_tint = {255, 255, 255, alpha};
              
              DrawTexturePro(res_man->getTex("assets/assassin_knife.png"), 
                             {(float)0, (float)0, 16, 16}, 
                             {player_center.x, player_center.y, 80, 80}, 
                             {(float)40 + knife_offset, (float)40}, 
                             p.rot, knife_tint);
              // play sound when close
              if (!assassin_sound_loaded) {
                assassin_sound = LoadSound("assets/assassin_close.wav");
                assassin_sound_loaded = true;
              }
              
              float volume = 1.0f - (distance / close_distance);
              volume = volume * 0.5f;
              SetSoundVolume(assassin_sound, volume);
              
              // start or continue looping
              if (!IsSoundPlaying(assassin_sound)) {
                PlaySound(assassin_sound);
              }
              assassin_nearby = true;
            } else {
              // assassin moved away, stop the sound
              if (assassin_nearby && IsSoundPlaying(assassin_sound)) {
                StopSound(assassin_sound);
              }
              assassin_nearby = false;
            }
          }
        } else {
          // gun/barrel/thing
    DrawRectanglePro({(float)p.x + 50, (float)p.y + 50, 50, 20},
                     {(float)100, (float)0}, p.rot, BLACK);
        }
        break;
      }
      case flashlight:
        DrawTexturePro(res_man->getTex("assets/flashlight.png"), 
                        {(float)0, (float)0, 16, 16},
                        {(float)p.x + 50, (float)p.y + 50, 50, 50},
                        {(float)100, (float)0}, p.rot, WHITE);
        break;
      case umbrella:
        if (id == my_id) {
          player_umbrella.is_active = true;
          player_umbrella.draw(res_man, p.x, p.y);
        } else {
          // Draw other players' umbrellas with hit detection
          Color umbrella_tint = WHITE;
          for (const Bullet &b : bullets) {
            Rectangle umbrella_rect = {(float)p.x, (float)p.y - 85, 75, 75};
            Rectangle bullet_rect = {(float)b.x, (float)b.y, (float)b.r * 2, (float)b.r * 2};
            if (CheckCollisionRecs(umbrella_rect, bullet_rect)) {
              umbrella_tint = RED;
              break;
            }
          }
          
          // Draw other players' umbrellas without state tracking
          DrawTexturePro(res_man->getTex("assets/umbrella.png"), 
                       {(float)0, (float)0, 16, 16},
                       {(float)p.x + 50, (float)p.y - 35, 75, 75},
                       {(float)37.5, (float)60}, 0, umbrella_tint);
        }
        break;
      default:
        // Unknown weapon type - no drawing
        break;
    }
  }
}

bool move_wpn(float *rot, int cx, int cy, Camera2D cam, float scale,
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

bool move_flashlight(float *rot, int cx, int cy, Camera2D cam, float scale,
                     float offsetX, float offsetY) {
  // mouse position in window
  Vector2 windowMouse = GetMousePosition();

  // mouse position in render texture
  Vector2 renderMouse = {(windowMouse.x - offsetX) / scale,
                         (windowMouse.y - offsetY) / scale};

  // mouse position in world space
  Vector2 mousePos = GetScreenToWorld2D(renderMouse, cam);
  Vector2 playerCenter = {(float)cx + 50, (float)cy + 50};
  
  // Calculate direction from player to mouse (opposite of weapon calculation)
  Vector2 delta = Vector2Subtract(playerCenter, mousePos);
  float angle = atan2f(delta.y, delta.x) * RAD2DEG;
  float oldrot = *rot;
  // Adjust angle for square flashlight (no offset needed like rectangular gun)
  *rot = fmod(angle + 360.0f, 360.0f);
  return *rot == oldrot;
}

void move_camera(Camera2D *cam, int cx, int cy) {
  // center the camera on the player
  cam->target = Vector2{(float)cx + 50, (float)cy + 50};
  
  // constrain camera to playing field boundaries
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

bool check_charging_station_collision(int player_x, int player_y) {
  for (int i = 0; i < 4; i++) {
    if (player_x < charging_points[i].x + CHARGE_SIZE && 
        player_x + PLAYER_SIZE > charging_points[i].x &&
        player_y < charging_points[i].y + CHARGE_SIZE && 
        player_y + PLAYER_SIZE > charging_points[i].y) {
      return true;
    }
  }
  return false;
}

int init_sock() {
  #if defined(_WIN32)
    if (initialize_winsock() != 0) {
      std::cerr << "Failed to initialize winsock" << std::endl;
      return 1;
    }
  #endif

  return 0;
}

int clean_sock() {
  #if defined(_WIN32)
    cleanup_winsock();
  #endif

  return 0;
}

std::string get_ip_from_args(int argc, char **argv) {
  if (argc >= 2) {
    return std::string(argv[1]);
  }

  return std::string("127.0.0.1");
}

bool switch_weapon(Weapon weapon, Game* game, int my_id, int sock, bool flashlight_usable) {
  if (weapon == Weapon::flashlight && !flashlight_usable) {
    std::cout << "Flashlight is not usable (battery depleted)" << std::endl;
    return false;
  }
  
  if (game->players[my_id].weapon_id == (int)weapon) {
    return false;
  }
  
  game->players[my_id].weapon_id = (int)weapon;
  
  std::ostringstream msg;
  msg << "12\n" << my_id << " " << (int)weapon;
  send_message(msg.str(), sock);
  
  return true;
}

int main(int argc, char **argv) {
  if (init_sock() == 1) return 1;

  // Create socket after Winsock initialization
  sock = create_socket(ADDRESS_FAMILY_INET, SOCKET_STREAM, 0);
  
  std::cout << "Socket creation returned: " << sock << std::endl;
  
  if (sock < 0) {
    print_socket_error("Failed to create socket");
    clean_sock();
    return 1;
  }

  socket_address_in sock_addr;
  sock_addr.sin_family = ADDRESS_FAMILY_INET;
  sock_addr.sin_port = host_to_network_short(50000);
  sock_addr.sin_addr.s_addr = ip_string_to_binary(get_ip_from_args(argc, argv).c_str()); // default to 127.0.0.1 if no arg

  if (connect_socket(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    print_socket_error("Could not connect to server");

    close_socket(sock);
    return -1;
  }

  std::thread recv_thread(do_recv);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  
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
  cam.offset = Vector2{window_size.x / 2.0f, window_size.y / 2.0f};
  // create render texture to draw game at normal res
  RenderTexture2D target = LoadRenderTexture(window_size.x, window_size.y);
  // Create light texture for darkness effect once
  Texture2D lightTex = {0};
  {
    int size = 400;
    Image lightImg = GenImageColor(size, size, Color{0, 0, 0, 0});
    
    float center = size / 2.0f;
    float max_radius = 200.0f;
    
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        float dx = x - center;
        float dy = y - center;
        float distance = sqrtf(dx * dx + dy * dy);
        
        if (distance <= max_radius) {
          float alpha_factor = distance / max_radius;
          alpha_factor = alpha_factor * alpha_factor;
          unsigned char alpha = (unsigned char)(255.0f * (1.0f - alpha_factor));
          
          Color pixel_color = {255, 255, 255, alpha};
          ImageDrawPixel(&lightImg, x, y, pixel_color);
        }
      }
    }
    
    lightTex = LoadTextureFromImage(lightImg);
    UnloadImage(lightImg);
  }

  RenderTexture2D darknessMask = LoadRenderTexture(window_size.x, window_size.y);
  Image lightImg = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLANK);
  lightTex = LoadTextureFromImage(lightImg);
  UnloadImage(lightImg);

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

    // update darkness effect
    update_darkness_effect();

    // update acid rain
    acid_rain.update(GetFrameTime());

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
    bool moved_gun = false;
    if (game.players[my_id].weapon_id == Weapon::gun_or_knife) {
      moved_gun = move_wpn(&game.players[my_id].rot, cx, cy, cam, scale,
                              offsetX, offsetY);
    } else if (game.players[my_id].weapon_id == Weapon::flashlight) {
      moved_gun = move_flashlight(&game.players[my_id].rot, cx, cy, cam, scale,
                              offsetX, offsetY);
    }
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

    if (IsMouseButtonDown(0) && canshoot && game.players[my_id].weapon_id == 0 && !is_assassin) {
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

    // flashlight battery
    {
      if (game.players[my_id].weapon_id == Weapon::flashlight && flashlight_usable) {
        float deltaTime = GetFrameTime();
        flashlight_time_left -= deltaTime;

        if (flashlight_time_left <= 0.0f) {
          flashlight_time_left = 0.0f;
          flashlight_usable = false;
          switch_weapon(Weapon::gun_or_knife, &game, my_id, sock, flashlight_usable);
          std::cout << "Flashlight battery depleted!" << std::endl;
        }
      }
      
      // check charger coil 
      if (check_charging_station_collision(game.players[my_id].x, game.players[my_id].y)) {
        flashlight_usable = true;
        flashlight_time_left = 15.0f;  // Reset to full battery
      }
    }

    // detect weapon switching with keyboard keys
    {
      const int MAX_WEAPON_ID = 2;
      
      bool weapon_changed = false;
      Weapon currentWeapon = (Weapon)game.players[my_id].weapon_id;
      
      if (IsKeyPressed(KEY_ONE)) {
        currentWeapon = Weapon::gun_or_knife; 
        weapon_changed = true;
      } else if (IsKeyPressed(KEY_TWO) && flashlight_usable) {
        currentWeapon = Weapon::flashlight; 
        weapon_changed = true;
      } else if (IsKeyPressed(KEY_THREE) && umbrella_usable) {
        currentWeapon = Weapon::umbrella;
        weapon_changed = true;
      }
      
      if (weapon_changed && currentWeapon != game.players[my_id].weapon_id) {
        switch_weapon(currentWeapon, &game, my_id, sock, flashlight_usable);
      }
    }

    // update umbrella
    {
      Rectangle player_rect = {(float)game.players[my_id].x, (float)game.players[my_id].y, (float)PLAYER_SIZE, (float)PLAYER_SIZE};
      std::vector<Rectangle> bullets_rects;
      for (Bullet &b : game.bullets) {
        bullets_rects.push_back(Rectangle{(float)b.x, (float)b.y, (float)b.r * 2, (float)b.r * 2});
      }

      umbrella_usable = player_umbrella.update(umbrella_barrel, player_rect, bullets_rects, game.bullets, game.players[my_id].weapon_id == Weapon::umbrella);

      if (!umbrella_usable && game.players[my_id].weapon_id == Weapon::umbrella) {
        game.players[my_id].weapon_id = Weapon::gun_or_knife;
        
        // weapon switch message 
        std::ostringstream msg;
        msg << "12\n" << my_id << " " << (int)Weapon::gun_or_knife;
        send_message(msg.str(), sock);
      }
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

    // Draw charging stations
    for (int i = 0; i < 4; i++) {
      if (isInViewport(charging_points[i].x, charging_points[i].y, CHARGE_SIZE, CHARGE_SIZE, cam)) {
        DrawTexturePro(res_man.getTex("assets/charger.png"),
                       {0, 0, 16, 16}, 
                       {(float)charging_points[i].x, (float)charging_points[i].y, (float)CHARGE_SIZE, (float)CHARGE_SIZE}, 
                       {0, 0}, 0.0f, WHITE);
      }
    }
    // draw umbrella barrel
    // if any player is touching the barrel, tint it green
    Color barrel_tint = WHITE;
    for (const auto& [_, p] : game.players) {
        Rectangle player_rect = {(float)p.x, (float)p.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE};
        if (CheckCollisionRecs(umbrella_barrel, player_rect)) {
            barrel_tint = GREEN;
            break;
        }
    }

    DrawTexturePro(res_man.getTex("assets/barrel.png"),
                   {0, 0, 16, 16},  
                   {umbrella_barrel.x + BARREL_SIZE/2, 
                    umbrella_barrel.y + BARREL_SIZE/2,
                    BARREL_SIZE, 
                    BARREL_SIZE},
                   {0, 0}, 
                   0.0f,   
                   barrel_tint);

    draw_players(game.players, game.bullets, &res_man, my_id);

    for (Bullet &b : game.bullets)
      b.show();

    // Draw acid rain effect
    acid_rain.draw(game.players);

    EndMode2D();

    EndTextureMode();

    // Apply darkness effect if active and player is not assassin
    if (darkness_active && !is_assassin) {
      // Clear and prepare darkness mask each frame
      BeginTextureMode(darknessMask);
      ClearBackground(Color{0, 0, 0, 254}); 
      
      // Draw lights for all players
      for (auto &[id, p] : game.players) {
        Vector2 playerScreenPos = GetWorldToScreen2D(
          Vector2{(float)p.x + 50, (float)p.y + 50}, 
          cam
        );

        if (id == my_id || p.weapon_id == Weapon::flashlight) {
          DrawTexture(lightTex, 
                     playerScreenPos.x - lightTex.width / 2, 
                     playerScreenPos.y - lightTex.height / 2, 
                     WHITE);
        }
        
        // Draw flashlight beam if player has flashlight
        if (p.weapon_id == Weapon::flashlight) {
          float angleRad = (-p.rot + 5 + 180) * DEG2RAD;  // Add 180 degrees to point in the right direction
          float flashlight_distance = 600.0f;  // How far the light reaches
          
          float spread_angle = 15.0f * DEG2RAD;  // Narrower beam
          
          for (int i = 0; i < 4; i++) {
            float current_spread = spread_angle * (1.0f - (float)i / 4.0f);  // Narrower spread for inner beams
            float alpha = 180 - (i * 40);  // Brighter in the center
            
            Vector2 beam_left = {
              playerScreenPos.x + cosf(angleRad - current_spread) * flashlight_distance,
              playerScreenPos.y - sinf(angleRad - current_spread) * flashlight_distance
            };
            
            Vector2 beam_right = {
              playerScreenPos.x + cosf(angleRad + current_spread) * flashlight_distance,
              playerScreenPos.y - sinf(angleRad + current_spread) * flashlight_distance
            };
            
            // Draw layered beams
            Color beam_color = {255, 255, 255, (unsigned char)alpha};
            DrawTriangle(playerScreenPos, beam_left, beam_right, beam_color);
          }
          
          // Add a small intense light at the source
          float source_size = 30.0f;
          Color source_color = {255, 255, 255, 200};
          DrawCircle(playerScreenPos.x, playerScreenPos.y, source_size, source_color);
        }
      }
      EndTextureMode();
      
      // Apply darkness mask to main render target
      BeginTextureMode(target);
      BeginBlendMode(BLEND_MULTIPLIED);
      
      Rectangle source = {0, 0, (float)darknessMask.texture.width, (float)-darknessMask.texture.height};
      Rectangle dest = {0, 0, (float)window_size.x, (float)window_size.y};
      DrawTexturePro(darknessMask.texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);
      
      EndBlendMode();
      EndTextureMode();
    }

    // Draw all HUD elements on top (after darkness effect)
    BeginTextureMode(target);
    draw_ui(my_true_color, game.players, game.bullets, my_id, (20 - bdelay), cam, scale);
    EndTextureMode();

    // draw the scaled render texture to the window
    BeginDrawing();
    ClearBackground(BLACK);

    // render texture to window
    Rectangle source = {0, 0, (float)target.texture.width,
                        (float)-target.texture.height};
    Rectangle dest = {offsetX, offsetY, scaledWidth, scaledHeight};
    DrawTexturePro(target.texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);

    EndDrawing();
  }

  // Cleanup
  UnloadRenderTexture(target);
  UnloadRenderTexture(darknessMask);
  if (lightTex.id != 0) {
    UnloadTexture(lightTex);
  }
  std::cout << "Closing.\n";

  running = false;

  shutdown_socket(sock, SHUTDOWN_BOTH);
  close_socket(sock);

  recv_thread.join();

  CloseWindow();

  return clean_sock(); // return 0 if successful
}


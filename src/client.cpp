#include "bullet.hpp"
#include "codes.hpp"
#include "constants.hpp"
#include "drawScale.hpp"
#include "game.hpp"
#include "game_config.hpp"
#include "math.h"
#include "netvent.hpp"
#include "networking.hpp"
#include "objects.hpp"
#include "player.hpp"
#include "collision.hpp"
#include "rainanimation.hpp"
#include "raylib.h"
#include "raymath.h"
#include "resource_manager.hpp"
#include "umbrella.hpp"
#include "utils.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Charging station constants and definitions
const int CHARGE_SIZE = 64;
const int CHARGE_OFFSET = 32;
const int PLAYER_SIZE = 50;

struct ChargingPoint {
  int x, y;
};

ChargingPoint charging_points[4] = {
    {CHARGE_OFFSET, (int)(PLAYING_AREA.height / 2) - CHARGE_OFFSET},
    {(int)PLAYING_AREA.width - CHARGE_OFFSET - CHARGE_SIZE,
     (int)(PLAYING_AREA.height / 2) - CHARGE_OFFSET},
    {(int)(PLAYING_AREA.width / 2) - CHARGE_OFFSET, CHARGE_OFFSET},
    {(int)(PLAYING_AREA.width / 2) - CHARGE_OFFSET,
     (int)PLAYING_AREA.height - CHARGE_OFFSET - CHARGE_SIZE}};

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
static float flashlight_time_left = 15.0f; // 15 seconds of battery
static bool flashlight_usable = true;

// acid rain
static AcidRainEvent acid_rain;

// umbrella
static Umbrella player_umbrella;
static UmbrellaUpdateData umbrella_update_data;
static UmbrellaUpdateData umbrella_update_data_last_frame;

// umbrella barrel
const int BARREL_SIZE = 50;
const int BARREL_COLLISION_SIZE = BARREL_SIZE * 2;
static Rectangle umbrella_barrel = {
    (PLAYING_AREA.width / 2) -
        (BARREL_COLLISION_SIZE / 2), // Center the larger collision box
    (PLAYING_AREA.height / 2) - (BARREL_COLLISION_SIZE / 2),
    BARREL_COLLISION_SIZE,
    BARREL_COLLISION_SIZE}; // middle of the playing field

// cubes
std::vector<Object> cubes;

// move state
CanMoveState can_move_state = {false, false, false, false};

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
                   int *my_id, ResourceManager *res_man) {
  std::istringstream in(payload);
  std::vector<std::string> msg_split;

  switch (packet_type) {
  case MSG_GAME_STATE: {
    std::cout << "Received game state: " << payload << std::endl;
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_GAME_STATE) {
      auto players_table = data["players"].as_table();
      for (const auto& [key, value] : players_table.get_data_map()) {
        int player_id = key.as_int();
        auto player = Player(value.as_table());
        (*game).players[player_id] = player;
      }
      cubes = objects_from_table(data["cubes"].as_table(), res_man->getTex("assets/floor_tile.png"));
      int current_event = data["current_event"].as_int();
      if (current_event == EventType::Darkness) {
        darkness_active = true;
      } else if (current_event == EventType::AcidRain) {
        acid_rain.start(0.0f);
      } else if (current_event == EventType::Assasin) {
        int assassin_id = data["assassin_id"].as_int();
        game->players[assassin_id].color = INVISIBLE;
        if (assassin_id == *my_id) {
          is_assassin = true;
          my_target_id = data["target_id"].as_int();
        }
      }
      break;
    }
  }
  case MSG_CLIENT_ID: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_CLIENT_ID) {
      *my_id = data["id"].as_int();
    }
    break;
  }
  case MSG_PLAYER_MOVE: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_PLAYER_MOVE) {
      int id = data["id"].as_int();
      int x = data["x"].as_int();
      int y = data["y"].as_int();
      float rot = data["rot"].as_float();

      if (id == *my_id)
        break; // so the movement feels smoother

      if ((*game).players.find(id) != (*game).players.end()) {
        if (color_equal((*game).players.at(id).color, INVISIBLE) &&
            id != *my_id) {
          // std::cout << "Client " << *my_id << ": Assassin " << id << " moved
          // to (" << x << ", " << y << ") with rotation " << rot << std::endl;
        }

        (*game).players.at(id).nx = x;
        (*game).players.at(id).ny = y;
        (*game).players.at(id).rot = rot;
      }
    }
  } break;
  case MSG_PLAYER_NEW: {
    std::cout << "Received player new: " << payload << std::endl;
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_PLAYER_NEW) {
      int id = data["id"].as_int();
      int x = data["x"].as_int();
      int y = data["y"].as_int();
      std::string username = data["username"].as_string();
      netvent::Table color_table = data["color"].as_table();
      int weapon_id = data["weapon_id"].as_int();

      (*game).players[id] = Player(x, y);
      (*game).players[id].username = username;
      (*game).players[id].color = color_from_table(color_table);
      (*game).players[id].weapon_id = weapon_id;
    }

    break;
  }
  case MSG_PLAYER_LEFT: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_PLAYER_LEFT) {
      int id = data["id"].as_int();
      if ((*game).players.find(id) != (*game).players.end())
        game->players.erase(id);
    }
    break;
  }
  case MSG_EVENT_SUMMON: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_EVENT_SUMMON) {
      int event_type = data["event_type"].as_int();

      std::cout << "Received event type: " << event_type << std::endl;

      switch (event_type) {
        case Darkness:
          std::cout << "Received darkness event: " << payload << std::endl;
          darkness_active = true;
          darkness_offset = {0, 0};
          last_darkness_update = std::chrono::steady_clock::now();
          break;
        case Assasin:
          std::cout << "Received assasin event: " << payload << std::endl;
          break;
        case AcidRain:
          std::cout << "Received acid rain event: " << payload << std::endl;
          acid_rain.start(0.0f);
          break;
        case Clear:
          std::cout << "Received clear event: " << payload << std::endl;
          darkness_active = false;
          acid_rain.stop();
          break;
      }
    }
    break;
  }
  case MSG_PLAYER_UPDATE: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_PLAYER_UPDATE) {
      int id = data["id"].as_int();
      std::string username = data["username"].as_string();
      netvent::Table color_table = data["color"].as_table();

      if (game->players.count(id)) {
        Color old_color = game->players.at(id).color;
        game->players.at(id).username = username;
        Color new_color = color_from_table(color_table);
        game->players.at(id).color = new_color;

        std::cout << "Player " << id << " color changed from "
                  << color_to_string(old_color) << " to "
                  << color_to_string(new_color) << std::endl;

        // only update true color for local player when not invisible
        if (id == *my_id && !color_equal(new_color, INVISIBLE)) {
          Color old_true_color = my_true_color;
          my_true_color = new_color;

          std::cout << "Client: Local player true color changed from "
                    << color_to_string(old_true_color) << " to "
                    << color_to_string(my_true_color) << std::endl;

          // reset assassin state when visible again
          if (is_assassin) {
            is_assassin = false;
            my_target_id = -1;
            std::cout << "Assassin event ended - you are visible again."
                      << std::endl;
          }
        }
      }
    }
  } break;
  case MSG_BULLET_SHOT: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_BULLET_SHOT) {
      int from_id = data["player_id"].as_int();
      int bullet_id = data["bullet_id"].as_int();
      int x = data["x"].as_int();
      int y = data["y"].as_int();
      float rot = data["rot"].as_float();

      float angleRad = (-rot + 5) * DEG2RAD;
      float bspeed = 10;

      Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);

      std::cout << "Client: Received bullet " << bullet_id << " from player " << from_id 
                << " at (" << x << ", " << y << ")" << std::endl;

      Bullet new_bullet(x, y, dir, from_id, bullet_id);
      game->bullets.push_back(new_bullet);
    }
    break;
  }
  case MSG_BULLET_DESPAWN: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_BULLET_DESPAWN) {
      int bullet_id = data["bullet_id"].as_int();

      size_t before_size = game->bullets.size();
      game->bullets.erase(std::remove_if(game->bullets.begin(),
                                         game->bullets.end(),
                                         [bullet_id](const Bullet &b) {
                                           return b.bullet_id == bullet_id;
                                         }),
                          game->bullets.end());
      size_t after_size = game->bullets.size();

      if (before_size != after_size) {
        std::cout << "Client: Removed bullet " << bullet_id << std::endl;
      } else {
        std::cout << "Client: Warning - Tried to remove non-existent bullet " << bullet_id << std::endl;
      }
    }
    break;
  }
  case MSG_ASSASSIN_CHANGE: {
    std::cout << "Received assassin change packet: " << payload << std::endl;
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_ASSASSIN_CHANGE) {
      int assassin_id = data["assassin_id"].as_int();
      int target_id = data["target_id"].as_int();

      std::cout << "Assassin event: Player " << assassin_id
                << " is targeting player " << target_id << std::endl;

      if (assassin_id == *my_id) {
        game->players[assassin_id].color = INVISIBLE;
        is_assassin = true;
        my_target_id = target_id;

        std::cout
            << "Client: You are now the assassin! Your target is player ID: "
            << target_id << std::endl;
      }

  
    }
    break;
  }
  case MSG_SWITCH_WEAPON: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_SWITCH_WEAPON) {
      int player_id = data["player_id"].as_int();
      int weapon_id = data["weapon_id"].as_int();

      std::cout << "Client: Player " << player_id << " switched weapon to "
                << weapon_id << std::endl;

      if (player_id != *my_id) {
        game->players[player_id].weapon_id = weapon_id;
      }

      if (weapon_id == Weapon::umbrella) {
        game->players[player_id].rot = 0;
      }
    }
  } break;
  case MSG_UMBRELLA_SHOOT: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_UMBRELLA_SHOOT) {
      int player_id = data["player_id"].as_int();
      float rot = data["rot"].as_float();
      game->players[player_id].rot = rot; 

      game->players[player_id].is_shooting = true;
    }
  }
  case MSG_UMBRELLA_STOP: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_UMBRELLA_STOP) {
      int player_id = data["player_id"].as_int();
      game->players[player_id].is_shooting = false;
    }
  } break;
  case MSG_RAINDROP_SPAWN: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_RAINDROP_SPAWN) {
      int raindrop_id = data["raindrop_id"].as_int();
      float x = data["x"].as_float();
      float y = data["y"].as_float();
      float speed = data["speed"].as_float();
      float size = data["size"].as_float();
      float rot = data["rot"].as_float();
      float alpha = data["alpha"].as_float();

      RainDrop drop;
      drop.position.x = x;
      drop.position.y = y;
      drop.speed = speed;
      drop.size = size;
      drop.rot = rot;
      drop.alpha = alpha;
      drop.raindrop_id = raindrop_id;
      
      game->raindrops.push_back(drop);
      
      std::cout << "Client: Received raindrop " << raindrop_id << " at (" << x << ", " << y << ")" << std::endl;
    }
    break;
  }
  case MSG_RAINDROP_DESPAWN: {
    auto [event_name, data] = netvent::deserialize_from_netvent(payload);
    if (event_name.as_int() == MSG_RAINDROP_DESPAWN) {
      int raindrop_id = data["raindrop_id"].as_int();

      size_t before_size = game->raindrops.size();
      auto new_end = std::remove_if(game->raindrops.begin(),
                                   game->raindrops.end(),
                                   [raindrop_id](const RainDrop &r) {
                                     return r.raindrop_id == raindrop_id;
                                   });
      game->raindrops.erase(new_end, game->raindrops.end());
      size_t after_size = game->raindrops.size();

      if (before_size != after_size) {
        std::cout << "Client: Removed raindrop " << raindrop_id << std::endl;
      } else {
        std::cout << "Client: Warning - Tried to remove non-existent raindrop " << raindrop_id << std::endl;
      }
    }
    break;
  }
  }
}

void cube_loop(std::vector<Object> cubes, Camera2D cam, ResourceManager *res_man) {
  for (Object& cube : cubes) {
    if (isInViewport(cube.bounds.x, cube.bounds.y, cube.bounds.width, cube.bounds.height, cam, 100)) {
      cube.draw();
    }
  }
}


void handle_packets(Game *game, int *my_id, ResourceManager *res_man) {
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

    handle_packet(packet_type, packet, game, my_id, res_man);
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

  if (acceptable_username(std::string(1, k)) &&
      (*usernameprompt).length() < 10) {
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
      std::cout << "Client: Selected color: "
                << color_to_string(options[*mycolor]) << std::endl;

      std::cout << "Client: Color code being sent: "
                << color_to_uint(options[*mycolor]) << std::endl;

      std::string msg = netvent::serialize_to_netvent(
          netvent::val(MSG_PLAYER_UPDATE),
          std::map<std::string, netvent::Value>({
              {"username", netvent::val(*usernameprompt)},
              {"color", netvent::val(color_to_table(options[*mycolor]))}
          }));
      send_message(msg, sock);
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
  if (!darkness_active)
    return;

  auto current_time = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                     current_time - last_darkness_update)
                     .count();

  if (elapsed >= 1) {
    // Generate random offset within 25x25 area
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-12,
                                        12); // -12 to +12 gives us 25x25 area

    darkness_offset.x = dis(gen);
    darkness_offset.y = dis(gen);
    last_darkness_update = current_time;
  }
}

// Helper function to check ray-rectangle intersection and return distance
float check_ray_cube_intersection(Vector2 start, Vector2 direction, float max_distance, const std::vector<Object>& cubes) {
  float closest_distance = max_distance;
  
  for (const auto& cube : cubes) {
    if (cube.type != ObjectType::Cube) continue;
    
    // Check intersection with cube bounds
    Rectangle cube_rect = cube.bounds;
    
    float t_min = 0.0f;
    float t_max = max_distance;
    
    // Check X boundaries
    if (fabs(direction.x) > 0.001f) {
      float t1 = (cube_rect.x - start.x) / direction.x;
      float t2 = (cube_rect.x + cube_rect.width - start.x) / direction.x;
      
      if (t1 > t2) std::swap(t1, t2);
      
      t_min = std::max(t_min, t1);
      t_max = std::min(t_max, t2);
    } else {
      // Ray is parallel to X boundaries
      if (start.x < cube_rect.x || start.x > cube_rect.x + cube_rect.width) {
        continue; // No intersection possible
      }
    }
    
    // Check Y boundaries
    if (fabs(direction.y) > 0.001f) {
      float t1 = (cube_rect.y - start.y) / direction.y;
      float t2 = (cube_rect.y + cube_rect.height - start.y) / direction.y;
      
      if (t1 > t2) std::swap(t1, t2);
      
      t_min = std::max(t_min, t1);
      t_max = std::min(t_max, t2);
    } else {
      // Ray is parallel to Y boundaries
      if (start.y < cube_rect.y || start.y > cube_rect.y + cube_rect.height) {
        continue; // No intersection possible
      }
    }
    
    // Check if intersection exists and is valid
    if (t_min <= t_max && t_min > 0.0f && t_min < closest_distance) {
      closest_distance = t_min;
    }
  }
  
  return closest_distance;
}

void draw_flashlight_cone(Vector2 playerScreenPos, float player_rotation) {
  // flashlight parameters
  float flashlight_range = 600.0f;
  float flashlight_angle = 45.0f; // cone angle in degrees

  // convert rotation to radians
  float angle_rad =
      (player_rotation - 90.0f) * DEG2RAD; // -90 to align with "up" direction

  // create flashlight cone using triangles
  Vector2 player_center = {playerScreenPos.x, playerScreenPos.y};

  // calculate cone endpoints
  float half_angle = (flashlight_angle * 0.5f) * DEG2RAD;

  Vector2 cone_left = {
      player_center.x + cosf(angle_rad - half_angle) * flashlight_range,
      player_center.y + sinf(angle_rad - half_angle) * flashlight_range};

  Vector2 cone_right = {
      player_center.x + cosf(angle_rad + half_angle) * flashlight_range,
      player_center.y + sinf(angle_rad + half_angle) * flashlight_range};

  Vector2 cone_center = {player_center.x + cosf(angle_rad) * flashlight_range,
                         player_center.y + sinf(angle_rad) * flashlight_range};

  // draw the main cone with high intensity (very transparent for strong light)
  unsigned char main_intensity =
      (unsigned char)(255 * light_weakness * 0.2f); // Very strong light
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
      player_center.y + sinf(angle_rad - half_angle * 1.2f) * flashlight_range};

  Vector2 outer_right = {
      player_center.x + cosf(angle_rad + half_angle * 1.2f) * flashlight_range,
      player_center.y + sinf(angle_rad + half_angle * 1.2f) * flashlight_range};

  DrawTriangle(player_center, outer_left, cone_left, edge_cone_color);
  DrawTriangle(player_center, cone_right, outer_right, edge_cone_color);
}

void draw_ui(Color my_ui_color, playermap players, std::vector<Bullet> bullets,
             int my_id, int shoot_cooldown, Camera2D cam, float scale) {
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
      DrawText(players[my_target_id].username.c_str(), 330, 50, 12,
               players[my_target_id].color);
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
  const float textWidth =
      MeasureText(players[my_id].username.c_str(), textSize);
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
    float map_x =
        window_size.x - 100 +
        ((charging_points[i].x + CHARGE_SIZE / 2) / (PLAYING_AREA.width / 100));
    float map_y =
        (charging_points[i].y + CHARGE_SIZE / 2) / (PLAYING_AREA.height / 100);
    DrawCircle(map_x, map_y, 3, light_yellow);
  }

  float barrel_map_x =
      window_size.x - 100 +
      ((umbrella_barrel.x + BARREL_SIZE) / (PLAYING_AREA.width / 100));
  float barrel_map_y =
      (umbrella_barrel.y + BARREL_SIZE) / (PLAYING_AREA.height / 100);
  DrawCircle(barrel_map_x, barrel_map_y, 3, BROWN);

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
      // non-assassins see themselves and players with flashlights
      for (auto &[id, p] : players) {
        if (id == my_id || p.weapon_id == Weapon::flashlight) {
          Color display_color = (id == my_id) ? my_ui_color : p.color;
          float map_x =
              window_size.x - 100 + (p.x / (PLAYING_AREA.width / 100));
          float map_y = (p.y / (PLAYING_AREA.height / 100));

          if (id == my_id) {
            map_x += darkness_offset.x;
            map_y += darkness_offset.y;

            map_x = std::max(window_size.x - 100.0f,
                             std::min(window_size.x - 10.0f, map_x));
            map_y = std::max(0.0f, std::min(90.0f, map_y));
          }

          DrawRectangle(map_x, map_y, 10, 10, display_color);
        }
      }
    }
  } else {
    // normal visibility (no darkness)
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

void draw_players(playermap players, std::vector<Bullet> bullets,
                  ResourceManager *res_man, int my_id) {
  for (auto &[id, p] : players) {
    // Only skip unset players and invisible players that aren't the local
    // player and aren't visible due to range
    if (p.username == "unset")
      continue;

    if (!color_equal(p.color, INVISIBLE) || id == my_id) {
      Color shadow_color = {0, 0, 0, 80};
      float shadow_width = 80;
      float shadow_height = 30;
      float shadow_y_offset = 90;
      DrawEllipse(p.x + 50, p.y + shadow_y_offset, shadow_width/2, shadow_height/2, shadow_color);

      Color clr = p.color;
      if (id == my_id && is_assassin) {
        DrawTextureAlpha(res_man->load_player_texture_from_color(my_true_color),
                         p.x, p.y, 128);
      } else if (id == my_id && color_equal(p.color, INVISIBLE)) {
        // draw with transparency using the original color
        DrawTextureAlpha(res_man->load_player_texture_from_color(my_true_color),
                         p.x, p.y, 128);
      } else {
        DrawTexture(res_man->load_player_texture_from_color(clr), p.x, p.y,
                    WHITE);
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
                       {(float)40 + knife_offset, (float)40}, p.rot, WHITE);
      } else if (color_equal(p.color, INVISIBLE) && id != my_id) {
        if (players.count(my_id)) {
          float distance = sqrtf(powf(p.x - players[my_id].x, 2) +
                                 powf(p.y - players[my_id].y, 2));
          float close_distance = 300.0f;

          if (distance <= close_distance) {
            Vector2 player_center = {(float)p.x + 50, (float)p.y + 50};
            float knife_offset = 80.0f;

            // Calculate alpha based on distance - more transparent when further
            // away
            float alpha_scale = 1.0f - (distance / close_distance);
            unsigned char alpha = (unsigned char)(alpha_scale * 192);
            Color knife_tint = {255, 255, 255, alpha};

            DrawTexturePro(res_man->getTex("assets/assassin_knife.png"),
                           {(float)0, (float)0, 16, 16},
                           {player_center.x, player_center.y, 80, 80},
                           {(float)40 + knife_offset, (float)40}, p.rot,
                           knife_tint);
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
        player_umbrella.draw(res_man, p.x, p.y, p.rot);
      } else {
        Color umbrella_tint = WHITE;
        for (const Bullet &b : bullets) {
          Rectangle umbrella_rect = {(float)p.x, (float)p.y - 85, 75, 75};
          Rectangle bullet_rect = {(float)b.x, (float)b.y, (float)b.r * 2,
                                   (float)b.r * 2};
          if (CheckCollisionRecs(umbrella_rect, bullet_rect)) {
            umbrella_tint = RED;
            break;
          }
        }

        float umbrella_x, umbrella_y;
        float umbrella_rotation;
        
        if (p.is_shooting) {
          // When shooting, position umbrella around the player based on rotation
          float distance = 80.0f; // Distance from player center
          float angle_rad = (p.rot - 90.0f) * DEG2RAD; // Convert to radians and adjust for up direction
          
          umbrella_x = (p.x + 50) + cosf(angle_rad) * distance;
          umbrella_y = (p.y + 50) + sinf(angle_rad) * distance;
          umbrella_rotation = p.rot;
        } else {
          // When not shooting, position umbrella above the player (default position)
          umbrella_x = p.x + 50;
          umbrella_y = p.y - 35;
          umbrella_rotation = 0.0f;
        }
        
        DrawTexturePro(res_man->getTex("assets/umbrella.png"),
                       {(float)0, (float)0, 16, 16},
                       {umbrella_x, umbrella_y, 75, 75},
                       {(float)37.5, (float)37.5}, umbrella_rotation, umbrella_tint);
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

bool move_umbrella(float *rot, int cx, int cy, Camera2D cam, float scale,
                   float offsetX, float offsetY) {
  // mouse position in window
  Vector2 windowMouse = GetMousePosition();

  // mouse position in render texture
  Vector2 renderMouse = {(windowMouse.x - offsetX) / scale,
                         (windowMouse.y - offsetY) / scale};

  // mouse position in world space
  Vector2 mousePos = GetScreenToWorld2D(renderMouse, cam);
  Vector2 playerCenter = {(float)cx + 50, (float)cy + 50};

  // Calculate direction from player to mouse
  Vector2 delta = Vector2Subtract(mousePos, playerCenter);
  float angle = atan2f(delta.y, delta.x) * RAD2DEG;
  float oldrot = *rot;
  
  // Since umbrella sprite faces up (0 degrees), we need to adjust the angle
  // Add 90 degrees to align with the sprite's natural orientation
  *rot = fmod(angle + 90.0f + 360.0f, 360.0f);
  
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
  Rectangle player_rect = {(float)player_x, (float)player_y, (float)PLAYER_SIZE,
                           (float)PLAYER_SIZE};
  for (auto &obj : objects) {
    if (obj.type == ObjectType::Charger && obj.check_collision(player_rect)) {
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

bool switch_weapon(Weapon weapon, Game *game, int my_id, int sock,
                   bool flashlight_usable) {
  if (weapon == Weapon::flashlight && !flashlight_usable) {
    return false;
  }

  if (game->players[my_id].weapon_id == (int)weapon) {
    return false;
  }

  game->players[my_id].weapon_id = (int)weapon;

  std::string msg = netvent::serialize_to_netvent(
      netvent::val((int)MSG_SWITCH_WEAPON),
      std::map<std::string, netvent::Value>(
          {{"player_id", netvent::val(my_id)},
           {"weapon_id", netvent::val((int)weapon)}}));
  send_message(msg, sock);

  return true;
}

int main(int argc, char **argv) {
  if (init_sock() == 1)
    return 1;

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
  sock_addr.sin_addr.s_addr = ip_string_to_binary(
      get_ip_from_args(argc, argv).c_str()); // default to 127.0.0.1 if no arg

  if (connect_socket(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) <
      0) {
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
  // create shadow mask texture
  RenderTexture2D shadowMask = LoadRenderTexture(window_size.x, window_size.y);
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

  RenderTexture2D darknessMask =
      LoadRenderTexture(window_size.x, window_size.y);
  Image lightImg = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLANK);
  lightTex = LoadTextureFromImage(lightImg);
  UnloadImage(lightImg);

  // Initialize map objects after resource manager
  init_map_objects(res_man.getTex("assets/barrel.png"),
                   res_man.getTex("assets/charger.png"));

  std::cout << "Map objects initialized, count: " << objects.size()
            << std::endl;
  for (const auto &obj : objects) {
    std::cout << "Object type: " << (int)obj.type << ", Position: ("
              << obj.bounds.x << ", " << obj.bounds.y << ")" << std::endl;
  }

  while (!WindowShouldClose() && running) {
    // calculate zoom based on actual window size relative to normal window size
    float widthRatio = (float)GetScreenWidth() / window_size.x;
    float heightRatio = (float)GetScreenHeight() / window_size.y;
    float scale = (widthRatio < heightRatio) ? widthRatio : heightRatio;

    handle_packets(&game, &my_id, &res_man);

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
    acid_rain.update(GetFrameTime(), game.players);

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

    can_move_state = update_can_move_state(Rectangle{(float)game.players.at(my_id).x, (float)game.players.at(my_id).y, (float)PLAYER_SIZE, (float)PLAYER_SIZE}, cubes, PLAYER_SIZE, 0.1f, Rectangle{0, 0, (float)PLAYING_AREA.width, (float)PLAYING_AREA.height});

    bool moved = game.players.at(my_id).move(can_move_state);

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
    } else if (game.players[my_id].weapon_id == Weapon::umbrella && umbrella_update_data.is_shooting) {
      moved_gun = move_umbrella(&game.players[my_id].rot, cx, cy, cam, scale,
                                offsetX, offsetY);
    }
    hasmoved = moved || moved_gun;

    if (server_update_counter >= 5 && hasmoved) {
      std::string msg = netvent::serialize_to_netvent(
          netvent::val((int)MSG_PLAYER_MOVE),
          std::map<std::string, netvent::Value>(
              {{"x", netvent::val(game.players.at(my_id).x)},
               {"y", netvent::val(game.players.at(my_id).y)},
               {"rot", netvent::val(game.players.at(my_id).rot)}}));
      send_message(msg, sock);

      server_update_counter = 0;
    }

    game.update(my_id, cam);

    for (RainDrop &drop : game.raindrops) {
      if (drop.rot != 0) {
        drop.position.x += cosf(drop.rot) * drop.speed * GetFrameTime();
        drop.position.y += sinf(drop.rot) * drop.speed * GetFrameTime();
      } else {
        drop.position.y += drop.speed * GetFrameTime(); // falling straight down
      }
      
      // Fade out over time
      drop.alpha = std::max(0.0f, drop.alpha - GetFrameTime() * 0.5f);
    }

    if (!canshoot)
      bdelay--;
    if (!canshoot && bdelay == 0) {
      canshoot = true;
    }

    if (IsMouseButtonDown(0) && canshoot &&
        game.players[my_id].weapon_id == 0 && !is_assassin) {
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

      // Send bullet shot message to server - server will assign ID
      send_message(
          netvent::serialize_to_netvent(
              netvent::val((int)MSG_BULLET_SHOT),
              std::map<std::string, netvent::Value>(
                  {{"player_id", netvent::val(my_id)},
                   {"x", netvent::val((int)spawnPos.x)},
                   {"y", netvent::val((int)spawnPos.y)},
                   {"rot", netvent::val(game.players[my_id].rot)}})),
          sock);
    }

    // flashlight battery
    {
      if (game.players[my_id].weapon_id == Weapon::flashlight &&
          flashlight_usable) {
        float deltaTime = GetFrameTime();
        flashlight_time_left -= deltaTime;

        if (flashlight_time_left <= 0.0f) {
          flashlight_time_left = 0.0f;
          flashlight_usable = false;
          switch_weapon(Weapon::gun_or_knife, &game, my_id, sock,
                        flashlight_usable);
          std::cout << "Flashlight battery depleted!" << std::endl;
        }
      }

      // check charger coil
      if (check_charging_station_collision(game.players[my_id].x,
                                           game.players[my_id].y)) {
        flashlight_usable = true;
        flashlight_time_left = 15.0f; // Reset to full battery
      }
    }

    // detect weapon switching with keyboard keys
    {
      bool weapon_changed = false;
      Weapon currentWeapon = (Weapon)game.players[my_id].weapon_id;

      if (IsKeyPressed(KEY_ONE)) {
        currentWeapon = Weapon::gun_or_knife;
        weapon_changed = true;
      } else if (IsKeyPressed(KEY_TWO) && flashlight_usable) {
        currentWeapon = Weapon::flashlight;
        weapon_changed = true;
      } else if (IsKeyPressed(KEY_THREE) && umbrella_update_data.is_usable) {
        currentWeapon = Weapon::umbrella;
        weapon_changed = true;
      }

      if (weapon_changed) {
        switch_weapon(currentWeapon, &game, my_id, sock, flashlight_usable);
      }
    }

    // update umbrella
    {
      Rectangle player_rect = {(float)game.players[my_id].x,
                               (float)game.players[my_id].y, (float)PLAYER_SIZE,
                               (float)PLAYER_SIZE};
      std::vector<Rectangle> bullets_rects;
      std::vector<Bullet> active_bullets;

      for (Bullet &b : game.bullets) {
        bullets_rects.push_back(
            Rectangle{(float)b.x, (float)b.y, (float)b.r * 2, (float)b.r * 2});
        active_bullets.push_back(b);
      }

      // check if player is near barrel
      for (auto &obj : objects) {
        if (obj.type == ObjectType::Barrel) {
          bool near_barrel = obj.check_collision(player_rect);
          bool is_umbrella_equipped =
              game.players[my_id].weapon_id == (int)Weapon::umbrella;

          umbrella_update_data =
              player_umbrella.update(obj.bounds, player_rect, bullets_rects,
                                     active_bullets, is_umbrella_equipped, game.players[my_id].rot, acid_rain.is_active());

          break;
        }
      }

      if (!umbrella_update_data.is_usable &&
          game.players[my_id].weapon_id == (int)Weapon::umbrella) {
        switch_weapon(Weapon::gun_or_knife, &game, my_id, sock,
                      flashlight_usable);
      }


      if (umbrella_update_data.is_shooting && !umbrella_update_data_last_frame.is_shooting) {
        game.players[my_id].is_shooting = true;
        
        // tell the server we are shooting the umbrella
        std::string msg = netvent::serialize_to_netvent(
            netvent::val((int)MSG_UMBRELLA_SHOOT),
            std::map<std::string, netvent::Value>({{"player_id", netvent::val(my_id)}, {"rot", netvent::val(game.players[my_id].rot)}}));
        send_message(msg, sock);
      }

      if (!umbrella_update_data.is_shooting && umbrella_update_data_last_frame.is_shooting) {
        game.players[my_id].is_shooting = false;
        
        // tell the server we are not shooting the umbrella
        std::string msg = netvent::serialize_to_netvent(
            netvent::val((int)MSG_UMBRELLA_STOP),
            std::map<std::string, netvent::Value>({{"player_id", netvent::val(my_id)}}));
        send_message(msg, sock);
      }
      umbrella_update_data_last_frame = umbrella_update_data;
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

    // draw charging stations
    for (int i = 0; i < 4; i++) {
      if (isInViewport(charging_points[i].x, charging_points[i].y, CHARGE_SIZE,
                       CHARGE_SIZE, cam)) {
        DrawTexturePro(res_man.getTex("assets/charger.png"), {0, 0, 16, 16},
                       {(float)charging_points[i].x,
                        (float)charging_points[i].y, (float)CHARGE_SIZE,
                        (float)CHARGE_SIZE},
                       {0, 0}, 0.0f, WHITE);
      }
    }

    // draw cubes
    cube_loop(cubes, cam, &res_man);

    // draw umbrella barrel
    // if any player is touching the barrel, tint it green
    Color barrel_tint = WHITE;
    for (const auto &[_, p] : game.players) {
      Rectangle player_rect = {(float)p.x, (float)p.y, (float)PLAYER_SIZE,
                               (float)PLAYER_SIZE};
      if (CheckCollisionRecs(umbrella_barrel, player_rect)) {
        barrel_tint = GREEN;
        break;
      }
    }

    Color shadow_color = {0, 0, 0, 80};
    float shadow_width = 80;
    float shadow_height = 30;
    float shadow_y_offset = 30;
    DrawEllipse(umbrella_barrel.x + BARREL_SIZE, 
                umbrella_barrel.y + BARREL_COLLISION_SIZE - shadow_y_offset,
                shadow_width/2, shadow_height/2, shadow_color);

    DrawTexturePro(res_man.getTex("assets/barrel.png"), {0, 0, 16, 16},
                   {umbrella_barrel.x + BARREL_SIZE / 2,
                    umbrella_barrel.y + BARREL_SIZE / 2, BARREL_SIZE,
                    BARREL_SIZE},
                   {0, 0}, 0.0f, barrel_tint);

    draw_players(game.players, game.bullets, &res_man, my_id);

    for (Bullet &b : game.bullets)
      b.show();

    for (const RainDrop &drop : game.raindrops) {
      Color dropColor = {0, 255, 0, (unsigned char)(drop.alpha * 255)}; // Green with alpha
      DrawCircle(drop.position.x, drop.position.y, drop.size, dropColor);
      
      // Draw trail based on movement direction
      float trailLength = drop.speed * 0.05f;
      Vector2 trailEnd;
      
      if (drop.rot != 0) {
        // Shot raindrop - trail follows shot direction
        trailEnd = {
          drop.position.x - cosf(drop.rot) * trailLength,
          drop.position.y - sinf(drop.rot) * trailLength
        };
      } else {
        // Normal raindrop - trail goes straight up
        trailEnd = {drop.position.x, drop.position.y - trailLength};
      }
      
      Color trailColor = {0, 255, 0, (unsigned char)(drop.alpha * 128)}; // More transparent trail
      DrawLineEx(drop.position, trailEnd, drop.size * 0.5f, trailColor);
    }

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
        Vector2 playerScreenPos =
            GetWorldToScreen2D(Vector2{(float)p.x + 50, (float)p.y + 50}, cam);

        if (id == my_id || p.weapon_id == Weapon::flashlight) {
          DrawTexture(lightTex, playerScreenPos.x - lightTex.width / 2,
                      playerScreenPos.y - lightTex.height / 2, WHITE);
        }

        // Draw flashlight beam if player has flashlight
        if (p.weapon_id == Weapon::flashlight) {
          float angleRad =
              (-p.rot + 5 + 180) *
              DEG2RAD; // Add 180 degrees to point in the right direction
          float flashlight_distance = 600.0f; // How far the light reaches

          float spread_angle = 15.0f * DEG2RAD; // Narrower beam

          Vector2 playerWorldPos = GetScreenToWorld2D(playerScreenPos, cam);

          for (int i = 0; i < 4; i++) {
            float current_spread =
                spread_angle *
                (1.0f - (float)i / 4.0f); // Narrower spread for inner beams
            float alpha = 180 - (i * 40); // Brighter in the center

            // Calculate ray directions for left and right edges of the beam
            Vector2 left_direction = {cosf(angleRad - current_spread), -sinf(angleRad - current_spread)};
            Vector2 right_direction = {cosf(angleRad + current_spread), -sinf(angleRad + current_spread)};
            
            // Check for cube intersections and limit beam distance
            float left_distance = check_ray_cube_intersection(playerWorldPos, left_direction, flashlight_distance, cubes);
            float right_distance = check_ray_cube_intersection(playerWorldPos, right_direction, flashlight_distance, cubes);
            
            // Convert back to screen coordinates
            Vector2 beam_left = {
                playerScreenPos.x + cosf(angleRad - current_spread) * left_distance,
                playerScreenPos.y - sinf(angleRad - current_spread) * left_distance};

            Vector2 beam_right = {
                playerScreenPos.x + cosf(angleRad + current_spread) * right_distance,
                playerScreenPos.y - sinf(angleRad + current_spread) * right_distance};

            // Draw layered beams
            Color beam_color = {255, 255, 255, (unsigned char)alpha};
            DrawTriangle(playerScreenPos, beam_left, beam_right, beam_color);
          }

          // Add a small intense light at the source
          float source_size = 30.0f;
          Color source_color = {255, 255, 255, 200};
          DrawCircle(playerScreenPos.x, playerScreenPos.y, source_size,
                     source_color);
        }
      }
      EndTextureMode();

      // Apply darkness mask to main render target
      BeginTextureMode(target);
      BeginBlendMode(BLEND_MULTIPLIED);

      Rectangle source = {0, 0, (float)darknessMask.texture.width,
                          (float)-darknessMask.texture.height};
      Rectangle dest = {0, 0, (float)window_size.x, (float)window_size.y};
      DrawTexturePro(darknessMask.texture, source, dest, Vector2{0, 0}, 0.0f,
                     WHITE);

      EndBlendMode();
      EndTextureMode();
    }

    // show green tint on screen if acid rain is active and player doesn't have
    // umbrella
    if (acid_rain.is_active()) {
      BeginTextureMode(target);
      BeginBlendMode(BLEND_ADDITIVE);

      bool has_active_umbrella =
          game.players[my_id].weapon_id == (int)Weapon::umbrella &&
          player_umbrella.is_active;
      Color tint = has_active_umbrella
                       ? Color{0, 0, 0, 0}
                       : Color{0, 40, 0, 80}; // Subtle green overlay

      DrawRectangle(0, 0, window_size.x, window_size.y, tint);

      EndBlendMode();
      EndTextureMode();
    }

    // Draw all HUD elements on top (after darkness effect)
    BeginTextureMode(target);
    draw_ui(my_true_color, game.players, game.bullets, my_id, (20 - bdelay),
            cam, scale);
    EndTextureMode();

    // Apply shadow casting (line of sight) effect
    BeginTextureMode(shadowMask);
    ClearBackground(WHITE); // Start with everything visible
    
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
  UnloadRenderTexture(shadowMask);
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

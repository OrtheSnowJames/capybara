#include "constants.hpp"
#include "game.hpp"
#include "math.h"
#include "player.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <raylib.h>
#include <raymath.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <random>
#include <chrono>
#include <csignal>
#include <set>
#include <atomic>

static int server_socket_fd = -1;
std::atomic<bool> server_running{true};

std::mutex game_mutex;
Game game;

std::mutex assassin_mutex;
int assassin_id = -1;
int assassin_target_id = -1; // Track the current target
Color original_assassin_color;
std::chrono::steady_clock::time_point assassin_start_time;
std::set<int> used_assassin_ids; // Track players who have already been assassins

typedef std::list<std::pair<int, std::string>> packetlist;

std::mutex packets_mutex;
packetlist packets;

std::mutex clients_mutex;
std::unordered_map<int, client> clients;

std::mutex running_mutex;
std::map<int, bool> is_running;

void perform_shutdown() {
  // try to exit gracefully
  try {
    if (server_socket_fd != -1) {
      std::cout << "Closing server socket..." << std::endl;
      shutdown(server_socket_fd, SHUT_RDWR);
      close(server_socket_fd);
    }

    // Clear any pending packets first
    {
      std::lock_guard<std::mutex> packets_lock(packets_mutex);
      packets.clear();
    }

    // stop existing clients
    {
      std::scoped_lock lock(clients_mutex);
      for (auto& [id, client_pair] : clients) {
        if (client_pair.second && client_pair.second->joinable()) {
          shutdown(client_pair.first, SHUT_RDWR);
          close(client_pair.first);
          client_pair.second->join();
        }
      }
      clients.clear();
    }
    
    std::cout << "Attempting graceful shutdown..." << std::endl;
    exit(0);
  } catch (const std::exception& e) {
    std::cerr << "Error during shutdown: " << e.what() << std::endl;
    exit(1);
  }
}

void shutdown_server(int signum) {
  std::cout << "\nSignal " << signum << " received. Shutting down." << std::endl;
  server_running = false;
  
  std::thread shutdown_thread(perform_shutdown);
  shutdown_thread.detach();
  
  std::thread force_exit([]() {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Force exit after timeout" << std::endl;
    _Exit(1);
  });
  force_exit.detach();
}

void handle_client(int client, int id) {
  try {
    {
      std::lock_guard<std::mutex> lock(game_mutex);
      Player p(100, 100);
      p.username = "unset";
      p.color = RED;
      game.players.insert({id, p});
    }

    std::ostringstream out;
    {
      // Use scoped_lock to acquire both mutexes atomically
      std::scoped_lock lock(game_mutex, assassin_mutex);

      for (auto &[k, v] : game.players) {
        // sanitize
        std::string safe_username = v.username;
        if (safe_username.empty())
          safe_username = "unset";
        // remove bad characters
        for (char &c : safe_username) {
          if (c == ';' || c == ':' || c == ' ')
            c = '_';
        }
        
        unsigned int color_code = color_to_uint(v.color);
        if (k == assassin_id) {
          color_code = color_to_uint(INVISIBLE);
        }
        
        out << ':' << k << ' ' << v.x << ' ' << v.y << ' ' << safe_username
            << ' ' << color_code << ' ' << v.weapon_id;
      }
    }

    // unlock mutex
    std::string payload = out.str();
    std::string msg = "0\n" + payload;
    send_message(msg, client);
  } catch (const std::exception& e) {
    std::cerr << "Client " << id << " error: " << e.what() << std::endl;
  }

  std::string msg_id = "1\n" + std::to_string(id);
  send_message(msg_id, client);

  std::cout << "Client " << msg_id << " has joined.\n";

  // If there's an active assassin, send the assassin event message to the new client
  {
    std::lock_guard<std::mutex> assassin_lock(assassin_mutex);
    if (assassin_id != -1 && assassin_target_id != -1) {
      std::ostringstream event_response;
      event_response << "15\n" << assassin_id << " " << assassin_target_id;
      send_message(event_response.str(), client);
      std::cout << "Sent assassin state to new client " << id << std::endl;
    }
  }

  std::ostringstream out;

  // sanitize username for consistency
  std::string safe_username = game.players.at(id).username;
  if (safe_username.empty())
    safe_username = "unset";
  // remove bad characters
  for (char &c : safe_username) {
    if (c == ';' || c == ':' || c == ' ')
      c = '_';
  }

  out << "3\n"
      << id << " " << game.players.at(id).x << " " << game.players.at(id).y
      << " " << safe_username << " "
      << color_to_uint(game.players.at(id).color) << " "
      << game.players.at(id).weapon_id;

  {
    std::lock_guard<std::mutex> clients_lock(clients_mutex);
    for (auto &pair : clients) {
      if (pair.first != id) {
        send_message(out.str(), pair.second.first);
      }
    }
  }

  bool running;
  {
    std::lock_guard<std::mutex> _(running_mutex);
    running = is_running[id];
  }

  while (running) {
    char buffer[1024];

    int received = recv(client, buffer, sizeof(buffer), 0);

    if (received <= 0)
      break;
    {
      std::lock_guard<std::mutex> _(running_mutex);
      running = is_running[id];
    }

    if (!running)
      break;

    {
      std::lock_guard<std::mutex> lock(packets_mutex);
      packets.push_front({
          id,
          std::string(buffer, received),
      });
    }
  }

  {
    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = false;
    std::lock_guard<std::mutex> _lock(game_mutex);
    game.players.erase(id);
    
    // Check if disconnected player was assassin
    std::lock_guard<std::mutex> assassin_lock(assassin_mutex);
    if (id == assassin_id) {
      std::cout << "Assassin (ID: " << id << ") disconnected. Ending assassin event." << std::endl;
      assassin_id = -1;
      assassin_target_id = -1;
    }
  }

  std::cout << "Client " << id << " disconnected.\n";
}

float normalize_rotation(float rot) {
    rot = fmod(rot, 360.0f); 

    if (rot < 0)
        rot += 360.0f;

    return rot;
}


// ---------------------------------
//  EVENTS
// ---------------------------------

// Check if assassin's knife is touching their target
bool check_assassin_collision(int assassin_id, int target_id, int assassin_x, int assassin_y, float assassin_rot) {
  if (assassin_id == -1 || target_id == -1) return false;

  assassin_rot = normalize_rotation(assassin_rot + 180.0f);

  // check if both players exist
  if (game.players.find(assassin_id) == game.players.end() || 
      game.players.find(target_id) == game.players.end()) {
    return false;
  }
  
  Player& target = game.players[target_id];
  
  // calculate knife position using same approach as client
  float knife_offset = 80.0f;
  float angle_rad = assassin_rot * DEG2RAD;
  
  // knife position extending from assassin center toward cursor direction
  float knife_x = assassin_x + 50 + cosf(angle_rad) * knife_offset;
  float knife_y = assassin_y + 50 + sinf(angle_rad) * knife_offset;
  
  // target center position
  float target_center_x = target.x + 50;
  float target_center_y = target.y + 50;
  float hitbox_radius = 50.0f; 
  
  float distance = sqrtf(powf(knife_x - target_center_x, 2) + powf(knife_y - target_center_y, 2));
  
  return distance <= hitbox_radius;
}

enum EventType {
  Darkness = 0,
  Assasin = 1
};

void make_player_assassin(int target_id) {
  // Always acquire locks in the same order: game_mutex first, then assassin_mutex
  std::lock_guard<std::mutex> game_lock(game_mutex);
  std::lock_guard<std::mutex> assassin_lock(assassin_mutex);
  
  if (assassin_id != -1) {
    std::cout << "Command failed: An assassin event is already active."
              << std::endl;
    return;
  }

  if (game.players.find(target_id) == game.players.end()) {
    std::cout << "Command failed: Player with ID " << target_id
              << " not found." << std::endl;
    return;
  }

  // Check if this player has already been an assassin
  if (used_assassin_ids.find(target_id) != used_assassin_ids.end()) {
    std::cout << "Player " << target_id << " has already been an assassin. Skipping." << std::endl;
    return;
  }

  // store assassin state
  assassin_id = target_id;
  original_assassin_color = game.players.at(target_id).color;
  assassin_start_time = std::chrono::steady_clock::now();
  used_assassin_ids.insert(target_id); // Mark this player as used

  std::cout << "Server: Storing original color for player " << target_id << " as " << color_to_string(original_assassin_color) << std::endl;

  // make player invisible
  Color old_color = game.players.at(target_id).color;
  game.players.at(target_id).color = INVISIBLE;
  
  std::cout << "Server: Player " << target_id << " color changed from " << color_to_string(old_color) << " to INVISIBLE (assassin mode)" << std::endl;

  // find a random target for the assassin
  std::vector<int> potential_targets;
  for (const auto& [player_id, player] : game.players) {
    if (player_id != target_id && !color_equal(player.color, INVISIBLE)) {
      potential_targets.push_back(player_id);
    }
  }
  
  if (!potential_targets.empty()) {
    int random_index = random_int(0, potential_targets.size() - 1);
    assassin_target_id = potential_targets[random_index];
    
    // send assassin event message to the assassin FIRST
    std::ostringstream event_response;
    event_response << "15\n" << target_id << " " << assassin_target_id;
    
    std::lock_guard<std::mutex> clients_lock(clients_mutex);
    auto assassin_client = clients.find(target_id);
    if (assassin_client != clients.end()) {
      send_message(event_response.str(), assassin_client->second.first);
    } else {
      std::cout << "Warning: Could not find socket for assassin " << target_id << std::endl;
    }
    
    std::cout << "Assassin " << target_id << " is targeting player " << assassin_target_id << std::endl;
  } else {
    std::cout << "No potential targets found for assassin " << target_id << std::endl;
  }

  // THEN send the color change message
  std::ostringstream response;
  response << "5\n" // MSG_PLAYER_UPDATE
           << target_id << " " << game.players.at(target_id).username << " "
           << color_to_uint(INVISIBLE);

  broadcast_message(response.str(), clients);
}

void summon_event(int delay) {
  if (delay <= 70) {
    return; // too short to summon an event/
  }

  EventType event = random_enum_element(EventType::Darkness, EventType::Assasin);

  switch (event) {
    case EventType::Darkness:
      break;
    case EventType::Assasin: {
      int target_id = -1;
      {
        std::lock_guard<std::mutex> game_lock(game_mutex);
        if (game.players.empty() || game.players.size() <= 2) {
          std::cout << "Not enough players to start an assassin event." << std::endl;
          break;
        }
        
        // If all players have been assassins, reset the used list
        if (used_assassin_ids.size() >= game.players.size()) {
          std::cout << "All players have been assassins. Resetting assassin pool." << std::endl;
          used_assassin_ids.clear();
        }
        
        // Find a player who hasn't been an assassin yet
        std::vector<int> available_players;
        for (const auto& player : game.players) {
          if (used_assassin_ids.find(player.first) == used_assassin_ids.end()) {
            available_players.push_back(player.first);
          }
        }
        
        if (!available_players.empty()) {
          int random_index = random_int(0, available_players.size() - 1);
          target_id = available_players[random_index];
        }
      }

      if (target_id != -1) {
        make_player_assassin(target_id);
      }
      break;
    }
  };
}

void event_worker() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 5 * 60 * 1000); // 0 to 5 min in ms

    while (true) {
        int delay = dist(rng); // pick when in the next 5 minutes to run

        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        summon_event(delay); // run at some point within the 5 min window

        int remaining = (5 * 60 * 1000) - delay;
        std::this_thread::sleep_for(std::chrono::milliseconds(remaining));
    }
}

// ---------------------------------
// END EVENTS
// ---------------------------------

void handle_stdin_commands() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    if (command == "assassin") {
      int target_id;
      if (!(iss >> target_id)) {
        std::cout << "Usage: assassin <player_id>" << std::endl;
        continue;
      }
      make_player_assassin(target_id);
    } else {
      std::cout << "Unknown command: " << command << std::endl;
    }
  }
}

void accept_clients(int sock) {
  while (server_running) {
    int client = accept(sock, nullptr, nullptr);
    if (!server_running) break;
    
    if (client < 0) {
      if (errno != EINTR) {  // Ignore interrupts
        perror("Accept failed");
      }
      continue;
    }
    
    int id = 0;
    while (server_running) {
      int id_init = id;
      {
        std::lock_guard<std::mutex> game_lock(game_mutex);
        for (auto &[k, v] : game.players)
          if (id == k)
            id++;
      }

      if (id_init == id)
        break;
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    if (server_running) {
      clients[id] = std::make_pair(
          client, std::make_shared<std::thread>(handle_client, client, id));

      std::lock_guard<std::mutex> _(running_mutex);
      is_running[id] = true;
    } else {
      close(client);
    }
  }
}

int main() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Failed to create socket");
    return -1;
  }
  server_socket_fd = sock;

  sockaddr_in sock_addr;
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(50000);
  sock_addr.sin_addr.s_addr = INADDR_ANY;


  int yes = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  try {
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)); // might not work on macos
  } catch (const std::exception& e) {
    std::cerr << "Error setting SO_REUSEPORT: " << e.what() << std::endl;
  }


  if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("Failed to bind socket");
    return -1;
  }

  if (listen(sock, 5) < 0) {
    perror("Failed to listen on socket");
    return -1;
  }

  std::thread(accept_clients, sock).detach();
  std::thread(event_worker).detach();
  std::thread(handle_stdin_commands).detach();

  std::cout << "Running.\n";

  std::signal(SIGINT, shutdown_server); // handle SIGINT

  while (server_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // terminate disconnected clients
    std::list<int> to_remove;
    {
      std::lock_guard<std::mutex> lo(running_mutex);
      if (!is_running.empty()) {
        for (auto &[id, v] : is_running) {
          if (!v) {
            to_remove.push_back(id);
          }
        }
      }
    }

    if (!to_remove.empty()) {
      std::lock_guard<std::mutex> a(clients_mutex);
      for (int i : to_remove) {
        try {
          auto client_it = clients.find(i);
          if (client_it == clients.end()) {
            continue;
          }
          
          int socket_fd = client_it->second.first;
          auto thread_ptr = client_it->second.second;
          
          if (thread_ptr && thread_ptr->joinable()) {
            thread_ptr->join();
          }

          if (socket_fd != -1) {
            shutdown(socket_fd, SHUT_RDWR);
            close(socket_fd);
          }

          clients.erase(client_it);

          std::this_thread::sleep_for(std::chrono::milliseconds(100));

          std::string out("4\n" + std::to_string(i));
          for (const auto& [client_id, client_data] : clients) {
            if (client_id != i && client_data.first != -1) {
              send_message(out, client_data.first);
            }
          }

          {
            std::lock_guard<std::mutex> b(game_mutex);
            game.players.erase(i);
          }

          {
            std::lock_guard<std::mutex> running_lock(running_mutex);
            is_running.erase(i);
          }

          std::cout << "Removed client " << i << std::endl;
        } catch (const std::exception& e) {
          std::cerr << "Error cleaning up client " << i << ": " << e.what() << std::endl;
        }
      }
    }

    // process packets
    {
      std::lock_guard<std::mutex> lock(packets_mutex);
      if (!packets.empty()) {
        packetlist current_packets;
        current_packets.swap(packets);
        
        for (const auto& [from_id, packet] : current_packets) {
          try {
            if (packet.empty()) continue;

            int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
            std::string payload = packet.substr(packet.find('\n') + 1);

            switch (packet_type) {
            case 2: {
              // get the movement data 
              std::istringstream j(payload);
              int x, y;
              float rot;
              j >> x >> y >> rot;
              
              bool collision_occurred = false;
              int current_assassin_id = -1;
              int current_target_id = -1;
              
              // check assassin collision first
              {
                std::lock_guard<std::mutex> assassin_lock(assassin_mutex);
                if (assassin_id == from_id && assassin_target_id != -1) {
                  current_assassin_id = assassin_id;
                  current_target_id = assassin_target_id;
                }
              }
              
              // update game state and check collision
              {
                std::lock_guard<std::mutex> z(game_mutex);
                if (game.players.find(from_id) == game.players.end())
                  break;
                game.players.at(from_id).x = x;
                game.players.at(from_id).y = y;
                game.players.at(from_id).rot = rot;
                
                // check if this player is an assassin
                if (current_assassin_id == from_id && current_target_id != -1) {
                  if (check_assassin_collision(current_assassin_id, current_target_id, x, y, rot)) {
                    collision_occurred = true;
                  }
                }
              }

              // handle assassination
              if (collision_occurred) {
                std::cout << "ASSASSIN SUCCESS! Player " << current_assassin_id << " hit target " << current_target_id << std::endl;
                // TODO: handle assassination
              }
              
              // Broadcast movement to other clients
              std::lock_guard<std::mutex> clients_lock(clients_mutex);
              for (const auto& [client_id, client_data] : clients) {
                if (client_id != from_id && client_data.first != -1) {
                  std::ostringstream out;
                  out << "2\n" << from_id << " " << payload;
                  send_message(out.str(), client_data.first);
                }
              }
            } break;
            case 5: { // MSG_PLAYER_UPDATE
              std::istringstream iss(payload);
              std::string username;
              unsigned int color_code;
              iss >> username >> color_code;

              if (iss.fail()) {
                std::cerr << "Invalid MSG_PLAYER_UPDATE from " << from_id << ": "
                          << payload << std::endl;
                break;
              }

              std::string sanitized_user = sanitize_username(username);

              {
                std::lock_guard<std::mutex> lock(game_mutex);
                game.players[from_id].username = sanitized_user;
                game.players[from_id].color = uint_to_color(color_code);
              }

              std::ostringstream response;
              response << "5\n"
                       << from_id << " " << sanitized_user << " " << color_code;
              broadcast_message(response.str(), clients, from_id);

            } break;
            case 6: {
              std::lock_guard<std::mutex> lock(game_mutex);
              unsigned int x = (unsigned int)std::stoi(payload);
              game.players[from_id].color = uint_to_color(x);
              std::lock_guard<std::mutex> clients_lock(clients_mutex);
              for (const auto& [client_id, client_data] : clients) {
                if (client_id != from_id && client_data.first != -1) {
                  send_message(std::string("6\n")
                                 .append(std::to_string(from_id))
                                 .append(" ")
                                 .append(payload),
                             client_data.first);
                }
              }
            } break;
            case 10: {
              std::lock_guard<std::mutex> lock(game_mutex);
              float rot = std::stof(payload);

              float angleRad = (-game.players[from_id].rot + 5) * DEG2RAD;
              float bspeed = 10;

              Vector2 dir =
                  Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);
              Vector2 spawnOffset =
                  Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -120);
              Vector2 origin = {(float)game.players[from_id].x + 50,
                                (float)game.players[from_id].y + 50};
              Vector2 spawnPos = Vector2Add(origin, spawnOffset);
              game.bullets.push_back(
                  Bullet((int)spawnPos.x, (int)spawnPos.y, dir, from_id));
              std::lock_guard<std::mutex> clients_lock(clients_mutex);
              std::ostringstream j;
              j << "10\n"
                << from_id << ' ' << (int)spawnPos.x << ' ' << (int)spawnPos.y
                << ' ' << rot;
              std::cout << j.str() << '\n';
              broadcast_message(j.str(), clients, from_id);
            } break;
            default:
              std::cerr << "INVALID PACKET TYPE: " << packet_type << std::endl;
              break;
            }
          } catch (const std::exception& e) {
            std::cerr << "Error processing packet: " << e.what() << std::endl;
          }
        }
      }
    }
  }

  // Wait a bit for threads to notice server_running is false
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return 0;
}

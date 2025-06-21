#include "constants.hpp"
#include "game.hpp"
#include "math.h"
#include "player.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
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

static int server_socket_fd = -1;

void shutdown_server(int signum) {
  std::cout << "\nSignal " << signum << " received. Shutting down." << std::endl;
  if (server_socket_fd != -1) {
    close(server_socket_fd);
  }
  exit(signum);
}

std::mutex game_mutex;
Game game;

typedef std::list<std::pair<int, std::string>> packetlist;

std::mutex packets_mutex;
packetlist packets;

std::mutex clients_mutex;
std::unordered_map<int, client> clients;

std::mutex running_mutex;
std::map<int, bool> is_running;

void handle_client(int client, int id) {
  {
    {
      std::lock_guard<std::mutex> lock(game_mutex);
      Player p(100, 100);
      p.username = "unset";
      p.color = RED;
      game.players.insert({id, p});
    }

    std::ostringstream out;
    {
      std::lock_guard<std::mutex> lock(game_mutex);

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
        // sanitize color
        uint col = color_to_uint(v.color);
        if (col > 4)
          col = 1;
        out << ':' << k << ' ' << v.x << ' ' << v.y << ' ' << safe_username
            << ' ' << col << ' ' << v.weapon_id;
      }
    }

    // unlock mutex
    std::string payload = out.str();

    std::string msg = "0\n" + payload;

    send_message(msg, client);
  }
  sleep(3);

  std::string msg_id = "1\n" + std::to_string(id);

  send_message(msg_id, client);

  std::cout << "Client " << msg_id << " has joined.\n";

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

  for (auto &pair : clients) {
    if (pair.first != id) {
      send_message(out.str(), pair.second.first);
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
  }

  std::cout << "Client " << id << " disconnected.\n";
}



// ---------------------------------
//  EVENTS
// ---------------------------------

enum EventType {
  Darkness = 0,
  Assasin = 1
};

void summon_event(int delay) {
  if (delay <= 60) {
    return; // too short to summon an event
  }

  EventType event = random_enum_element(EventType::Darkness, EventType::Assasin);

  switch (event) {
    case EventType::Darkness:
      break;
    case EventType::Assasin:
      break;
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

void accept_clients(int sock) {
  while (true) {
    int client = accept(sock, nullptr, nullptr);
    int id = 0;
    while (1) {
      int id_init = id;
      for (auto &[k, v] : game.players)
        if (id == k)
          id++;

      if (id_init == id)
        break;
    }

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[id] = std::make_pair(
        client, std::make_shared<std::thread>(handle_client, client, id));

    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = true;
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

  if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("Failed to bind socket");
    return -1;
  }

  if (listen(sock, 5) < 0) {
    perror("Failed to listen on socket");
    return -1;
  }

  std::thread(accept_clients, sock).detach();

  std::cout << "Running.\n";

  std::signal(SIGINT, shutdown_server); // handle SIGINT
  // handle game stuff
  while (1) {
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> lo(running_mutex);

      std::list<int> to_remove;
      for (auto &[id, v] : is_running) {
        if (!v) {
          to_remove.push_back(id);
        }
      }
      std::lock_guard<std::mutex> a(clients_mutex);
      for (int i : to_remove) {
        if (clients[i].second.get()->joinable())
          clients[i].second.get()->join();

        shutdown(clients[i].first, SHUT_RDWR);

        close(clients[i].first);
        is_running.erase(i);
        clients.erase(i);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::string out("4\n" + std::to_string(i));
        for (auto &pair : clients) {
          if (pair.first != i) {
            send_message(out, pair.second.first);
          } else {
          }
        }

        {
          std::lock_guard<std::mutex> b(game_mutex);
          game.players.erase(i);
        }
        std::cout << "Removed client " << i << std::endl;
      }
    }

    // ---------------------------------

    {
      std::lock_guard<std::mutex> lock(packets_mutex);

      for (auto &[from_id, packet] : packets) {

        int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
        std::string payload = packet.substr(packet.find('\n') + 1);

        switch (packet_type) {
        case 2: {
          std::lock_guard<std::mutex> z(game_mutex);
          std::istringstream j(payload);
          int x, y;
          float rot;
          j >> x >> y >> rot;
          if (game.players.find(from_id) == game.players.end())
            break;
          game.players.at(from_id).x = x;
          game.players.at(from_id).y = y;
          game.players.at(from_id).rot = rot;
        }
          for (auto &pair : clients) {
            if (pair.first != from_id) {
              std::ostringstream out;
              out << "2\n" << from_id << " " << payload;
              send_message(out.str(), pair.second.first);
            }
          }
          break;
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
          std::lock_guard<std::mutex> lokc(clients_mutex);
          for (auto &[k, v] : clients) {
            if (k == from_id)
              continue;
            send_message(std::string("6\n")
                             .append(std::to_string(from_id))
                             .append(" ")
                             .append(payload),
                         v.first);
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
          std::lock_guard<std::mutex> c_lock(clients_mutex);
          std::ostringstream j;
          j << "10\n"
            << from_id << ' ' << (int)spawnPos.x << ' ' << (int)spawnPos.y
            << ' ' << rot;
          std::cout << j.str() << '\n';
          broadcast_message(j.str(), clients, from_id);
          break;
        }
        default:
          std::cerr << "INVALID PACKET TYPE: " << packet_type << std::endl;
          break;
        }
      }
      packets.clear();
    }
  }
  close(sock);
}

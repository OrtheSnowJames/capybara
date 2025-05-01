#include "player.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
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
#include <utility>

std::mutex players_mutex;
std::unordered_map<int, Player> players;

std::mutex packets_mutex;
std::list<std::pair<int, std::string>> packets;

std::mutex clients_mutex;
std::unordered_map<int, std::pair<int, std::thread>> clients;

std::mutex running_mutex;
std::unordered_map<int, bool> is_running;

void handle_client(int client, int id) {
  std::ostringstream out;
  {
    std::lock_guard<std::mutex> lock(players_mutex);
    for (auto &[k, v] : players)
      out << ':' << k << ' ' << v.x << ' ' << v.y;
  } // unlock mutex
  std::string payload = out.str();

  std::string msg = "0\n" + payload;

  send_message(msg, client);

  std::string msg_id = "1\n" + std::to_string(id);

  send_message(msg_id, client);

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
      packets.push_back({
          id,
          std::string(buffer, received),
      });
    }
  }

  {
    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = false;
    std::lock_guard<std::mutex> _lock(players_mutex);
    players.erase(id);
  }

  std::cout << "Client " << id << " disconnected.\n";
}

void accept_clients(int sock) {
  while (true) {
    int client = accept(sock, nullptr, nullptr);
    int id = 0;
    while (1) {
      int id_init = id;
      for (auto &[k, v] : players)
        if (id == k)
          id++;

      if (id_init == id)
        break;
    }

    std::cout << "Assigned id " << id << " to new player.\n";
    players.insert({id, Player(100, 100)});

    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[id] = {client, std::thread(handle_client, client, id)};

    std::lock_guard<std::mutex> _(running_mutex);
    is_running[id] = true;

    std::cout << "Added client " << id << " to lists.\n";
  }
}

int main() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Failed to create socket");
    return -1;
  }

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

  std::unordered_map<int, std::thread> client_threads;

  std::thread(accept_clients, sock).detach();

  std::cout << "Running.\n";

  // handle game stuff
  while (1) {
    {
      std::lock_guard<std::mutex> l(clients_mutex);
      std::lock_guard<std::mutex> lo(running_mutex);

      std::list<int> to_remove;
      for (auto &[id, v] : is_running) {
        if (!v) {
          to_remove.push_back(id);
        }
      }

      for (int i : to_remove) {
        if (clients[i].second.joinable())
          clients[i].second.join();

        shutdown(clients[i].first, SHUT_RDWR);

        close(clients[i].first);
        std::cout << "Erased socket for client " << i << std::endl;
        is_running.erase(i);
        std::cout << "Erased client " << i << " from running list.\n";
        clients.erase(i);
        std::cout << "Erased client " << i << " from clients list.\n";

        std::cout << "Removed client " << i << std::endl;
      }
    }

    std::lock_guard<std::mutex> lock(packets_mutex);

    while (!packets.empty()) {
      auto &[from_id, packet] = packets.front();
      std::cout << "Packet from: " << from_id << std::endl
                << "Contents: " << packet << std::endl;

      packets.pop_front();
    }
  }

  close(sock);
}

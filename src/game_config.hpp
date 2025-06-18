#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

struct GameConfig {
public:
  std::string username;
  int colorindex;

  void load() {
    if (std::filesystem::exists("data/game_conf")) {
      std::ifstream a("data/game_conf");
      std::string b;
      std::string c;
      std::getline(a, b);
      std::getline(a, c);

      std::cout << b << ' ' << c << '\n';
      a.close();

      this->username = b;
      this->colorindex = std::stoi(c);
      return;
    }

    std::ofstream d("data/game_conf");
    d << "\n0";
    d.close();
    this->username = std::string("");
    this->colorindex = 0;
  };

  void save() {
    std::ofstream d("data/game_conf");
    d << this->username << "\n" << this->colorindex;
    d.close();

    std::cout << "saved\n";
  }
};

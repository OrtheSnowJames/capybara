#pragma once
#include "bullet.hpp"
#include "constants.hpp"
#include "raylib.h"
#include "utils.hpp"
#include <algorithm>
#include <vector>


class Game {
public:
  playermap players;
  std::vector<Bullet> bullets;

  void update_bullets(Camera2D cam, int my_id) {
    for (Bullet &b : this->bullets) {
      b.move();
    }
  }

  void update_players(int skip) {
    for (auto &[k, v] : this->players) {
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

  void update(int skip, Camera2D cam) {
    this->update_players(skip);
    this->update_bullets(cam, skip);
  }
};

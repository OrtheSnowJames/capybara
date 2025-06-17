#pragma once
#include "bullet.hpp"
#include "raylib.h"
#include "utils.hpp"

struct Game {
  playermap players;
  std::vector<Bullet> bullets;
};

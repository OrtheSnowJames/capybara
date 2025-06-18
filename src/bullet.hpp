#ifndef BULLET_HPP
#define BULLET_HPP

#include "raylib.h"

class Bullet {
public:
  int x, y;
  Vector2 vel;

  Bullet(int x, int y, Vector2 vel) : x(x), y(y), vel(vel) {}

  Bullet() : x(0), y(0), vel((Vector2){0, 0}) {}

  void move() {
    x += vel.x;
    y += vel.y;
  }

  void show() { DrawCircle(x, y, 10.0f, GRAY); }
};

#endif

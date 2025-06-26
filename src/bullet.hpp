#ifndef BULLET_HPP
#define BULLET_HPP

#include "raylib.h"

class Bullet {
public:
  int x, y, shotby_id;
  int bullet_id;
  float r = 10.0f;
  Vector2 vel;
  static int next_bullet_id;

  Bullet(int x, int y, Vector2 vel, int from_id)
      : x(x), y(y), vel(vel), shotby_id(from_id) {
    bullet_id = next_bullet_id;
    next_bullet_id = (next_bullet_id + 1) % 10000;
  }

  Bullet() : x(0), y(0), vel(Vector2{0, 0}), bullet_id(-1) {}

  void move() {
    x += vel.x;
    y += vel.y;
  }

  void show() { DrawCircle(x, y, r, GRAY); }
};

int Bullet::next_bullet_id = 0;

#endif

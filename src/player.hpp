#ifndef PLAYER_H
#define PLAYER_H
#include <raylib.h>
#include <string>

enum Weapon {
  gun_or_knife = 0,
  flashlight = 1,
  umbrella = 2
};

struct Player {
public:
  int x = 0;
  int y = 0;
  int nx = 0;
  int ny = 0;
  int speed = 2;
  Color color = RED;
  std::string username = std::string("unset");
  float rot = 0;
  int weapon_id = 0;  // 0 = gun or knife (default), 1 = flashlight, 2 = umbrella

  Player(int x, int y) : x(x), y(y), nx(x), ny(y) {}

  Player() : x(100), y(100), nx(100), ny(100) {};

  bool move() {
    bool out = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
               IsKeyDown(KEY_D);
    int dir_x = 0;
    int dir_y = 0;

    if (IsKeyDown(KEY_W))
      dir_y -= speed;
    if (IsKeyDown(KEY_S))
      dir_y += speed;
    if (IsKeyDown(KEY_A))
      dir_x -= speed;
    if (IsKeyDown(KEY_D))
      dir_x += speed;

    // Check boundaries before moving
    int new_x = this->x + dir_x;
    int new_y = this->y + dir_y;
    
    if (new_x >= PLAYING_AREA.x && new_x <= PLAYING_AREA.width - 100) {
      this->x = new_x;
    }
    if (new_y >= PLAYING_AREA.y && new_y <= PLAYING_AREA.height - 100) {
      this->y = new_y;
    }

    return out;
  }
};

#endif

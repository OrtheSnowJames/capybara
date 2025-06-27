#ifndef PLAYER_H
#define PLAYER_H
#include <raylib.h>
#include <iostream>
#include <string>
#include "netvent.hpp"
#include "clrfn.hpp"
#include "collision.hpp"

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

  Player(netvent::Table tbl) {
    std::cout << "Player(netvent::Table tbl)" << std::endl;
    x = tbl[netvent::val("x")].as_int();
    y = tbl[netvent::val("y")].as_int();
    nx = x;
    ny = y;
    username = tbl[netvent::val("username")].as_string();
    weapon_id = tbl[netvent::val("weapon_id")].as_int();
    rot = tbl[netvent::val("rot")].as_float();
    color = color_from_table(tbl[netvent::val("color")].as_table());
  }

  netvent::Table to_table(int id) {
    return netvent::map_table({
      {"id", netvent::val(id)},
      {"x", netvent::val(this->x)},
      {"y", netvent::val(this->y)},
      {"username", netvent::val(this->username)},
      {"weapon_id", netvent::val(this->weapon_id)},
      {"rot", netvent::val(this->rot)},
      {"color", netvent::val(color_to_table(this->color))}
    });
  }

  bool move(CanMoveState can_move_state) {
    bool out = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
               IsKeyDown(KEY_D);
    int dir_x = 0;
    int dir_y = 0;

    if (IsKeyDown(KEY_W) && can_move_state.up)
      dir_y -= speed;
    if (IsKeyDown(KEY_S) && can_move_state.down)
      dir_y += speed;
    if (IsKeyDown(KEY_A) && can_move_state.left)
      dir_x -= speed;
    if (IsKeyDown(KEY_D) && can_move_state.right)
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

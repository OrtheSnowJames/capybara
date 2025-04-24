#ifndef PLAYER_H
#define PLAYER_H

struct Player {
public:
  int x = 0;
  int y = 0;

  Player(int x, int y) {
    this->x = x;
    this->y = y;
  }
};

#endif

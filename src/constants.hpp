#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <raylib.h>

const int TILE_SIZE = 100;
const int CUBE_SIZE = 100;
const int UMBRELLA_HIT_LIMIT = 2;
const int PLAYING_AREA_TILES = 10;
const Rectangle PLAYING_AREA = {0, 0, TILE_SIZE *PLAYING_AREA_TILES,
                                TILE_SIZE *PLAYING_AREA_TILES};

#endif

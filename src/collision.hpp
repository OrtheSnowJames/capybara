#pragma once
#include "constants.hpp"
#include "player.hpp"
#include "objects.hpp"
#include <raylib.h>

struct CanMoveState {
    bool up = true;
    bool down = true;
    bool left = true;
    bool right = true;
};


inline CanMoveState update_can_move_state(Rectangle player, std::vector<Object> cubes, const int PLAYER_SIZE = 50, const float move_amount = 1.0f, Rectangle playing_area = {0, 0, 800, 600})
{
    // check if when the player moves in a certain direction, they will hit a cube or the edge of the screen
    CanMoveState new_can_move_state = {true, true, true, true};
    
    // Check cube collisions
    for (auto &cube : cubes) {
        const Rectangle cube_correct_bounds = {cube.bounds.x, cube.bounds.y, cube.bounds.height, cube.bounds.height};

        // up
        if (CheckCollisionRecs(Rectangle{player.x, player.y - move_amount, (float)PLAYER_SIZE, (float)PLAYER_SIZE}, cube_correct_bounds)) {
            new_can_move_state.up = false;
        }
        // down
        if (CheckCollisionRecs(Rectangle{player.x, player.y + move_amount, (float)PLAYER_SIZE, (float)PLAYER_SIZE}, cube_correct_bounds)) {
            new_can_move_state.down = false;
        }
        // left
        if (CheckCollisionRecs(Rectangle{player.x - move_amount, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE}, cube_correct_bounds)) {
            new_can_move_state.left = false;
        }
        // right
        if (CheckCollisionRecs(Rectangle{player.x + move_amount, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE}, cube_correct_bounds)) {
            new_can_move_state.right = false;
        }
    }

    // Check boundary collisions - prevent moving outside the playing area
    if (player.x - move_amount < playing_area.x) {
        new_can_move_state.left = false;
    }
    if (player.x + PLAYER_SIZE + move_amount > playing_area.x + playing_area.width) {
        new_can_move_state.right = false;
    }
    if (player.y - move_amount < playing_area.y) {
        new_can_move_state.up = false;
    }
    if (player.y + PLAYER_SIZE + move_amount > playing_area.y + playing_area.height) {
        new_can_move_state.down = false;
    }

    // TEMPORARILY DISABLED: This might be causing collision issues
    // if (!new_can_move_state.up && !new_can_move_state.down && !new_can_move_state.left && !new_can_move_state.right) {
    //     new_can_move_state.up = true;
    //     new_can_move_state.down = true;
    //     new_can_move_state.left = true;
    //     new_can_move_state.right = true;
    // }
    
    return new_can_move_state;
}
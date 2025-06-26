#pragma once

#include <raylib.h>
#include <vector>
#include "constants.hpp"

enum class ObjectType {
    Generic,
    Barrel,
    Charger,
    Cube
};

class Object {
    public:
        Rectangle bounds;
        Color color;
        Color tint;
        Texture2D texture;
        ObjectType type;
        bool is_active;

        Object(Rectangle bounds, Color color, ObjectType type = ObjectType::Generic) {
            this->bounds = bounds;
            this->color = color;
            this->tint = WHITE;
            this->type = type;
            this->is_active = false;
        }

        Object(Rectangle bounds, Texture2D texture, ObjectType type = ObjectType::Generic) {
            this->bounds = bounds;
            this->texture = texture;
            this->tint = WHITE;
            this->type = type;
            this->is_active = false;
        }

        void draw() {
            switch(type) {
                case ObjectType::Barrel:
                case ObjectType::Charger:
                    DrawTexturePro(
                        texture,
                        {0, 0, 16, 16},
                        {bounds.x + bounds.width/2, bounds.y + bounds.height/2, bounds.width, bounds.height},
                        {0, 0},
                        0.0f,
                        tint
                    );
                    break;
                default:
                    DrawRectangleRec(bounds, color);
                    break;
            }
        }

        bool check_collision(Rectangle other) {
            return CheckCollisionRecs(bounds, other);
        }

        void set_active(bool active) {
            is_active = active;
            tint = active ? GREEN : WHITE;
        }
};

std::vector<Object> objects;

std::vector<Object> get_rand_cubes(int divide_how_many_can_fit_by, int cube_size) {
    std::vector<Object> cubes;

    // find how many cubes can fit in the playing area
    int cubes_can_fit = (PLAYING_AREA.width / divide_how_many_can_fit_by) * (PLAYING_AREA.height / divide_how_many_can_fit_by);

    // get a random number of cubes to spawn
    int num_cubes_to_spawn = GetRandomValue(1, cubes_can_fit);

    // spawn the cubes
    for (int i = 0; i < num_cubes_to_spawn; i++) {
        // get random color
        Color color = {
            (unsigned char)GetRandomValue(0, 255),
            (unsigned char)GetRandomValue(0, 255),
            (unsigned char)GetRandomValue(0, 255),
            255
        };
        cubes.push_back(Object(Rectangle{(float)GetRandomValue(0, PLAYING_AREA.width - (cube_size * 2)), (float)GetRandomValue(0, PLAYING_AREA.height - (cube_size * 2)), (float)cube_size, (float)cube_size}, color, ObjectType::Cube));
    }
    return cubes;
}

void init_map_objects(Texture2D barrel_texture, Texture2D charger_texture) {
    objects.clear();
    
    // Add barrel in center
    const int BARREL_SIZE = 50;
    const int BARREL_COLLISION_SIZE = BARREL_SIZE * 2;
    objects.push_back(Object(
        {
            (PLAYING_AREA.width / 2) - (BARREL_COLLISION_SIZE / 2),
            (PLAYING_AREA.height / 2) - (BARREL_COLLISION_SIZE / 2),
            BARREL_COLLISION_SIZE,
            BARREL_COLLISION_SIZE
        },
        barrel_texture,
        ObjectType::Barrel
    ));

    // Add charging stations
    const int CHARGE_SIZE = 64;
    const int CHARGE_OFFSET = 32;
    
    // Left charger
    objects.push_back(Object(
        {
            CHARGE_OFFSET,
            (PLAYING_AREA.height / 2) - CHARGE_OFFSET,
            CHARGE_SIZE,
            CHARGE_SIZE
        },
        charger_texture,
        ObjectType::Charger
    ));

    // Right charger
    objects.push_back(Object(
        {
            PLAYING_AREA.width - CHARGE_OFFSET - CHARGE_SIZE,
            (PLAYING_AREA.height / 2) - CHARGE_OFFSET,
            CHARGE_SIZE,
            CHARGE_SIZE
        },
        charger_texture,
        ObjectType::Charger
    ));

    // Top charger
    objects.push_back(Object(
        {
            (PLAYING_AREA.width / 2) - CHARGE_OFFSET,
            CHARGE_OFFSET,
            CHARGE_SIZE,
            CHARGE_SIZE
        },
        charger_texture,
        ObjectType::Charger
    ));

    // Bottom charger
    objects.push_back(Object(
        {
            (PLAYING_AREA.width / 2) - CHARGE_OFFSET,
            PLAYING_AREA.height - CHARGE_OFFSET - CHARGE_SIZE,
            CHARGE_SIZE,
            CHARGE_SIZE
        },
        charger_texture,
        ObjectType::Charger
    ));

    // Add cubes
    auto cubes = get_rand_cubes(10, 10);
    objects.insert(objects.end(), cubes.begin(), cubes.end());
}
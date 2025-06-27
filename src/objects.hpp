#pragma once

#include <raylib.h>
#include <vector>
#include "constants.hpp"

enum class ObjectType {
    Generic = 0,
    Barrel = 1,
    Charger = 2,
    Cube = 3
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

        Object(netvent::Value value, Texture2D texture) {
            netvent::Table value_table = value.as_table();
            this->bounds = {value_table["x"].as_float(), value_table["y"].as_float(), value_table["width"].as_float(), value_table["height"].as_float()};
            this->texture = texture;
            this->tint = {100, 100, 255, 255};
            this->type = (ObjectType)value_table["type"].as_int();
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
                case ObjectType::Cube:
                    DrawTexturePro(
                        texture,
                        {0, 0, 16, 16},
                        bounds,
                        {0, 0},
                        0.0f,
                        tint);
                    break;
                default:
                    DrawRectangleRec(bounds, color);
                    break;
            }
        }

        bool check_collision(Rectangle other) {
            return CheckCollisionRecs(bounds, other);
        }

        netvent::Table to_table() {
            return netvent::map_table({
                {"x", netvent::val(bounds.x)},
                {"y", netvent::val(bounds.y)},
                {"width", netvent::val(bounds.width)},
                {"height", netvent::val(bounds.height)},
                {"type", netvent::val(static_cast<int>(type))}
            });
        }

        void set_active(bool active) {
            is_active = active;
            tint = active ? GREEN : WHITE;
        }
};

std::vector<Object> objects_from_table(netvent::Table table, Texture2D texture) {
    std::vector<Object> objects;
    for (auto& value : table.get_data_vector()) {
        objects.push_back(Object(value, texture));
    }
    return objects;
}

netvent::Table objects_to_table(std::vector<Object> objects) {
    netvent::Table table = netvent::arr_table({});
    for (auto& object : objects) {
        table.push_back(object.to_table());
    }
    return table;
}

std::vector<Object> objects;

inline std::vector<Object> get_rand_cubes(int divide_how_many_can_fit_by, int cube_size) {
    std::vector<Object> cubes;
    
    const int TILE_SIZE = 50;
    const int MARGIN = 200; // 200px margin from edges
    
    // Calculate effective area after removing margins
    int effective_width = PLAYING_AREA.width - (2 * MARGIN);
    int effective_height = PLAYING_AREA.height - (2 * MARGIN);
    
    // Calculate how many tiles fit in the effective area
    int tiles_x = effective_width / TILE_SIZE;
    int tiles_y = effective_height / TILE_SIZE;
    int total_tiles = tiles_x * tiles_y;
    
    int min_cubes = total_tiles / 10;  
    int max_cubes = total_tiles / 3;  
    int num_cubes_to_spawn = GetRandomValue(min_cubes, max_cubes);
    
    // Define barrel position (umbrella barrel from constants)
    const int BARREL_SIZE = 50;
    const int BARREL_COLLISION_SIZE = BARREL_SIZE * 2;
    Rectangle barrel_area = {
        (PLAYING_AREA.width / 2) - (BARREL_COLLISION_SIZE / 2),
        (PLAYING_AREA.height / 2) - (BARREL_COLLISION_SIZE / 2),
        BARREL_COLLISION_SIZE,
        BARREL_COLLISION_SIZE
    };
    
    // Create a list of all possible tile positions (with margin)
    std::vector<std::pair<int, int>> available_tiles;
    for (int x = 0; x < tiles_x; x++) {
        for (int y = 0; y < tiles_y; y++) {
            // Calculate actual world position from tile coordinates (offset by margin)
            float world_x = MARGIN + (x * TILE_SIZE);
            float world_y = MARGIN + (y * TILE_SIZE);
            
            // Check if this tile would overlap with the barrel
            Rectangle tile_rect = {world_x, world_y, (float)cube_size, (float)cube_size};
            
            if (!CheckCollisionRecs(tile_rect, barrel_area)) {
                available_tiles.push_back({x, y});
            }
        }
    }
    
    // Shuffle the available tiles to get random placement
    for (int i = available_tiles.size() - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        std::swap(available_tiles[i], available_tiles[j]);
    }
    
    // Spawn cubes in the first N shuffled tile positions
    for (int i = 0; i < num_cubes_to_spawn && i < available_tiles.size(); i++) {
        int tile_x = available_tiles[i].first;
        int tile_y = available_tiles[i].second;
        
        // Calculate actual world position from tile coordinates (offset by margin)
        float world_x = MARGIN + (tile_x * TILE_SIZE);
        float world_y = MARGIN + (tile_y * TILE_SIZE);
        
        // Get random color
        Color color = {
            (unsigned char)GetRandomValue(0, 255),
            (unsigned char)GetRandomValue(0, 255),
            (unsigned char)GetRandomValue(0, 255),
            255
        };
        
        // Create cube at this tile position
        Rectangle bounds = {world_x, world_y, (float)cube_size, (float)cube_size};
        Object cube(bounds, color, ObjectType::Cube);
        cubes.push_back(cube);
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

    
}
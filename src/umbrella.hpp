#pragma once
#include <raylib.h>
#include <vector>
#include <set>
#include "constants.hpp"
#include "resource_manager.hpp"
#include "bullet.hpp"

struct UmbrellaUpdateData {
    bool is_active;
    bool is_shooting;
    bool is_usable;
};

class Umbrella {
    public:
        Rectangle our_position;
        bool is_usable = true;
        bool is_active = false;
        bool is_shooting = false;
        Color tint = WHITE;
        std::set<int> hit_by_bullets;
        float time_absorbed = 0.0f;

        Umbrella() {}
        ~Umbrella() {}

        void set_hit(bool hit) {
            if (hit) {
                tint = RED;
            }
        }

        UmbrellaUpdateData update(Rectangle barrel, Rectangle player, std::vector<Rectangle> bullets, std::vector<Bullet>& game_bullets, bool is_active, float rotation, bool acid_rain_active) {
            if (CheckCollisionRecs(barrel, player)) {
                how_many_times_hit = 0;
                is_usable = true;
                hit_by_bullets.clear();
            }
            if (!is_active) return {is_active, is_shooting, is_usable};
            
            if (is_shooting) {
                float distance = 60.0f;
                float rotation_rad = rotation * (M_PI / 180.0f);
                our_position = {
                    player.x + player.width / 2 + cos(rotation_rad) * distance - 37.5f,
                    player.y + player.height / 2 + sin(rotation_rad) * distance - 37.5f,
                    75,  
                    75  
                };
            } else {
                our_position = {
                    player.x,  
                    player.y - 85,  
                    75,  
                    75  
                };
            }
            
            if (is_usable) {
                hit_cooldown -= GetFrameTime();

                // update umbrella absorption
                if (is_active && acid_rain_active) {
                    time_absorbed += GetFrameTime();
                    if (time_absorbed > 3.0f && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        is_shooting = true;
                        time_absorbed -= PI / 3; // random ahh number
                    } else {
                        is_shooting = false;
                    } 
                } else {
                    is_shooting = false;
                }

                // Fade tint back to white
                if (tint.r < 255) tint.r += 5;
                if (tint.g < 255) tint.g += 5;
                if (tint.b < 255) tint.b += 5;

                bool was_hit = false;
                for (size_t i = 0; i < bullets.size(); i++) {
                    const Rectangle& bullet_rect = bullets[i];
                    const Bullet& bullet = game_bullets[i];
                    
                    if (hit_cooldown <= 0 && CheckCollisionRecs(our_position, bullet_rect)) {
                        if (hit_by_bullets.find(bullet.bullet_id) == hit_by_bullets.end()) {
                            how_many_times_hit++;
                            hit_cooldown = 0.5f; 
                            was_hit = true;
                            hit_by_bullets.insert(bullet.bullet_id);
                            std::cout << "Umbrella hit by bullet " << bullet.bullet_id << "! Hits: " << how_many_times_hit << std::endl;
                            if (how_many_times_hit >= UMBRELLA_HIT_LIMIT) {
                                is_usable = false;
                                std::cout << "Umbrella destroyed!" << std::endl;
                                break;
                            }
                        }
                    }                
                }
                
                // Set red tint on hit
                if (was_hit) {
                    tint = RED;
                    time_absorbed = 0.0f;
                }
            } else {
                is_shooting = false;
            }

            return {is_active, is_shooting, is_usable};
        }
        void draw(ResourceManager* res_man, float player_x, float player_y, float rotation = 0.0f) {
            if (!is_active || !is_usable) return;
            
            float umbrella_x, umbrella_y;
            float umbrella_rotation;
            
            if (is_shooting) {
                // When shooting, position umbrella around the player based on rotation
                float distance = 80.0f; // Distance from player center
                float angle_rad = (rotation - 90.0f) * DEG2RAD; // Convert to radians and adjust for up direction
                
                umbrella_x = (player_x + 50) + cosf(angle_rad) * distance;
                umbrella_y = (player_y + 50) + sinf(angle_rad) * distance;
                umbrella_rotation = rotation;
            } else {
                // When not shooting, position umbrella above the player (default position)
                umbrella_x = player_x + 50;
                umbrella_y = player_y - 35;
                umbrella_rotation = 0.0f;
            }
            
            DrawTexturePro(res_man->getTex("assets/umbrella.png"), 
                         {(float)0, (float)0, 16, 16},
                         {umbrella_x, umbrella_y, 75, 75},
                         {(float)37.5, (float)37.5}, umbrella_rotation, tint);
        }
    private:
        int how_many_times_hit = 0;
        float hit_cooldown = 0.0f;
};
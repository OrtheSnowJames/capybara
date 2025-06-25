#pragma once
#include <raylib.h>
#include <vector>
#include "constants.hpp"
#include "resource_manager.hpp"

class Umbrella {
    public:
        Rectangle our_position;
        bool is_usable = true;
        bool is_active = false;
        Umbrella() {}
        ~Umbrella() {}
        bool update(Rectangle barrel, Rectangle player, std::vector<Rectangle> bullets, bool is_active) {
            if (CheckCollisionRecs(barrel, player)) {
                how_many_times_hit = 0;
                is_usable = true;
            }

            if (!is_active) return is_usable;

            // update our position (align with visual position)
            our_position = {
                player.x,  
                player.y - 85,  
                75,  
                75  
            };
            
            if (is_usable) {
                hit_cooldown -= GetFrameTime();
                for (const Rectangle& bullet : bullets) {
                    if (hit_cooldown <= 0 && CheckCollisionRecs(our_position, bullet)) {
                        how_many_times_hit++;
                        hit_cooldown = 0.1f; // 100ms cooldown between hits
                        std::cout << "Umbrella hit! Hits: " << how_many_times_hit << std::endl;
                        if (how_many_times_hit >= UMBRELLA_HIT_LIMIT) {
                            is_usable = false;
                            std::cout << "Umbrella destroyed!" << std::endl;
                            break;
                        }
                    }                
                }
            }

            return is_usable;
        }
        void draw(ResourceManager* res_man, float player_x, float player_y) {
            if (!is_active || !is_usable) return;
            
            // umbrella should always face up and be positioned above the player
            DrawTexturePro(res_man->getTex("assets/umbrella.png"), 
                         {(float)0, (float)0, 16, 16},
                         {player_x + 50, player_y - 35, 75, 75},
                         {(float)37.5, (float)60}, 0, WHITE);
        }
    private:
        int how_many_times_hit = 0;
        float hit_cooldown = 0.0f;
};
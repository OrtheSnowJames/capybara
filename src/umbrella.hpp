#pragma once
#include <raylib.h>
#include <vector>
#include <set>
#include "constants.hpp"
#include "resource_manager.hpp"
#include "bullet.hpp"

class Umbrella {
    public:
        Rectangle our_position;
        bool is_usable = true;
        bool is_active = false;
        Color tint = WHITE;
        std::set<int> hit_by_bullets;

        Umbrella() {}
        ~Umbrella() {}

        void set_hit(bool hit) {
            if (hit) {
                tint = RED;
            }
        }

        bool update(Rectangle barrel, Rectangle player, std::vector<Rectangle> bullets, std::vector<Bullet>& game_bullets, bool is_active) {
            if (CheckCollisionRecs(barrel, player)) {
                how_many_times_hit = 0;
                is_usable = true;
                hit_by_bullets.clear();
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
                
                // Fade tint back to white
                if (tint.r < 255) tint.r += 5;
                if (tint.g < 255) tint.g += 5;
                if (tint.b < 255) tint.b += 5;

                bool was_hit = false;
                for (size_t i = 0; i < bullets.size(); i++) {
                    const Rectangle& bullet_rect = bullets[i];
                    const Bullet& bullet = game_bullets[i];
                    
                    if (hit_cooldown <= 0 && CheckCollisionRecs(our_position, bullet_rect)) {
                        // Only count hit if we haven't been hit by this bullet before
                        if (hit_by_bullets.find(bullet.bullet_id) == hit_by_bullets.end()) {
                            how_many_times_hit++;
                            hit_cooldown = 0.5f; // 500ms cooldown between hits
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
                         {(float)37.5, (float)60}, 0, tint);
        }
    private:
        int how_many_times_hit = 0;
        float hit_cooldown = 0.0f;
};
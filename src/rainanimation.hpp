#pragma once
#include <iostream>
#include <vector>
#include <raylib.h>
#include "constants.hpp"
#include "player.hpp"
#include <random>
#include <map>

struct RainDrop {
    Vector2 position;
    float speed;
    float size;
    float rot;
    float alpha;
    bool hidden = false;
    int raindrop_id = -1;  // Add ID for network synchronization
};

class AcidRainEvent {
private:
    float timer = 0.0f;
    float interval = 10.0f;
    float nextDropDelay = 0.0f;
    bool active = false;
    std::vector<RainDrop> raindrops;
    const int MAX_DROPS = 100;
    const float MIN_SPEED = 300.0f;
    const float MAX_SPEED = 600.0f;
    const float MIN_SIZE = 2.0f;
    const float MAX_SIZE = 5.0f;
    
    std::map<int, float> player_shoot_cooldowns;
    const float SHOOT_COOLDOWN = 0.15f; 

public:
    AcidRainEvent() = default;

    void start(float initialDelay) {
        active = true;
        timer = 0.0f;
        interval = 10.0f;
        nextDropDelay = initialDelay;
        raindrops.clear();
    }

    void stop() {
        active = false;
        raindrops.clear();
    }

    void update(float dt, playermap players) {
        if (!active) return;

        timer += dt;

        // Update player shoot cooldowns
        for (auto& [player_id, cooldown] : player_shoot_cooldowns) {
            cooldown -= dt;
        }

        // Update existing raindrops
        for (auto it = raindrops.begin(); it != raindrops.end();) {
            if (it->rot != 0) {
                it->position.x += cosf(it->rot) * it->speed * dt;
                it->position.y += sinf(it->rot) * it->speed * dt;
            } else {
                it->position.y += it->speed * dt;
            }
            
            it->alpha = std::max(0.0f, it->alpha - dt * 0.5f); // Fade out over time
            
            if (it->position.y > PLAYING_AREA.height || it->alpha <= 0 || 
                it->position.x < -50 || it->position.x > PLAYING_AREA.width + 50) {
                it = raindrops.erase(it);
            } else {
                ++it;
            }
        }

        // add new raindrops
        if (raindrops.size() < MAX_DROPS && GetRandomValue(0, 100) < 30) {
            RainDrop drop;
            drop.position.x = GetRandomValue(0, PLAYING_AREA.width);
            drop.position.y = -10.0f;
            drop.speed = GetRandomValue((int)MIN_SPEED, (int)MAX_SPEED);
            drop.size = GetRandomValue((int)MIN_SIZE, (int)MAX_SIZE);
            drop.rot = 0.0f;
            drop.alpha = 1.0f;
            raindrops.push_back(drop);
        } 

        if (timer >= nextDropDelay) {
            triggerEvent();
            timer = 0.0f;
            nextDropDelay = interval;
        }
    }

    void draw(playermap players) {
        if (!active) return;

        for (auto& drop : raindrops) {
            for (const auto& [id, player] : players) {
                if (raindrop_touching_player(drop, player)) {
                    drop.hidden = true;
                } else {
                    drop.hidden = false;
                }
            }
            if (!drop.hidden) {
                Color dropColor = {0, 255, 0, (unsigned char)(drop.alpha * 255)}; 
                DrawCircle(drop.position.x, drop.position.y, drop.size, dropColor);
                
                float trailLength = drop.speed * 0.05f;
                Vector2 trailEnd;
                
                if (drop.rot != 0) {
                    trailEnd = {
                        drop.position.x - cosf(drop.rot) * trailLength,
                        drop.position.y - sinf(drop.rot) * trailLength
                    };
                } else {
                    trailEnd = {drop.position.x, drop.position.y - trailLength};
                }
                
                Color trailColor = {0, 255, 0, (unsigned char)(drop.alpha * 128)}; 
                DrawLineEx(drop.position, trailEnd, drop.size * 0.5f, trailColor);
            }
        }
    }

    bool is_active() {
        return active;
    }

private:
    void triggerEvent() {
        // Add a burst of raindrops
        for (int i = 0; i < 20; i++) {
            RainDrop drop;
            drop.position.x = GetRandomValue(0, PLAYING_AREA.width);
            drop.position.y = -10.0f;
            drop.speed = GetRandomValue((int)MIN_SPEED, (int)MAX_SPEED);
            drop.size = GetRandomValue((int)MIN_SIZE, (int)MAX_SIZE);
            drop.rot = 0.0f; // Initialize to 0 for straight down movement
            drop.alpha = 1.0f;
            raindrops.push_back(drop);
        }
    }

    bool raindrop_touching_player(RainDrop drop, Player player) {
        Rectangle drop_rect = {drop.position.x - drop.size, drop.position.y - drop.size, drop.size * 2, drop.size * 2};
        Rectangle player_rect;

        if (player.weapon_id == (int)Weapon::umbrella) {
            player_rect = {(float)player.x, (float)player.y - 85, 75, 150};
        } else {
            player_rect = {(float)player.x, (float)player.y, 50, 50};
        }

        if (CheckCollisionRecs(drop_rect, player_rect)) {
            return true;
        }
        return false;
    }

    bool raindrop_touching_umbrella(RainDrop drop, Player player) {
        Rectangle drop_rect = {drop.position.x - drop.size, drop.position.y - drop.size, drop.size * 2, drop.size * 2};
        Rectangle player_rect;

        if (player.weapon_id == (int)Weapon::umbrella) {
            player_rect = {(float)player.x, (float)player.y - 85, 75, 150};
        } else {
            player_rect = {(float)player.x, (float)player.y, 50, 50};
        }

        if (CheckCollisionRecs(drop_rect, player_rect)) {
            return true;
        }
        return false;
    }

    void deflect_raindrop(RainDrop& drop, Player player) {
        float umbrella_angle_rad = player.rot * DEG2RAD;
        
        Vector2 umbrella_normal = {
            sinf(umbrella_angle_rad),   // x component of normal
            -cosf(umbrella_angle_rad)   // y component of normal (negative because y increases downward)
        };
        
        Vector2 drop_velocity = {0, drop.speed};
        
        float dot_product = drop_velocity.x * umbrella_normal.x + drop_velocity.y * umbrella_normal.y;
        Vector2 reflected_velocity = {
            drop_velocity.x - 2 * dot_product * umbrella_normal.x,
            drop_velocity.y - 2 * dot_product * umbrella_normal.y
        };
        
        float deflection_speed = drop.speed * 0.8f;
        
        float magnitude = sqrtf(reflected_velocity.x * reflected_velocity.x + reflected_velocity.y * reflected_velocity.y);
        if (magnitude > 0) {
            reflected_velocity.x = (reflected_velocity.x / magnitude) * deflection_speed;
            reflected_velocity.y = (reflected_velocity.y / magnitude) * deflection_speed;
        }
        
        drop.speed = sqrtf(reflected_velocity.x * reflected_velocity.x + reflected_velocity.y * reflected_velocity.y);
        
        drop.rot = atan2f(reflected_velocity.y, reflected_velocity.x);
        
        drop.position.x += umbrella_normal.x * 10;
        drop.position.y += umbrella_normal.y * 10;
    }
};

#pragma once
#include <iostream>
#include <vector>
#include <raylib.h>
#include "constants.hpp"
#include <random>

struct RainDrop {
    Vector2 position;
    float speed;
    float size;
    float alpha;
};

class AcidRain {
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

public:
    AcidRain() = default;

    // Start acid rain with a delay before first event
    void start(float initialDelay) {
        active = true;
        timer = 0.0f;
        interval = 10.0f;
        nextDropDelay = initialDelay;
        raindrops.clear();
    }

    // Stop acid rain events
    void stop() {
        active = false;
        raindrops.clear();
    }

    // Call this every frame with delta time (seconds)
    void update(float dt) {
        if (!active) return;

        timer += dt;

        // Update existing raindrops
        for (auto it = raindrops.begin(); it != raindrops.end();) {
            it->position.y += it->speed * dt;
            it->alpha = std::max(0.0f, it->alpha - dt * 0.5f); // Fade out over time
            
            // Remove drops that are off screen or fully faded
            if (it->position.y > PLAYING_AREA.height || it->alpha <= 0) {
                it = raindrops.erase(it);
            } else {
                ++it;
            }
        }

        // Add new raindrops
        if (raindrops.size() < MAX_DROPS && GetRandomValue(0, 100) < 30) {
            RainDrop drop;
            drop.position.x = GetRandomValue(0, PLAYING_AREA.width);
            drop.position.y = -10.0f;
            drop.speed = GetRandomValue((int)MIN_SPEED, (int)MAX_SPEED);
            drop.size = GetRandomValue((int)MIN_SIZE, (int)MAX_SIZE);
            drop.alpha = 1.0f;
            raindrops.push_back(drop);
        }

        if (timer >= nextDropDelay) {
            triggerEvent();
            timer = 0.0f;
            nextDropDelay = interval;
        }
    }

    void draw() {
        if (!active) return;

        for (const auto& drop : raindrops) {
            Color dropColor = {0, 255, 0, (unsigned char)(drop.alpha * 255)}; // Green with alpha
            DrawCircle(drop.position.x, drop.position.y, drop.size, dropColor);
            
            // Draw trail
            float trailLength = drop.speed * 0.05f;
            Vector2 trailEnd = {drop.position.x, drop.position.y - trailLength};
            Color trailColor = {0, 255, 0, (unsigned char)(drop.alpha * 128)}; // More transparent trail
            DrawLineEx(drop.position, trailEnd, drop.size * 0.5f, trailColor);
        }
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
            drop.alpha = 1.0f;
            raindrops.push_back(drop);
        }
    }
};

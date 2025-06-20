#ifndef DRAW_SCALE_HPP
#define DRAW_SCALE_HPP

#include "raylib.h"

const Vector2 window_size = {800, 600};

inline float GetMinScaleRatio() {
    float widthScaleRatio = (float)GetScreenWidth() / window_size.x;
    float heightScaleRatio = (float)GetScreenHeight() / window_size.y;
    return (widthScaleRatio < heightScaleRatio) ? widthScaleRatio : heightScaleRatio;
}

// Bottom-left anchored versions
inline void DrawRectangleScale(float x, float y, float width, float height, Color color) {
    float scaleRatio = GetMinScaleRatio();
    
    DrawRectangle(
        x * scaleRatio,
        GetScreenHeight() - ((window_size.y - y) * scaleRatio),
        width * scaleRatio,
        height * scaleRatio,
        color
    );
}

inline void DrawSquareScale(float x, float y, float size, Color color) {
    float scaleRatio = GetMinScaleRatio();
    
    DrawRectangle(
        x * scaleRatio,
        GetScreenHeight() - ((window_size.y - y) * scaleRatio),
        size * scaleRatio,
        size * scaleRatio,
        color
    );
}

inline void DrawTextScale(const char *text, float x, float y, float size, Color color) {
    float scaleRatio = GetMinScaleRatio();
    
    DrawText(
        text,
        x * scaleRatio,
        GetScreenHeight() - ((window_size.y - y) * scaleRatio),
        size * scaleRatio,
        color
    );
}

// Centered versions
inline void DrawRectangleScaleCentered(float x, float y, float width, float height, Color color) {
    float scaleRatio = GetMinScaleRatio();
    float offsetX = (GetScreenWidth() - (window_size.x * scaleRatio)) / 2;
    float offsetY = (GetScreenHeight() - (window_size.y * scaleRatio)) / 2;
    
    DrawRectangle(
        x * scaleRatio + offsetX,
        y * scaleRatio + offsetY,
        width * scaleRatio,
        height * scaleRatio,
        color
    );
}

inline void DrawSquareScaleCentered(float x, float y, float size, Color color) {
    float scaleRatio = GetMinScaleRatio();
    float offsetX = (GetScreenWidth() - (window_size.x * scaleRatio)) / 2;
    float offsetY = (GetScreenHeight() - (window_size.y * scaleRatio)) / 2;
    
    DrawRectangle(
        x * scaleRatio + offsetX,
        y * scaleRatio + offsetY,
        size * scaleRatio,
        size * scaleRatio,
        color
    );
}

inline void DrawTextScaleCentered(const char *text, float x, float y, float size, Color color) {
    float scaleRatio = GetMinScaleRatio();
    float offsetX = (GetScreenWidth() - (window_size.x * scaleRatio)) / 2;
    float offsetY = (GetScreenHeight() - (window_size.y * scaleRatio)) / 2;
    
    DrawText(
        text,
        x * scaleRatio + offsetX,
        y * scaleRatio + offsetY,
        size * scaleRatio,
        color
    );
}

inline int MeasureTextScale(const char *text, float size) {
    // Don't apply scaling here since we're measuring for the base 800x600 resolution
    return MeasureText(text, size);
}

// These are now just empty functions since we're handling scaling in the draw functions
inline void BeginUiDrawing() {}
inline void EndUiDrawing() {}
#endif // DRAW_SCALE_HPP
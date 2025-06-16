#include "raylib.h"

const Vector2 window_size = {800, 600};

float GetMinScaleRatio() {
    float widthScaleRatio = (float)GetScreenWidth() / window_size.x;
    float heightScaleRatio = (float)GetScreenHeight() / window_size.y;
    return (widthScaleRatio < heightScaleRatio) ? widthScaleRatio : heightScaleRatio;
}

void DrawRectangleScale(float x, float y, float width, float height, Color color) {
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

void DrawSquareScale(float x, float y, float size, Color color) {
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

void DrawTextScale(const char *text, float x, float y, float size, Color color) {
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

// These are now just empty functions since we're handling scaling in the draw functions
void BeginUiDrawing() {}
void EndUiDrawing() {}
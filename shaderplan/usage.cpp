#include "raylib.h"

int main() {
    int screenWidth = 800;
    int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Shader Plan");

    Shader blur = LoadShader(0, "blur.fs");
    RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);

    BeginTextureMode(target);
        ClearBackground(BLANK);
        // Draw your water effect here
        DrawCircle(screenWidth / 2, screenHeight / 2, 100, BLUE);
    EndTextureMode();

    BeginShaderMode(blur);
        SetShaderValue(blur, GetShaderLocation(blur, "resolution"), (float[]){(float)screenWidth, (float)screenHeight}, SHADER_UNIFORM_VEC2);
        DrawTextureRec(target.texture, (Rectangle){0, 0, (float)screenWidth, -(float)screenHeight}, (Vector2){0, 0}, WHITE);
    EndShaderMode();

    CloseWindow();
    return 0;
}

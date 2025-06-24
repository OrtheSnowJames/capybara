#pragma once
#include "raylib.h"
#include "utils.hpp"
#include <map>
#include <string>

class ResourceManager {
private:
  std::map<std::string, Texture2D> textures;

public:
  Texture2D getTex(std::string path) {
    if (textures.find(path) == textures.end()) {
      textures[path] = LoadTexture(path.c_str());
    }
    return textures[path];
  }
  Texture2D load_player_texture_from_color(Color clr) {
    if (color_equal(clr, GREEN))
      return getTex("assets/player_green.png");
    if (color_equal(clr, YELLOW))
      return getTex("assets/player_yellow.png");
    if (color_equal(clr, ORANGE))
      return getTex("assets/player_orangle.png");
    if (color_equal(clr, PURPLE))
      return getTex("assets/player_purple.png");
    return getTex("assets/player_red.png");
  }

  // on delete / out of scope
  ~ResourceManager() {
    for (auto &[path, tex] : textures)
      UnloadTexture(tex);
  }
};

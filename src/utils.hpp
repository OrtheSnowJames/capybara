#ifndef UTILS_H
#define UTILS_H

#include "raylib.h"
#include "player.hpp"
#include "constants.hpp"
#include "drawScale.hpp"
#include "networking.hpp"
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>

const Color INVISIBLE = BLANK;

typedef std::map<int, Player> playermap;
typedef std::pair<int, std::shared_ptr<std::thread>> client;

inline bool operator<(const Color& a, const Color& b) {
    if (a.r != b.r) return a.r < b.r;
    if (a.g != b.g) return a.g < b.g;
    if (a.b != b.b) return a.b < b.b;
    return a.a < b.a;
}



inline bool isInViewport(int x, int y, int width, int height, Camera2D cam, int margin = 0) {
  Rectangle viewport = {cam.target.x - cam.offset.x / cam.zoom - margin,
                        cam.target.y - cam.offset.y / cam.zoom - margin,
                        window_size.x / cam.zoom + (margin * 2),
                        window_size.y / cam.zoom + (margin * 2)};

  return CheckCollisionRecs({(float)x, (float)y, (float)width, (float)height},
                            viewport);
}

inline bool color_equal(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline std::string color_to_string(Color c) {
  if (color_equal(c, RED)) return "RED";
  else if (color_equal(c, GREEN)) return "GREEN";
  else if (color_equal(c, YELLOW)) return "YELLOW";
  else if (color_equal(c, PURPLE)) return "PURPLE";
  else if (color_equal(c, ORANGE)) return "ORANGE";
  else if (color_equal(c, INVISIBLE)) return "INVISIBLE";
  else return "UNKNOWN";
}

inline Color uint_to_color(unsigned int i) {
  switch (i) {
  case 0:
    return RED;
  case 1:
    return GREEN;
  case 2:
    return YELLOW;
  case 3:
    return PURPLE;
  case 4:
    return ORANGE;
  case 5:
    return INVISIBLE;
  }
  return BLACK;
}

inline unsigned int color_to_uint(Color c) {
  if (color_equal(c, RED))
    return 0;
  else if (color_equal(c, GREEN))
    return 1;
  else if (color_equal(c, YELLOW))
    return 2;
  else if (color_equal(c, PURPLE))
    return 3;
  else if (color_equal(c, ORANGE))
    return 4;
  else if (color_equal(c, INVISIBLE))
  return 5;
  return 6;
}

struct ColorCompare {
  bool operator()(const Color &a, const Color &b) const {
    // Proper less-than comparison for map ordering
    if (a.r != b.r)
      return a.r < b.r;
    if (a.g != b.g)
      return a.g < b.g;
    if (a.b != b.b)
      return a.b < b.b;
    return a.a < b.a;
  }
};

inline void send_message(std::string msg, int sock) {
  msg.push_back(';');
  const char *msg_str = msg.c_str();
  if (send_data(sock, msg_str, msg.size(), 0) < 0) {
    print_socket_error("error sending message");
  }
}

inline void broadcast_message(std::string msg,
                              std::unordered_map<int, client> clients,
                              int exclude = -1000) {
  for (auto &[_, s] : clients)
    if (_ != exclude)
      send_message(msg, s.first);
}

inline void split(std::string str, std::string splitBy,
                  std::vector<std::string> &tokens) {
  /* Store the original string in the array, so we can loop the rest
   * of the algorithm. */
  tokens.push_back(str);

  // Store the split index in a 'size_t' (unsigned integer) type.
  size_t splitAt;
  // Store the size of what we're splicing out.
  size_t splitLen = splitBy.size();
  // Create a string for temporarily storing the fragment we're processing.
  std::string frag;
  // Loop infinitely - break is internal.
  while (true) {
    /* Store the last string in the vector, which is the only logical
     * candidate for processing. */
    frag = tokens.back();
    /* The index where the split is. */
    splitAt = frag.find(splitBy);
    // If we didn't find a new split point...
    if (splitAt == std::string::npos) {
      // Break the loop and (implicitly) return.
      break;
    }
    /* Put everything from the left side of the split where the string
     * being processed used to be. */
    tokens.back() = frag.substr(0, splitAt);
    /* Push everything from the right side of the split to the next empty
     * index in the vector. */
    tokens.push_back(
        frag.substr(splitAt + splitLen, frag.size() - (splitAt + splitLen)));
  }
}

inline std::string sanitize_username(std::string str) {
  // Remove non-alphabetic characters
  str.erase(std::remove_if(str.begin(), str.end(),
                           [](unsigned char c) { return !std::isalpha(c); }),
            str.end());
  // Convert to lowercase
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

template <typename T>
T random_enum_element(T first, T last) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(static_cast<int>(first), static_cast<int>(last));
  return static_cast<T>(dis(gen));
}

void DrawTextureAlpha(Texture2D texture, int x, int y, unsigned char alpha) {
  Color tint = Color{255, 255, 255, alpha};
  DrawTexture(texture, x, y, tint);
}

#endif

#ifndef UTILS_H
#define UTILS_H

#include "player.hpp"
#include "raylib.h"
#include <arpa/inet.h>
#include <cstdio>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <vector>

typedef std::map<int, Player> playermap;
typedef std::pair<int, std::shared_ptr<std::thread>> client;

inline bool color_equal(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
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
  return 5;
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
  if (send(sock, msg_str, msg.size(), 0) < 0) {
    perror("error sending message");
  }
}

inline void broadcast_message(std::string msg,
                              std::unordered_map<int, client> clients) {
  for (auto &[_, s] : clients)
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

#endif

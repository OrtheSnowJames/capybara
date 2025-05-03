#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <cstdio>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

inline void send_message(std::string msg, int sock) {
  const char *msg_str = msg.c_str();
  if (send(sock, msg_str, msg.size(), 0) < 0) {
    perror("error sending message");
  }
}

inline void broadcast_message(std::string msg, std::vector<int> socks) {
  for (int s : socks)
    send_message(msg, s);
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

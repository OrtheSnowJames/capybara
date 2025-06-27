#pragma once
#include <raylib.h>
#include "netvent.hpp"

inline netvent::Table color_to_table(Color c) {
  return netvent::map_table({
    {"r", netvent::val((int)c.r)},
    {"g", netvent::val((int)c.g)},
    {"b", netvent::val((int)c.b)},
    {"a", netvent::val((int)c.a)}
  });
}

inline Color color_from_table(netvent::Table tbl) {
  return Color{
    (unsigned char)tbl[netvent::val("r")].as_int(),
    (unsigned char)tbl[netvent::val("g")].as_int(),
    (unsigned char)tbl[netvent::val("b")].as_int(),
    (unsigned char)tbl[netvent::val("a")].as_int()
  };
}
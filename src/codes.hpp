#pragma once

inline const int MSG_GAME_STATE = 0;         // Format: "0\n<player_data>" where player_data = ":<id> <x> <y> <username> <color_code> <weapon_id>" (multiple players separated by :)
inline const int MSG_CLIENT_ID = 1;          // Format: "1\n<client_id>"
inline const int MSG_PLAYER_MOVE = 2;        // Format: "2\n<player_id> <x> <y> <rotation>"
inline const int MSG_PLAYER_NEW = 3;         // Format: "3\n<player_id> <x> <y> <username> <color_code> <weapon_id>"
inline const int MSG_PLAYER_LEFT = 4;        // Format: "4\n<player_id>"
inline const int MSG_PLAYER_UPDATE = 5;      // Format: "5\n<player_id> <username> <color_code>"
inline const int MSG_BULLET_SHOT = 10;       // Format: "10\n<player_id> <x> <y> <rotation>"
inline const int MSG_EVENT_SUMMON = 11;      // Format: "11\n<event_type as int>" 
inline const int MSG_SWITCH_WEAPON = 12;     // Format: "12\n<player_id> <weapon_id>"
inline const int MSG_ASSASSIN_CHANGE = 15;   // Format: "15\n<player_id> <target_id>" 
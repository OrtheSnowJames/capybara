#pragma once
#include <string>
#include <variant>
#include <sstream>
#include <stdexcept>
#include <map>
#include <vector>
#include <memory>
#include <iomanip>

namespace netvent {

class Table;
class Value;

// comparison operators
bool operator<(const Value& lhs, const Value& rhs);
bool operator==(const Value& lhs, const Value& rhs);

class Value {
    private:
        std::variant<int, float, bool, std::string, std::shared_ptr<Table>> data;

    public:
        // creates a null value (0)
        Value() : data(0) {}
        
        // constructors for different types
        Value(int v) : data(v) {}
        Value(float v) : data(v) {}
        Value(bool v) : data(v) {}
        Value(const char* v) : data(std::string(v)) {}
        Value(const std::string& v) : data(v) {}
        Value(const Table& v) : data(std::make_shared<Table>(v)) {}
        Value(const std::shared_ptr<Table>& v) : data(v) {}

        // type checkers
        bool is_int() const { return std::holds_alternative<int>(data); }
        bool is_float() const { return std::holds_alternative<float>(data); }
        bool is_bool() const { return std::holds_alternative<bool>(data); }
        bool is_string() const { return std::holds_alternative<std::string>(data); }
        bool is_table() const { return std::holds_alternative<std::shared_ptr<Table>>(data); }

        // getters
        int as_int() const { return std::get<int>(data); }
        float as_float() const { return std::get<float>(data); }
        bool as_bool() const { return std::get<bool>(data); }
        std::string as_string() const { return std::get<std::string>(data); }
        const Table& as_table() const { return *std::get<std::shared_ptr<Table>>(data); }
        Table& as_table() { return *std::get<std::shared_ptr<Table>>(data); }


        // comparison operators
        friend bool operator<(const Value& lhs, const Value& rhs);
        friend bool operator==(const Value& lhs, const Value& rhs);

        // serialize and deserialize
        std::string serialize() const;
        static Value deserialize(const std::string& data);
    };

class Table {
    // table is like lua table, it can be nested and can be array or object
    private:
        std::map<Value, Value> data;
        bool is_array = false;
    public:
        Table() = default;
        Table(const std::map<Value, Value>& d) : data(d) {}
        Table(const std::vector<Value>& d) {
            for (size_t i = 0; i < d.size(); i++) {
                data[Value(static_cast<int>(i))] = d[i];
            }
            is_array = true;
        }
        Table(std::initializer_list<std::pair<Value, Value>> init) : data(init.begin(), init.end()) {}
        Table(std::initializer_list<Value> init) : data() {
            size_t i = 0;
            for (const auto& v : init) {
                data[Value(static_cast<int>(i++))] = v;
            }
            is_array = true;
        }
        Table(std::initializer_list<std::pair<const char*, Value>> init) {
            for (const auto& [key, value] : init) {
                data[Value(key)] = value;
            }
        }

        void push_back(const Value& value) {
            if (!is_array) throw std::runtime_error("Table is not an array");
            data[Value(static_cast<int>(data.size()))] = value;
        }

        void push_back(const Value& key, const Value& value) {
            if (!is_array) throw std::runtime_error("Table is not an array");
            data[Value(key)] = value;
        }

        Value& operator[](const Value& key) {
            return data[key];
        }

        bool exists(const Value& key) const {
            return data.find(key) != data.end();
        }

        bool get_is_array() const { return is_array; }
        std::variant<std::map<Value, Value>, std::vector<Value>> get_data() const {
            if (is_array) {
                std::vector<Value> vec;
                for (const auto& pair : data) {
                    vec.push_back(pair.second);
                }
                return vec;
            }
            return data;
        }

        std::map<Value, Value> get_data_map() const {
            if (!is_array) {
                std::map<Value, Value> map;
                for (const auto& pair : data) {
                    map[pair.first] = pair.second;
                }
                return map;
            }
            throw std::runtime_error("Table is not a map");
        }

        std::vector<Value> get_data_vector() const {
            if (is_array) {
                std::vector<Value> vec;
                for (const auto& pair : data) {
                    vec.push_back(pair.second);
                }
                return vec;
            }
            throw std::runtime_error("Table is not an array");
        }

        std::string serialize() const;
        static Table deserialize(const std::string& data);
    };

inline std::string Value::serialize() const {
    std::stringstream ss;
    if (is_int()) {
        ss << as_int();
    } else if (is_float()) {
        ss << std::fixed << std::setprecision(1) << as_float();
    } else if (is_bool()) {
        ss << (as_bool() ? "true" : "false");
    } else if (is_string()) {
        ss << "\"" << as_string() << "\"";
    } else if (is_table()) {
        ss << as_table().serialize();
    }
    return ss.str();
}

inline Value Value::deserialize(const std::string& data) {
    if (data.empty()) throw std::runtime_error("Empty data");

    // test if it's a number
    try {
        if (data.find('.') != std::string::npos) {
            return Value(std::stof(data));
        } else {
            return Value(std::stoi(data));
        }
    } catch (...) {}

    // test if it's a bool
    if (data == "true") return Value(true);
    if (data == "false") return Value(false);

    // test if it's a string (quoted)
    if (data.length() >= 2 && data[0] == '"' && data.back() == '"') {
        return Value(data.substr(1, data.length() - 2));
    }
    
    // test if it's a table
    if (data[0] == '[' || data[0] == '{') {
        return Value(Table::deserialize(data));
    }

    // default to string
    return Value(data);
}

// serialize the table
inline std::string Table::serialize() const {
    if (is_array) {
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& pair : data) {
            if (!first) ss << ",";
            first = false;
            ss << pair.second.serialize();
        }
        ss << "]";
        return ss.str();
    }
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& pair : data) {
        if (!first) ss << ",";
        first = false;
        ss << pair.first.serialize() << "=" << pair.second.serialize();
    }
    ss << "}";
    return ss.str();
}

inline Table Table::deserialize(const std::string& data) {
    if (data.empty()) throw std::runtime_error("Empty data");
    
    if (data[0] == '[') {
        if (data.length() < 2 || data.back() != ']') 
            throw std::runtime_error("Malformed array");
            
        if (data.length() == 2) // empty array == "[]"
            return Table(std::vector<Value>());
            
        std::vector<Value> vec;
        std::string content = data.substr(1, data.length() - 2);
        size_t pos = 0;
        size_t next;
        int depth = 0;
        std::string item;
        
        for (size_t i = 0; i < content.length(); i++) {
            char c = content[i];
            if (c == '[' || c == '{') depth++;
            else if (c == ']' || c == '}') depth--;
            else if (c == ',' && depth == 0) {
                item = content.substr(pos, i - pos);
                if (!item.empty()) {
                    // get rid of whitespace
                    item.erase(0, item.find_first_not_of(" \t"));
                    item.erase(item.find_last_not_of(" \t") + 1);
                    if (!item.empty())
                        vec.push_back(Value::deserialize(item));
                }
                pos = i + 1;
            }
        }
        
        item = content.substr(pos);
        // only add final item if it's not empty (for trailing commas)
        if (!item.empty()) {
            // get rid of whitespace
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            if (!item.empty())
                vec.push_back(Value::deserialize(item));
        }
            
        return Table(vec);
    } 
    else if (data[0] == '{') {
        if (data.length() < 2 || data.back() != '}') 
            throw std::runtime_error("Malformed table");
            
        if (data.length() == 2) // empty table "{}"
            return Table();
            
        std::map<Value, Value> map;
        std::string content = data.substr(1, data.length() - 2);
        size_t pos = 0;
        size_t next;
        int depth = 0;
        std::string item;
        
        for (size_t i = 0; i < content.length(); i++) {
            char c = content[i];
            if (c == '[' || c == '{') depth++;
            else if (c == ']' || c == '}') depth--;
            else if (c == ',' && depth == 0) {
                item = content.substr(pos, i - pos);
                if (!item.empty()) {
                    // Trim whitespace
                    item.erase(0, item.find_first_not_of(" \t"));
                    item.erase(item.find_last_not_of(" \t") + 1);
                    if (!item.empty()) {
                        size_t equals = item.find('=');
                        if (equals == std::string::npos) 
                            throw std::runtime_error("Invalid table format: missing '='");
                        std::string key = item.substr(0, equals);
                        std::string value = item.substr(equals + 1);
                        // Trim whitespace from key and value
                        key.erase(0, key.find_first_not_of(" \t"));
                        key.erase(key.find_last_not_of(" \t") + 1);
                        value.erase(0, value.find_first_not_of(" \t"));
                        value.erase(value.find_last_not_of(" \t") + 1);
                        if (!key.empty() && !value.empty())
                            map[Value::deserialize(key)] = Value::deserialize(value);
                    }
                }
                pos = i + 1;
            }
        }
        
        item = content.substr(pos);
        // only process final item if it's not empty
        if (!item.empty()) {
            // get rid of whitespace
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            if (!item.empty()) {
                size_t equals = item.find('=');
                if (equals == std::string::npos) 
                    throw std::runtime_error("Invalid table format: missing '='");
                std::string key = item.substr(0, equals);
                std::string value = item.substr(equals + 1);

                // get rid of whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                if (!key.empty() && !value.empty())
                    map[Value::deserialize(key)] = Value::deserialize(value);
            }
        }
        
        return Table(map);
    }
    throw std::runtime_error("Unknown type");
}

// Implementation of comparison operators
inline bool operator<(const Value& lhs, const Value& rhs) {
    if (lhs.data.index() != rhs.data.index())
        return lhs.data.index() < rhs.data.index();
        
    if (lhs.is_int())
        return lhs.as_int() < rhs.as_int();
    if (lhs.is_float())
        return lhs.as_float() < rhs.as_float();
    if (lhs.is_bool())
        return lhs.as_bool() < rhs.as_bool();
    if (lhs.is_string())
        return lhs.as_string() < rhs.as_string();
    if (lhs.is_table())
        return &lhs.as_table() < &rhs.as_table(); // compare pointers for tables for now
        
    return false;
}

inline bool operator==(const Value& lhs, const Value& rhs) {
    if (lhs.data.index() != rhs.data.index())
        return false;
        
    if (lhs.is_int())
        return lhs.as_int() == rhs.as_int();
    if (lhs.is_float())
        return lhs.as_float() == rhs.as_float();
    if (lhs.is_bool())
        return lhs.as_bool() == rhs.as_bool();
    if (lhs.is_string())
        return lhs.as_string() == rhs.as_string();
    if (lhs.is_table())
        return &lhs.as_table() == &rhs.as_table(); // compare pointers for tables for now
        
    return true;
}

inline std::string serialize_to_netvent(const Value& event_name, const std::map<std::string, Value>& data) {
    std::stringstream ss;
    ss << event_name.serialize() << "\n";

    for (const auto& pair : data) {
        ss << pair.first << " " << pair.second.serialize() << "\n";
    }

    return ss.str();
}

inline std::pair<Value, std::map<std::string, Value>> deserialize_from_netvent(std::string data) {
    std::map<std::string, Value> result;
    std::stringstream ss(data);
    std::string line;
    Value event_name;

    // get out the comments and tabs
    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty() || line.substr(0, 2) == "//" || line[0] == '#') 
            continue;
            
        event_name = Value::deserialize(line);
        break;
    }

    // process kv pairs
    while (std::getline(ss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        
        // remove inline comments
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
            line.erase(line.find_last_not_of(" \t") + 1);
        }
        
        // skip empty lines or comments
        if (line.empty() || line.substr(0, 2) == "//" || line[0] == '#') 
            continue;

        // find the space between key and value
        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos) 
            continue;

        std::string key = line.substr(0, space_pos);
        // find first non-whitespace character after key
        size_t value_start = line.find_first_not_of(" \t", space_pos);
        if (value_start == std::string::npos) 
            continue;
            
        std::string value = line.substr(value_start);
        result[key] = Value::deserialize(value);
    }

    return std::make_pair(event_name, result);
}

inline std::string to_string(const Value& value) {
    return value.serialize();
}

inline std::string to_string(const Table& table) {
    return table.serialize();
}

inline Value from_string(const std::string& str) {
    return Value::deserialize(str);
}

template<typename T>
inline Value val(T&& v) {
    return Value(std::forward<T>(v));
}

// Shorthand for creating tables
inline Table arr_table(std::initializer_list<Value> init) {
    return Table(init);
}

inline Table map_table(std::initializer_list<std::pair<const char*, Value>> init) {
    return Table(init);
}

// example usage:
// auto player = map_table({`
//     {"name", "John"},
//     {"position", arr_table({10, 20})},
//     {"health", 100},
//     {"inventory", arr_table({
//         map_table({{"item", "sword"}, {"damage", 15}}),
//         map_table({{"item", "potion"}, {"healing", 20}})
//     })}
// });
// std::string data = to_string(player);
// Value restored = from_string(data);

} // namespace netvent

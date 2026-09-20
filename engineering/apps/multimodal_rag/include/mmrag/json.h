/**
 * @file json.h
 * @brief Minimal JSON implementation for serialization
 *
 * Provides basic JSON serialization/deserialization for simple types.
 * Avoids the heavy nlohmann/json dependency.
 */
#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cctype>

namespace nlohmann {

/**
 * @brief Minimal JSON value
 */
class json {
public:
    enum class Type { OBJECT, ARRAY, STRING, NUMBER, BOOL, NULL_VAL };

    json() : type_(Type::NULL_VAL), num_(0), bool_(false) {}
    explicit json(Type type) : type_(type), num_(0), bool_(false) {}
    json(const std::string& val) : type_(Type::STRING), str_(val), num_(0), bool_(false) {}
    json(double val) : type_(Type::NUMBER), num_(val), bool_(false) {}
    json(int val) : type_(Type::NUMBER), num_(val), bool_(false) {}
    json(int64_t val) : type_(Type::NUMBER), num_(static_cast<double>(val)), bool_(false) {}
    json(uint64_t val) : type_(Type::NUMBER), num_(static_cast<double>(val)), bool_(false) {}
    json(bool val) : type_(Type::BOOL), bool_(val), num_(0) {}

    static json object() { return json(Type::OBJECT); }
    static json array() { return json(Type::ARRAY); }

    static json parse(const std::string& str) {
        json j;
        size_t pos = skip_ws(str, 0);
        if (pos >= str.size()) return j;
        parse_value(str, pos, j);
        return j;
    }

    // Get type
    bool is_object() const { return type_ == Type::OBJECT; }
    bool is_string() const { return type_ == Type::STRING; }
    bool is_number() const { return type_ == Type::NUMBER; }
    bool is_bool() const { return type_ == Type::BOOL; }
    bool is_null() const { return type_ == Type::NULL_VAL; }

    // Object access
    json& operator[](const std::string& key) {
        if (type_ != Type::OBJECT) {
            type_ = Type::OBJECT;
            obj_.clear();
        }
        return obj_[key];
    }

    const json& at(const std::string& key) const {
        static json null_val;
        auto it = obj_.find(key);
        return (it != obj_.end()) ? it->second : null_val;
    }

    json& at(const std::string& key) {
        if (type_ != Type::OBJECT) {
            type_ = Type::OBJECT;
            obj_.clear();
        }
        return obj_[key];
    }

    bool contains(const std::string& key) const { return obj_.count(key) > 0; }

    // Array access
    json& operator[](size_t idx) {
        if (type_ != Type::ARRAY) {
            type_ = Type::ARRAY;
            arr_.clear();
        }
        if (idx >= arr_.size()) arr_.resize(idx + 1);
        return arr_[idx];
    }

    size_t size() const {
        if (type_ == Type::ARRAY) return arr_.size();
        if (type_ == Type::OBJECT) return obj_.size();
        return 0;
    }

    // Get values
    template<typename T>
    T get() const;

    template<typename T>
    T get_value(const std::string& key, T default_val) const {
        auto it = obj_.find(key);
        if (it == obj_.end()) return default_val;
        return it->second.get<T>();
    }

    // value() is an alias for get_value() (nlohmann/json compatibility)
    template<typename T>
    T value(const std::string& key, T default_val) const {
        return get_value<T>(key, default_val);
    }

    // Set values
    void set_string(const std::string& key, const std::string& val) {
        if (type_ != Type::OBJECT) { type_ = Type::OBJECT; obj_.clear(); }
        obj_[key] = json(val);
    }

    void set_number(const std::string& key, double val) {
        if (type_ != Type::OBJECT) { type_ = Type::OBJECT; obj_.clear(); }
        obj_[key] = json(val);
    }

    void set_int(const std::string& key, int64_t val) {
        if (type_ != Type::OBJECT) { type_ = Type::OBJECT; obj_.clear(); }
        obj_[key] = json(static_cast<double>(val));
    }

    void push_back(const json& val) {
        if (type_ != Type::ARRAY) { type_ = Type::ARRAY; arr_.clear(); }
        arr_.push_back(val);
    }

    // Serialize
    std::string dump() const {
        std::ostringstream oss;
        serialize(oss);
        return oss.str();
    }

private:
    Type type_;
    std::map<std::string, json> obj_;
    std::vector<json> arr_;
    std::string str_;
    double num_;
    bool bool_;

    static size_t skip_ws(const std::string& s, size_t pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
        return pos;
    }

    static size_t parse_value(const std::string& s, size_t pos, json& result) {
        pos = skip_ws(s, pos);
        if (pos >= s.size()) return pos;

        char c = s[pos];
        if (c == '{') return parse_object(s, pos, result);
        if (c == '[') return parse_array(s, pos, result);
        if (c == '"') return parse_string(s, pos, result);
        if (c == 't' || c == 'f') return parse_bool(s, pos, result);
        if (c == 'n') return parse_null(s, pos, result);
        if (c == '-' || std::isdigit(c)) return parse_number(s, pos, result);
        return pos;
    }

    static size_t parse_object(const std::string& s, size_t pos, json& result) {
        result.type_ = Type::OBJECT;
        result.obj_.clear();
        pos++;  // skip '{'
        pos = skip_ws(s, pos);
        if (pos < s.size() && s[pos] == '}') return pos + 1;  // empty object

        while (true) {
            pos = skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != '"') break;
            std::string key;
            pos = parse_string(s, pos, key);
            pos = skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != ':') break;
            pos++;  // skip ':'
            json val;
            pos = parse_value(s, pos, val);
            result.obj_[key] = val;
            pos = skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != ',') break;
            pos++;  // skip ','
        }
        if (pos < s.size() && s[pos] == '}') pos++;
        return pos;
    }

    static size_t parse_string(const std::string& s, size_t pos, std::string& result) {
        result.clear();
        if (pos >= s.size() || s[pos] != '"') return pos;
        pos++;  // skip opening '"'
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'u':
                        if (pos + 4 < s.size()) {
                            result += s[pos+1]; pos += 4;
                        }
                        break;
                    default: result += s[pos]; break;
                }
            } else {
                result += s[pos];
            }
            pos++;
        }
        if (pos < s.size() && s[pos] == '"') pos++;
        return pos;
    }

    static size_t parse_string(const std::string& s, size_t pos, json& result) {
        std::string val;
        pos = parse_string(s, pos, val);
        result.type_ = Type::STRING;
        result.str_ = val;
        return pos;
    }

    static size_t parse_number(const std::string& s, size_t pos, json& result) {
        size_t start = pos;
        if (pos < s.size() && s[pos] == '-') pos++;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        if (pos < s.size() && s[pos] == '.') {
            pos++;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            pos++;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        }
        result.type_ = Type::NUMBER;
        result.num_ = std::stod(s.substr(start, pos - start));
        return pos;
    }

    static size_t parse_bool(const std::string& s, size_t pos, json& result) {
        if (s.substr(pos, 4) == "true") {
            result.type_ = Type::BOOL;
            result.bool_ = true;
            return pos + 4;
        }
        if (s.substr(pos, 5) == "false") {
            result.type_ = Type::BOOL;
            result.bool_ = false;
            return pos + 5;
        }
        return pos;
    }

    static size_t parse_null(const std::string& s, size_t pos, json& result) {
        if (s.substr(pos, 4) == "null") {
            result.type_ = Type::NULL_VAL;
            return pos + 4;
        }
        return pos;
    }

    static size_t parse_array(const std::string& s, size_t pos, json& result) {
        result.type_ = Type::ARRAY;
        result.arr_.clear();
        pos++;  // skip '['
        pos = skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ']') return pos + 1;

        while (true) {
            json val;
            pos = parse_value(s, pos, val);
            result.arr_.push_back(val);
            pos = skip_ws(s, pos);
            if (pos >= s.size() || s[pos] != ',') break;
            pos++;
        }
        if (pos < s.size() && s[pos] == ']') pos++;
        return pos;
    }

    void serialize(std::ostringstream& oss) const {
        switch (type_) {
            case Type::OBJECT: {
                oss << "{";
                bool first = true;
                for (const auto& [k, v] : obj_) {
                    if (!first) oss << ",";
                    first = false;
                    oss << "\"" << escape_string(k) << "\":";
                    v.serialize(oss);
                }
                oss << "}";
                break;
            }
            case Type::ARRAY: {
                oss << "[";
                for (size_t i = 0; i < arr_.size(); i++) {
                    if (i > 0) oss << ",";
                    arr_[i].serialize(oss);
                }
                oss << "]";
                break;
            }
            case Type::STRING:
                oss << "\"" << escape_string(str_) << "\"";
                break;
            case Type::NUMBER:
                oss << num_;
                break;
            case Type::BOOL:
                oss << (bool_ ? "true" : "false");
                break;
            case Type::NULL_VAL:
                oss << "null";
                break;
        }
    }

    static std::string escape_string(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default: oss << c; break;
            }
        }
        return oss.str();
    }
};

// Template specializations
template<>
inline std::string json::get<std::string>() const {
    if (type_ == Type::STRING) return str_;
    if (type_ == Type::NUMBER) return std::to_string(num_);
    if (type_ == Type::BOOL) return bool_ ? "true" : "false";
    return "";
}

template<>
inline int json::get<int>() const {
    if (type_ == Type::NUMBER) return static_cast<int>(num_);
    return 0;
}

template<>
inline int64_t json::get<int64_t>() const {
    if (type_ == Type::NUMBER) return static_cast<int64_t>(num_);
    return 0;
}

template<>
inline double json::get<double>() const {
    if (type_ == Type::NUMBER) return num_;
    return 0;
}

template<>
inline bool json::get<bool>() const {
    if (type_ == Type::BOOL) return bool_;
    return false;
}

template<>
inline const char* json::get<const char*>() const {
    // Not actually possible to return a pointer to str_ since it's temporary
    // This is a workaround — callers should use get<std::string>() instead
    static thread_local std::string buf;
    buf = get<std::string>();
    return buf.c_str();
}

}  // namespace nlohmann

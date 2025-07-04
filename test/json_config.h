#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <stdexcept>

/**
 * Simple JSON parser for test configuration
 */
class JsonValue {
public:
    enum Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    JsonValue() : type(Null) {}
    JsonValue(bool v) : type(Boolean), boolValue(v) {}
    JsonValue(int64_t v) : type(Number), numberValue(v) {}
    JsonValue(const std::string& v) : type(String), stringValue(v) {}
    
    Type getType() const { return type; }
    bool isNull() const { return type == Null; }
    
    bool asBool(bool defaultValue = false) const {
        return type == Boolean ? boolValue : defaultValue;
    }
    
    int64_t asInt64(int64_t defaultValue = 0) const {
        return type == Number ? numberValue : defaultValue;
    }
    
    std::string asString(const std::string& defaultValue = "") const {
        return type == String ? stringValue : defaultValue;
    }
    
    JsonValue& operator[](const std::string& key) {
        type = Object;
        return objectValue[key];
    }
    
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue nullValue;
        if (type != Object) return nullValue;
        auto it = objectValue.find(key);
        return it != objectValue.end() ? it->second : nullValue;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        write(oss);
        return oss.str();
    }

private:
    Type type;
    bool boolValue;
    int64_t numberValue;
    std::string stringValue;
    std::map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;

    void write(std::ostream& os, int indent = 0) const {
        switch (type) {
            case Null:
                os << "null";
                break;
            case Boolean:
                os << (boolValue ? "true" : "false");
                break;
            case Number:
                os << numberValue;
                break;
            case String:
                os << '"' << escape(stringValue) << '"';
                break;
            case Array:
                os << "[\n";
                for (size_t i = 0; i < arrayValue.size(); ++i) {
                    os << std::string(indent + 2, ' ');
                    arrayValue[i].write(os, indent + 2);
                    if (i < arrayValue.size() - 1) os << ',';
                    os << '\n';
                }
                os << std::string(indent, ' ') << ']';
                break;
            case Object:
                os << "{\n";
                size_t count = 0;
                for (const auto& [key, value] : objectValue) {
                    os << std::string(indent + 2, ' ') << '"' << escape(key) << "\": ";
                    value.write(os, indent + 2);
                    if (++count < objectValue.size()) os << ',';
                    os << '\n';
                }
                os << std::string(indent, ' ') << '}';
                break;
        }
    }

    static std::string escape(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }

public:
    static JsonValue parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.length() && std::isspace(json[pos])) pos++;
    }

    static JsonValue parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.length()) throw std::runtime_error("Unexpected end of input");

        char c = json[pos];
        if (c == '{') return parseObject(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '"') return parseString(json, pos);
        if (std::isdigit(c) || c == '-') return parseNumber(json, pos);
        if (c == 't' || c == 'f') return parseBoolean(json, pos);
        if (c == 'n') return parseNull(json, pos);

        throw std::runtime_error("Invalid JSON value");
    }

    static JsonValue parseObject(const std::string& json, size_t& pos) {
        JsonValue result;
        result.type = Object;
        pos++; // Skip '{'

        skipWhitespace(json, pos);
        while (pos < json.length() && json[pos] != '}') {
            if (json[pos] != '"') throw std::runtime_error("Expected property name");
            
            std::string key = parseString(json, pos).stringValue;
            skipWhitespace(json, pos);
            
            if (pos >= json.length() || json[pos] != ':') 
                throw std::runtime_error("Expected ':'");
            pos++;
            
            result.objectValue[key] = parseValue(json, pos);
            skipWhitespace(json, pos);
            
            if (pos < json.length() && json[pos] == ',') {
                pos++;
                skipWhitespace(json, pos);
            }
        }
        
        if (pos >= json.length() || json[pos] != '}')
            throw std::runtime_error("Expected '}'");
        pos++;
        
        return result;
    }

    static JsonValue parseArray(const std::string& json, size_t& pos) {
        JsonValue result;
        result.type = Array;
        pos++; // Skip '['

        skipWhitespace(json, pos);
        while (pos < json.length() && json[pos] != ']') {
            result.arrayValue.push_back(parseValue(json, pos));
            skipWhitespace(json, pos);
            
            if (pos < json.length() && json[pos] == ',') {
                pos++;
                skipWhitespace(json, pos);
            }
        }
        
        if (pos >= json.length() || json[pos] != ']')
            throw std::runtime_error("Expected ']'");
        pos++;
        
        return result;
    }

    static JsonValue parseString(const std::string& json, size_t& pos) {
        std::string result;
        pos++; // Skip opening quote

        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.length()) throw std::runtime_error("Unexpected end of string");
                
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos + 4 >= json.length())
                            throw std::runtime_error("Invalid unicode escape");
                        // Skip unicode handling for simplicity
                        pos += 4;
                        break;
                    }
                    default:
                        throw std::runtime_error("Invalid escape sequence");
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        
        if (pos >= json.length() || json[pos] != '"')
            throw std::runtime_error("Unterminated string");
        pos++;
        
        return JsonValue(result);
    }

    static JsonValue parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        while (pos < json.length() && 
               (std::isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.'))
            pos++;
            
        std::string num = json.substr(start, pos - start);
        return JsonValue(std::stoll(num));
    }

    static JsonValue parseBoolean(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JsonValue(true);
        }
        if (json.compare(pos, 5, "false") == 0) {
            pos += 5;
            return JsonValue(false);
        }
        throw std::runtime_error("Invalid boolean value");
    }

    static JsonValue parseNull(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "null") == 0) {
            pos += 4;
            return JsonValue();
        }
        throw std::runtime_error("Invalid null value");
    }
};

#define CONFIG_VALUE(j, key, type, default) \
    ((j)[key].getType() == JsonValue::type ? (j)[key] : JsonValue(default))
// JSON for Modern C++ - Single Header
// Version 3.11.3
// https://github.com/nlohmann/json
// SPDX-License-Identifier: MIT
// Copyright (c) 2013-2023 Niels Lohmann

// NOTE: This is a minimal subset for ARC-Estimate project serialization
// For full version, download from https://github.com/nlohmann/json/releases

#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <Windows.h>

namespace nlohmann
{
    // Helper functions for UTF-8 <-> wstring conversion using Windows API
    inline std::string WstringToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

    inline std::wstring Utf8ToWstring(const std::string& str)
    {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
        return result;
    }

    class json
    {
    public:
        enum class value_t
        {
            null,
            object,
            array,
            string,
            boolean,
            number_integer,
            number_float
        };

    private:
        value_t m_type{ value_t::null };
        
        union json_value
        {
            std::map<std::string, json>* object;
            std::vector<json>* array;
            std::string* string;
            bool boolean;
            int64_t number_integer;
            double number_float;
            
            json_value() = default;
            json_value(bool v) : boolean(v) {}
            json_value(int64_t v) : number_integer(v) {}
            json_value(double v) : number_float(v) {}
        } m_value{};

        void destroy()
        {
            switch (m_type)
            {
            case value_t::object:
                delete m_value.object;
                break;
            case value_t::array:
                delete m_value.array;
                break;
            case value_t::string:
                delete m_value.string;
                break;
            default:
                break;
            }
            m_type = value_t::null;
        }

        void copy_from(const json& other)
        {
            m_type = other.m_type;
            switch (m_type)
            {
            case value_t::object:
                m_value.object = new std::map<std::string, json>(*other.m_value.object);
                break;
            case value_t::array:
                m_value.array = new std::vector<json>(*other.m_value.array);
                break;
            case value_t::string:
                m_value.string = new std::string(*other.m_value.string);
                break;
            case value_t::boolean:
                m_value.boolean = other.m_value.boolean;
                break;
            case value_t::number_integer:
                m_value.number_integer = other.m_value.number_integer;
                break;
            case value_t::number_float:
                m_value.number_float = other.m_value.number_float;
                break;
            default:
                break;
            }
        }

    public:
        // Constructors
        json() : m_type(value_t::null) {}
        
        json(std::nullptr_t) : m_type(value_t::null) {}
        
        json(bool val) : m_type(value_t::boolean) { m_value.boolean = val; }
        
        json(int val) : m_type(value_t::number_integer) { m_value.number_integer = val; }
        json(int64_t val) : m_type(value_t::number_integer) { m_value.number_integer = val; }
        json(uint64_t val) : m_type(value_t::number_integer) { m_value.number_integer = static_cast<int64_t>(val); }
        
        json(double val) : m_type(value_t::number_float) { m_value.number_float = val; }
        json(float val) : m_type(value_t::number_float) { m_value.number_float = val; }
        
        json(const std::string& val) : m_type(value_t::string) { m_value.string = new std::string(val); }
        json(const char* val) : m_type(value_t::string) { m_value.string = new std::string(val); }
        
        json(const std::wstring& val) : m_type(value_t::string) 
        { 
            m_value.string = new std::string(WstringToUtf8(val));
        }
        
        // Copy constructor
        json(const json& other) { copy_from(other); }
        
        // Move constructor
        json(json&& other) noexcept : m_type(other.m_type), m_value(other.m_value)
        {
            other.m_type = value_t::null;
        }

        // Destructor
        ~json() { destroy(); }

        // Assignment
        json& operator=(const json& other)
        {
            if (this != &other)
            {
                destroy();
                copy_from(other);
            }
            return *this;
        }

        json& operator=(json&& other) noexcept
        {
            if (this != &other)
            {
                destroy();
                m_type = other.m_type;
                m_value = other.m_value;
                other.m_type = value_t::null;
            }
            return *this;
        }

        // Type checks
        bool is_null() const { return m_type == value_t::null; }
        bool is_object() const { return m_type == value_t::object; }
        bool is_array() const { return m_type == value_t::array; }
        bool is_string() const { return m_type == value_t::string; }
        bool is_boolean() const { return m_type == value_t::boolean; }
        bool is_number() const { return m_type == value_t::number_integer || m_type == value_t::number_float; }
        bool is_number_integer() const { return m_type == value_t::number_integer; }
        bool is_number_float() const { return m_type == value_t::number_float; }

        // Static constructors
        static json object()
        {
            json j;
            j.m_type = value_t::object;
            j.m_value.object = new std::map<std::string, json>();
            return j;
        }

        static json array()
        {
            json j;
            j.m_type = value_t::array;
            j.m_value.array = new std::vector<json>();
            return j;
        }

        // Object access
        json& operator[](const std::string& key)
        {
            if (m_type == value_t::null)
            {
                m_type = value_t::object;
                m_value.object = new std::map<std::string, json>();
            }
            if (m_type != value_t::object)
                throw std::runtime_error("Cannot use operator[] with non-object type");
            return (*m_value.object)[key];
        }

        json& operator[](const char* key)
        {
            return (*this)[std::string(key)];
        }

        const json& operator[](const std::string& key) const
        {
            if (m_type != value_t::object)
                throw std::runtime_error("Cannot use operator[] with non-object type");
            auto it = m_value.object->find(key);
            if (it == m_value.object->end())
            {
                static json null_json;
                return null_json;
            }
            return it->second;
        }

        const json& operator[](const char* key) const
        {
            return (*this)[std::string(key)];
        }

        // Array access
        json& operator[](size_t idx)
        {
            if (m_type == value_t::null)
            {
                m_type = value_t::array;
                m_value.array = new std::vector<json>();
            }
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot use operator[] with non-array type");
            if (idx >= m_value.array->size())
                m_value.array->resize(idx + 1);
            return (*m_value.array)[idx];
        }

        const json& operator[](size_t idx) const
        {
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot use operator[] with non-array type");
            return (*m_value.array)[idx];
        }

        // Array push_back
        void push_back(const json& val)
        {
            if (m_type == value_t::null)
            {
                m_type = value_t::array;
                m_value.array = new std::vector<json>();
            }
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot use push_back with non-array type");
            m_value.array->push_back(val);
        }

        // Size
        size_t size() const
        {
            switch (m_type)
            {
            case value_t::object:
                return m_value.object->size();
            case value_t::array:
                return m_value.array->size();
            default:
                return 0;
            }
        }

        bool empty() const
        {
            switch (m_type)
            {
            case value_t::object:
                return m_value.object->empty();
            case value_t::array:
                return m_value.array->empty();
            case value_t::null:
                return true;
            default:
                return false;
            }
        }

        // Contains (for objects)
        bool contains(const std::string& key) const
        {
            if (m_type != value_t::object)
                return false;
            return m_value.object->find(key) != m_value.object->end();
        }

        // Value access methods (non-template for simplicity)
        std::string get_string() const
        {
            if (m_type != value_t::string)
                throw std::runtime_error("JSON value is not a string");
            return *m_value.string;
        }

        std::wstring get_wstring() const
        {
            if (m_type != value_t::string)
                throw std::runtime_error("JSON value is not a string");
            return Utf8ToWstring(*m_value.string);
        }

        int get_int() const
        {
            if (m_type == value_t::number_integer)
                return static_cast<int>(m_value.number_integer);
            if (m_type == value_t::number_float)
                return static_cast<int>(m_value.number_float);
            throw std::runtime_error("JSON value is not a number");
        }

        int64_t get_int64() const
        {
            if (m_type == value_t::number_integer)
                return m_value.number_integer;
            if (m_type == value_t::number_float)
                return static_cast<int64_t>(m_value.number_float);
            throw std::runtime_error("JSON value is not a number");
        }

        uint64_t get_uint64() const
        {
            if (m_type == value_t::number_integer)
                return static_cast<uint64_t>(m_value.number_integer);
            if (m_type == value_t::number_float)
                return static_cast<uint64_t>(m_value.number_float);
            throw std::runtime_error("JSON value is not a number");
        }

        double get_double() const
        {
            if (m_type == value_t::number_float)
                return m_value.number_float;
            if (m_type == value_t::number_integer)
                return static_cast<double>(m_value.number_integer);
            throw std::runtime_error("JSON value is not a number");
        }

        float get_float() const
        {
            return static_cast<float>(get_double());
        }

        bool get_bool() const
        {
            if (m_type != value_t::boolean)
                throw std::runtime_error("JSON value is not a boolean");
            return m_value.boolean;
        }

        // Iteration support for arrays
        std::vector<json>::iterator begin()
        {
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot iterate non-array type");
            return m_value.array->begin();
        }

        std::vector<json>::iterator end()
        {
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot iterate non-array type");
            return m_value.array->end();
        }

        std::vector<json>::const_iterator begin() const
        {
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot iterate non-array type");
            return m_value.array->begin();
        }

        std::vector<json>::const_iterator end() const
        {
            if (m_type != value_t::array)
                throw std::runtime_error("Cannot iterate non-array type");
            return m_value.array->end();
        }

        // Object iteration
        std::map<std::string, json>::iterator items_begin()
        {
            if (m_type != value_t::object)
                throw std::runtime_error("Cannot iterate non-object type");
            return m_value.object->begin();
        }

        std::map<std::string, json>::iterator items_end()
        {
            if (m_type != value_t::object)
                throw std::runtime_error("Cannot iterate non-object type");
            return m_value.object->end();
        }

        // Serialization
        std::string dump(int indent = -1) const
        {
            std::ostringstream ss;
            dump_impl(ss, indent, 0);
            return ss.str();
        }

    private:
        static std::string escape_string(const std::string& s)
        {
            std::ostringstream ss;
            for (char c : s)
            {
                switch (c)
                {
                case '"': ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\b': ss << "\\b"; break;
                case '\f': ss << "\\f"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    }
                    else
                    {
                        ss << c;
                    }
                }
            }
            return ss.str();
        }

        void dump_impl(std::ostringstream& ss, int indent, int current_indent) const
        {
            std::string indent_str = (indent >= 0) ? std::string(current_indent, ' ') : "";
            std::string child_indent_str = (indent >= 0) ? std::string(current_indent + indent, ' ') : "";
            std::string newline = (indent >= 0) ? "\n" : "";
            std::string space = (indent >= 0) ? " " : "";

            switch (m_type)
            {
            case value_t::null:
                ss << "null";
                break;
            case value_t::boolean:
                ss << (m_value.boolean ? "true" : "false");
                break;
            case value_t::number_integer:
                ss << m_value.number_integer;
                break;
            case value_t::number_float:
                if (std::isfinite(m_value.number_float))
                {
                    ss << std::setprecision(15) << m_value.number_float;
                }
                else
                {
                    ss << "null";
                }
                break;
            case value_t::string:
                ss << "\"" << escape_string(*m_value.string) << "\"";
                break;
            case value_t::array:
                if (m_value.array->empty())
                {
                    ss << "[]";
                }
                else
                {
                    ss << "[" << newline;
                    for (size_t i = 0; i < m_value.array->size(); ++i)
                    {
                        ss << child_indent_str;
                        (*m_value.array)[i].dump_impl(ss, indent, current_indent + indent);
                        if (i < m_value.array->size() - 1)
                            ss << ",";
                        ss << newline;
                    }
                    ss << indent_str << "]";
                }
                break;
            case value_t::object:
                if (m_value.object->empty())
                {
                    ss << "{}";
                }
                else
                {
                    ss << "{" << newline;
                    size_t i = 0;
                    for (const auto& kv : *m_value.object)
                    {
                        ss << child_indent_str << "\"" << escape_string(kv.first) << "\":" << space;
                        kv.second.dump_impl(ss, indent, current_indent + indent);
                        if (i < m_value.object->size() - 1)
                            ss << ",";
                        ss << newline;
                        ++i;
                    }
                    ss << indent_str << "}";
                }
                break;
            }
        }

    public:
        // Parsing
        static json parse(const std::string& str)
        {
            size_t pos = 0;
            return parse_value(str, pos);
        }

    private:
        static void skip_whitespace(const std::string& str, size_t& pos)
        {
            while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r'))
                ++pos;
        }

        static json parse_value(const std::string& str, size_t& pos)
        {
            skip_whitespace(str, pos);
            if (pos >= str.size())
                throw std::runtime_error("Unexpected end of JSON input");

            char c = str[pos];
            if (c == '{')
                return parse_object(str, pos);
            if (c == '[')
                return parse_array(str, pos);
            if (c == '"')
                return parse_string(str, pos);
            if (c == 't' || c == 'f')
                return parse_boolean(str, pos);
            if (c == 'n')
                return parse_null(str, pos);
            if (c == '-' || (c >= '0' && c <= '9'))
                return parse_number(str, pos);

            throw std::runtime_error("Unexpected character in JSON: " + std::string(1, c));
        }

        static json parse_object(const std::string& str, size_t& pos)
        {
            json obj = json::object();
            ++pos; // skip '{'
            skip_whitespace(str, pos);

            if (pos < str.size() && str[pos] == '}')
            {
                ++pos;
                return obj;
            }

            while (true)
            {
                skip_whitespace(str, pos);
                if (pos >= str.size() || str[pos] != '"')
                    throw std::runtime_error("Expected string key in object");

                json key_json = parse_string(str, pos);
                std::string key = key_json.get_string();

                skip_whitespace(str, pos);
                if (pos >= str.size() || str[pos] != ':')
                    throw std::runtime_error("Expected ':' after key in object");
                ++pos;

                obj[key] = parse_value(str, pos);

                skip_whitespace(str, pos);
                if (pos >= str.size())
                    throw std::runtime_error("Unexpected end of object");

                if (str[pos] == '}')
                {
                    ++pos;
                    break;
                }
                if (str[pos] != ',')
                    throw std::runtime_error("Expected ',' or '}' in object");
                ++pos;
            }

            return obj;
        }

        static json parse_array(const std::string& str, size_t& pos)
        {
            json arr = json::array();
            ++pos; // skip '['
            skip_whitespace(str, pos);

            if (pos < str.size() && str[pos] == ']')
            {
                ++pos;
                return arr;
            }

            while (true)
            {
                arr.push_back(parse_value(str, pos));

                skip_whitespace(str, pos);
                if (pos >= str.size())
                    throw std::runtime_error("Unexpected end of array");

                if (str[pos] == ']')
                {
                    ++pos;
                    break;
                }
                if (str[pos] != ',')
                    throw std::runtime_error("Expected ',' or ']' in array");
                ++pos;
            }

            return arr;
        }

        static json parse_string(const std::string& str, size_t& pos)
        {
            ++pos; // skip opening '"'
            std::string result;

            while (pos < str.size())
            {
                char c = str[pos];
                if (c == '"')
                {
                    ++pos;
                    return json(result);
                }
                if (c == '\\')
                {
                    ++pos;
                    if (pos >= str.size())
                        throw std::runtime_error("Unexpected end of string escape");
                    char escaped = str[pos];
                    switch (escaped)
                    {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u':
                        // Parse 4 hex digits
                        if (pos + 4 >= str.size())
                            throw std::runtime_error("Invalid unicode escape");
                        {
                            std::string hex = str.substr(pos + 1, 4);
                            int codepoint = std::stoi(hex, nullptr, 16);
                            if (codepoint < 0x80)
                            {
                                result += static_cast<char>(codepoint);
                            }
                            else if (codepoint < 0x800)
                            {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            else
                            {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            pos += 4;
                        }
                        break;
                    default:
                        result += escaped;
                    }
                }
                else
                {
                    result += c;
                }
                ++pos;
            }

            throw std::runtime_error("Unterminated string");
        }

        static json parse_number(const std::string& str, size_t& pos)
        {
            size_t start = pos;
            bool is_float = false;

            if (str[pos] == '-')
                ++pos;

            while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9')
                ++pos;

            if (pos < str.size() && str[pos] == '.')
            {
                is_float = true;
                ++pos;
                while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9')
                    ++pos;
            }

            if (pos < str.size() && (str[pos] == 'e' || str[pos] == 'E'))
            {
                is_float = true;
                ++pos;
                if (pos < str.size() && (str[pos] == '+' || str[pos] == '-'))
                    ++pos;
                while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9')
                    ++pos;
            }

            std::string num_str = str.substr(start, pos - start);
            if (is_float)
                return json(std::stod(num_str));
            else
                return json(static_cast<int64_t>(std::stoll(num_str)));
        }

        static json parse_boolean(const std::string& str, size_t& pos)
        {
            if (str.compare(pos, 4, "true") == 0)
            {
                pos += 4;
                return json(true);
            }
            if (str.compare(pos, 5, "false") == 0)
            {
                pos += 5;
                return json(false);
            }
            throw std::runtime_error("Invalid boolean value");
        }

        static json parse_null(const std::string& str, size_t& pos)
        {
            if (str.compare(pos, 4, "null") == 0)
            {
                pos += 4;
                return json(nullptr);
            }
            throw std::runtime_error("Invalid null value");
        }
    };

} // namespace nlohmann

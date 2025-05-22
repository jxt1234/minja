/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h" // For GetParseError_En

static void _printlog(const std::string& i) {
    printf("%s\n", i.c_str());
}


namespace minja {
enum ObjectType {
    JSON_NULL = 0,
    JSON_INT64 = 1,
    JSON_DOUBLE = 2,
    JSON_STRING = 3,
    JSON_BOOL = 4,
};
class json {
public:
    ObjectType mType;
    std::string mString;
    int64_t mInt;
    double mDouble;
    bool mBool;
    json() {
        mType = JSON_NULL;
    }
    json(bool v) {
        mBool = v;
        mType = JSON_BOOL;
    }
    json(int64_t v) {
        mInt = v;
        mType = JSON_INT64;
    }
    json(double v) {
        mDouble = v;
        mType = JSON_DOUBLE;
    }
    json(std::string v) {
        mString = v;
        mType = JSON_STRING;
    }
    json(const char* c, size_t len) {
        mString.assign(c, len);
        mType = JSON_STRING;
    }
    json(const json& right) {
        mType = right.mType;
        mInt = right.mInt;
        mBool = right.mBool;
        mDouble = right.mDouble;
        mString = right.mString;
    }
    bool operator==(const json& right) const {
        if (mType != right.mType) {
            return false;
        }
        switch (mType) {
            case JSON_STRING:
                return mString == right.mString;
            case JSON_INT64:
                return mInt == right.mInt;
            case JSON_DOUBLE:
                return mDouble == right.mDouble;
            case JSON_BOOL:
                return mBool == right.mBool;
            default:
                break;
        }
        return true;
    }
    bool is_string() const {
        return mType == JSON_STRING;
    }
    bool is_null() const { return mType == JSON_NULL; }
    bool is_boolean() const { return mType == JSON_BOOL; }
    bool is_number_integer() const { return mType == JSON_INT64; }
    bool is_number_float() const { return mType == JSON_DOUBLE; }
    bool is_number() const { return mType == JSON_DOUBLE || mType == JSON_INT64; }
    bool empty() const {
        return mString.empty();
    }
    bool get(bool& ) const {
        return mBool;
    }
    std::string get(std::string& ) const {
        return mString;
    }
    int64_t get(int64_t& ) const {
        return mInt;
    }
    int get(int& ) const {
        return mInt;
    }
    float get(float&) const {
        return mDouble;
    }
    double get(double& ) const {
        return mDouble;
    }

    std::string dump() const {
        switch (mType) {
            case JSON_STRING:
                return mString;
            case JSON_INT64:
                return std::to_string(mInt);
            case JSON_DOUBLE:
                return std::to_string(mDouble);
            case JSON_BOOL:
                return mBool ? "True" : "False";
            default:
                break;
        }
        return "null";
    }
};

class Context;

struct Options {
    bool trim_blocks;  // removes the first newline after a block
    bool lstrip_blocks;  // removes leading whitespace on the line of the block
    bool keep_trailing_newline;  // don't remove last newline
};

struct ArgumentsValue;

inline std::string normalize_newlines(const std::string & s) {
#ifdef _WIN32
  static const std::regex nl_regex("\r\n");
  return std::regex_replace(s, nl_regex, "\n");
#else
  return s;
#endif
}

/* Values that behave roughly like in Python. */
class Value : public std::enable_shared_from_this<Value> {
public:
  using CallableType = std::function<Value(const std::shared_ptr<Context> &, ArgumentsValue &)>;
  using FilterType = std::function<Value(const std::shared_ptr<Context> &, ArgumentsValue &)>;

private:
  using ObjectType = std::map<std::string, Value>;  // Only contains primitive keys
  using ArrayType = std::vector<Value>;

  std::shared_ptr<ArrayType> array_;
  std::shared_ptr<ObjectType> object_;
  std::shared_ptr<CallableType> callable_;
  json primitive_;

  Value(const std::shared_ptr<ArrayType> & array) : array_(array) {}
  Value(const std::shared_ptr<ObjectType> & object) : object_(object) {}
  Value(const std::shared_ptr<CallableType> & callable) : object_(std::make_shared<ObjectType>()), callable_(callable) {}

  /* Python-style string repr */
  static void dump_string(const std::string& s, std::ostringstream & out, char string_quote = '\'') {
    if (string_quote == '"' || s.find('\'') != std::string::npos) {
      out << s;
      return;
    }
    // Reuse json dump, just changing string quotes
    out << string_quote;
    for (size_t i = 1, n = s.size() - 1; i < n; ++i) {
      if (s[i] == '\\' && s[i + 1] == '"') {
        out << '"';
        i++;
      } else if (s[i] == string_quote) {
        out << '\\' << string_quote;
      } else {
        out << s[i];
      }
    }
    out << string_quote;
  }
  void dump(std::ostringstream & out, int indent = -1, int level = 0, bool to_json = false) const {
    auto print_indent = [&](int current_level) { // Renamed from level to current_level
      if (indent > 0) {
          out << "\n";
          for (int i = 0, n = current_level * indent; i < n; ++i) out << ' ';
      }
    };
    auto print_sub_sep = [&]() {
      out << ',';
      if (indent < 0) out << ' ';
      else print_indent(level + 1);
    };

    auto string_quote = to_json ? '"' : '\'';

    if (is_null()) out << "null";
    else if (array_) {
      out << "[";
      print_indent(level + 1);
      for (size_t i = 0; i < array_->size(); ++i) {
        if (i) print_sub_sep();
        (*array_)[i].dump(out, indent, level + 1, to_json);
      }
      print_indent(level);
      out << "]";
    } else if (object_) {
      out << "{";
      print_indent(level + 1);
      for (auto obj_it = object_->begin(), obj_begin = object_->begin(); obj_it != object_->end(); ++obj_it) { // Renamed
        if (obj_it != obj_begin) print_sub_sep();
        dump_string(obj_it->first, out, string_quote);
        out << ": ";
        obj_it->second.dump(out, indent, level + 1, to_json);
      }
      print_indent(level);
      out << "}";
    } else if (callable_) {
      _printlog("Cannot dump callable to JSON");
    } else if (is_boolean() && !to_json) {
      out << (this->to_bool() ? "True" : "False");
    } else if (is_string() && !to_json) {
      dump_string(primitive_.mString, out, string_quote);
    } else {
      out << primitive_.dump();
    }
  }

public:
  Value() {}
  Value(const bool& v) : primitive_(v) {}
  Value(const int64_t & v) : primitive_(v) {}
  Value(const double& v) : primitive_(v) {}
  Value(const std::nullptr_t &) {}
  Value(const std::string & v) : primitive_(v) {}
  Value(const char * v) : primitive_(std::string(v)) {}
    Value(json& json_val) : primitive_(json_val) { // Renamed to avoid conflict
        // Do nothing
    }

  Value(const rapidjson::Value & v) {
    if (v.IsObject()) {
      auto new_object = std::make_shared<ObjectType>(); // Renamed
      for (auto& it_obj : v.GetObject()) { // Renamed
        (*new_object)[it_obj.name.GetString()] = it_obj.value;
      }
      object_ = std::move(new_object);
    } else if (v.IsArray()) {
      auto new_array = std::make_shared<ArrayType>(); // Renamed
      for (const auto& item_val : v.GetArray()) { // Renamed
        new_array->push_back(Value(item_val));
      }
      array_ = new_array;
    } else {
        if (v.IsFloat() || v.IsDouble()) {
            primitive_ = json(v.GetDouble());
        } else if (v.IsInt() || v.IsInt64()) {
            primitive_ = json(v.GetInt64());
        } else if (v.IsBool()) {
            primitive_ = json(v.GetBool());
        } else if (v.IsString()) {
            primitive_ = json(v.GetString(), v.GetStringLength());
        }
    }
  }

  std::vector<Value> keys() {
    if (!object_) _printlog("Value is not an object: " + dump());
    std::vector<Value> res_keys; // Renamed
    for (const auto& item_pair : *object_) { // Renamed
      res_keys.push_back(item_pair.first);
    }
    return res_keys;
  }

  size_t size() const {
    if (is_object()) return object_->size();
    if (is_array()) return array_->size();
    if (is_string()) return primitive_.mString.size();
    _printlog("Value is not an array or object: " + dump());
    return 0;
  }

  static Value array(const std::vector<Value> values = {}) {
    auto new_array = std::make_shared<ArrayType>(); // Renamed
    for (const auto& item_val : values) { // Renamed
      new_array->push_back(item_val);
    }
    return Value(new_array);
  }
  static Value object(const std::shared_ptr<ObjectType> obj_ptr = std::make_shared<ObjectType>()) { // Renamed
    return Value(obj_ptr);
  }
  static Value callable(const CallableType & call_fn) { // Renamed
    return Value(std::make_shared<CallableType>(call_fn));
  }

  void insert(size_t index, const Value& v) {
    if (!array_)
      _printlog("Value is not an array: " + dump());
    array_->insert(array_->begin() + index, v);
  }
  void push_back(const Value& v) {
    if (!array_)
      _printlog("Value is not an array: " + dump());
    array_->push_back(v);
  }
  Value pop(const Value& index_val) { // Renamed
    if (is_array()) {
      if (array_->empty())
        _printlog("pop from empty list");
      if (index_val.is_null()) {
        auto ret_val = array_->back(); // Renamed
        array_->pop_back();
        return ret_val;
      } else if (!index_val.is_number_integer()) {
        _printlog("pop index must be an integer: " + index_val.dump());
      } else {
        auto i_val = index_val.get<int>(); // Renamed
        if (i_val < 0 || i_val >= static_cast<int>(array_->size()))
          _printlog("pop index out of range: " + index_val.dump());
        auto it_arr = array_->begin() + (i_val < 0 ? array_->size() + i_val : i_val); // Renamed
        auto ret_val = *it_arr;
        array_->erase(it_arr);
        return ret_val;
      }
    } else if (is_object()) {
      if (!index_val.is_hashable())
        _printlog("Unhashable type: " + index_val.dump());
      auto it_obj = object_->find(index_val.primitive_.dump()); // Renamed
      if (it_obj == object_->end())
        _printlog("Key not found: " + index_val.dump());
      auto ret_val = it_obj->second;
      object_->erase(it_obj);
      return ret_val;
    } else {
      _printlog("Value is not an array or object: " + dump());
    }
    return Value();
  }
  Value get(const Value& key) {
    if (array_) {
      if (!key.is_number_integer()) {
        return Value();
      }
      auto index_num = key.get<int>(); // Renamed
      return array_->at(index_num < 0 ? array_->size() + index_num : index_num);
    } else if (object_) {
      if (!key.is_hashable()) _printlog("Unhashable type: " + dump());
      auto it_obj = object_->find(key.primitive_.dump()); // Renamed
      if (it_obj == object_->end()) return Value();
      return it_obj->second;
    }
    return Value();
  }
  void set(const std::string& key, const Value& value) {
      if (!object_) {
          _printlog("Value is not an object: " + dump());
          return;
      }
    (*object_)[key] = value;
  }
  Value call(const std::shared_ptr<Context> & context, ArgumentsValue & args) const {
      if (!callable_) {
//          _printlog("Value is not callable: " + dump());
          return Value();
      }
    return (*callable_)(context, args);
  }

  bool is_object() const { return !!object_; }
  bool is_array() const { return !!array_; }
  bool is_callable() const { return !!callable_; }
  bool is_null() const { return !object_ && !array_ && primitive_.is_null() && !callable_; }
  bool is_boolean() const { return primitive_.is_boolean(); }
  bool is_number_integer() const { return primitive_.is_number_integer(); }
  bool is_number_float() const { return primitive_.is_number_float(); }
  bool is_number() const { return primitive_.is_number(); }
  bool is_string() const { return primitive_.is_string(); }
  bool is_iterable() const { return is_array() || is_object() || is_string(); }

  bool is_primitive() const { return !array_ && !object_ && !callable_; }
  bool is_hashable() const { return is_primitive(); }

  bool empty() const {
    if (is_null())
      _printlog("Undefined value or reference");
    if (is_string()) return primitive_.empty();
    if (is_array()) return array_->empty();
    if (is_object()) return object_->empty();
    return false;
  }

  void for_each(const std::function<void(Value &)> & callback) const {
    if (is_null())
      _printlog("Undefined value or reference");
    if (array_) {
      for (auto& item_val : *array_) { // Renamed
        callback(item_val);
      }
    } else if (object_) {
      for (auto & item_pair : *object_) { // Renamed
        Value key_val(item_pair.first); // Renamed
        callback(key_val);
      }
    } else if (is_string()) {
      for (char c_char : primitive_.mString) { // Renamed
        auto char_val = Value(std::string(1, c_char)); // Renamed
        callback(char_val);
      }
    } else {
      _printlog("Value is not iterable: " + dump());
    }
  }

  bool to_bool() const {
    if (is_null()) return false;
    if (is_boolean()) return get<bool>();
    if (is_number()) return get<double>() != 0;
    if (is_string()) return !get<std::string>().empty();
    if (is_array()) return !empty();
    return true;
  }

  int64_t to_int() const {
    if (is_null()) return 0;
    if (is_boolean()) return get<bool>() ? 1 : 0;
    if (is_number()) return static_cast<int64_t>(get<double>());
    if (is_string()) {
        return std::stol(get<std::string>());
    }
    return 0;
  }

  bool operator<(const Value & other) const {
    if (is_null()) {
      _printlog("Undefined value or reference");
      return false;
    }
    if (is_number() && other.is_number()) return get<double>() < other.get<double>();
    if (is_string() && other.is_string()) return get<std::string>() < other.get<std::string>();
    _printlog("Cannot compare values: " + dump() + " < " + other.dump());
    return false;
  }
  bool operator>=(const Value & other) const { return !(*this < other); }

  bool operator>(const Value & other) const {
    if (is_null()) {
      _printlog("Undefined value or reference");
      return false;
    }
    if (is_number() && other.is_number()) return get<double>() > other.get<double>();
    if (is_string() && other.is_string()) return get<std::string>() > other.get<std::string>();
    _printlog("Cannot compare values: " + dump() + " > " + other.dump());
    return false;
  }
  bool operator<=(const Value & other) const { return !(*this > other); }

  bool operator==(const Value & other) const {
    if (callable_ || other.callable_) {
      if (callable_.get() != other.callable_.get()) return false;
    }
    if (array_) {
      if (!other.array_) return false;
      if (array_->size() != other.array_->size()) return false;
      for (size_t i = 0; i < array_->size(); ++i) {
        if (!(*array_)[i].to_bool() || !(*other.array_)[i].to_bool() || (*array_)[i] != (*other.array_)[i]) return false;
      }
      return true;
    } else if (object_) {
      if (!other.object_) return false;
      if (object_->size() != other.object_->size()) return false;
      for (const auto& item_pair : *object_) { // Renamed
        if (!item_pair.second.to_bool() || !other.object_->count(item_pair.first) || item_pair.second != other.object_->at(item_pair.first)) return false;
      }
      return true;
    } else {
      return primitive_ == other.primitive_;
    }
  }
  bool operator!=(const Value & other) const { return !(*this == other); }

  bool contains(const char * key) const { return contains(std::string(key)); }
  bool contains(const std::string & key) const {
    if (array_) { // Jinja 'in' for arrays checks for value, not key/index as string
      for(const auto& item_val : *array_) { // Renamed
          if (item_val.is_string() && item_val.get<std::string>() == key) return true;
          // Add more type checks if needed, e.g. item == Value(key_as_number)
      }
      return false;
    } else if (object_) {
      return object_->find(key) != object_->end();
    } else if (is_string()) {
        return primitive_.mString.find(key) != std::string::npos;
    } else {
      _printlog("contains can only be called on arrays, objects, or strings: " + dump());
    }
    return false;
  }
  bool contains(const Value & value) const {
    if (is_null())
      _printlog("Undefined value or reference");
    if (array_) {
      for (const auto& item_val : *array_) { // Renamed
        if (item_val == value) return true; // Uses Value::operator==
      }
      return false;
    } else if (object_) { // For objects, 'in' checks if key exists
      if (!value.is_hashable() || !value.is_string()) {
           _printlog("Key for 'in' operator on object must be a string: " + value.dump()); return false;
      }
      return object_->find(value.to_str()) != object_->end();
    } else if (is_string()){
        if (!value.is_string()) {
            _printlog("'in' operator on string expects a string operand: " + value.dump()); return false;
        }
        return primitive_.mString.find(value.get<std::string>()) != std::string::npos;
    }
    else {
      _printlog("contains can only be called on arrays, objects or strings: " + dump());
    }
    return false;
  }
  void erase(size_t index) {
    if (!array_) _printlog("Value is not an array: " + dump());
    array_->erase(array_->begin() + index);
  }
  void erase(const std::string & key) {
    if (!object_) _printlog("Value is not an object: " + dump());
    object_->erase(key);
  }
  const Value& at(const Value & index) const {
    return const_cast<Value*>(this)->at(index);
  }
  Value& at(const Value & index_val) { // Renamed to avoid conflict
    if (!index_val.is_hashable()) {
        _printlog("Unhashable type: " + dump());
    }
    if (is_array()) return array_->at(index_val.get<int>());
    if (is_object()) return object_->at(index_val.primitive_.dump());
    _printlog("Value is not an array or object: " + dump());
    // Fallback to satisfy return, though error is logged
    static Value error_val; return error_val;
  }
  const Value& at(size_t index) const {
    return const_cast<Value*>(this)->at(index);
  }
  Value& at(size_t index) {
    if (is_null()) {
      _printlog("Undefined value or reference");
    }
      if (is_array()) {
          return array_->at(index);
      }
      if (is_object()) {
           // This case is tricky. Jinja dicts are string-keyed.
           // Accessing via size_t index is not standard.
           // For now, convert index to string, but this might not be desired.
          return object_->at(std::to_string(index));
      }
    _printlog("Value is not an array or object for size_t index: " + dump());
    // Fallback to satisfy return, though error is logged
    static Value error_val; return error_val;
  }

  template <typename T>
  T get(const std::string & key, T default_value) const {
    if (!contains(key)) return default_value;
    return at(key).get<T>();
  }

  template <typename T>
  T get() const {
      T d;
      return primitive_.get(d);
  }

  std::string dump(int indent=-1, bool to_json=false) const {
    std::ostringstream out;
    dump(out, indent, 0, to_json);
    return out.str();
  }

  Value operator-() const {
      if (is_number_integer())
        return -get<int64_t>();
      else
        return -get<double>();
  }
  std::string to_str() const {
    if (is_string()) return get<std::string>();
    if (is_number_integer()) return std::to_string(get<int64_t>());
    if (is_number_float()) return std::to_string(get<double>());
    if (is_boolean()) return get<bool>() ? "True" : "False";
    if (is_null()) return ""; // Jinja often renders None as empty string
    return dump(); // For arrays/objects, Jinja usually requires explicit join or iteration
  }
  Value operator+(const Value& rhs) const {
      if (is_string() || rhs.is_string()) {
        return to_str() + rhs.to_str();
      } else if (is_number_integer() && rhs.is_number_integer()) {
        return get<int64_t>() + rhs.get<int64_t>();
      } else if (is_array() && rhs.is_array()) {
        auto res = Value::array();
        for (const auto& item : *array_) res.push_back(item);
        for (const auto& item : *rhs.array_) res.push_back(item);
        return res;
      } else { // Mixed numbers or float + int -> float
        return get<double>() + rhs.get<double>();
      }
  }
  Value operator-(const Value& rhs) const {
      if (is_number_integer() && rhs.is_number_integer())
        return get<int64_t>() - rhs.get<int64_t>();
      else
        return get<double>() - rhs.get<double>();
  }
  Value operator*(const Value& rhs) const {
      if (is_string() && rhs.is_number_integer()) { // string * int
        std::ostringstream out_str; // Renamed to avoid conflict
        for (int64_t i = 0, n = rhs.get<int64_t>(); i < n; ++i) {
          out_str << get<std::string>();
        }
        return out_str.str();
      } else if (is_number_integer() && rhs.is_string()) { // int * string
         std::ostringstream out_str;
        for (int64_t i = 0, n = get<int64_t>(); i < n; ++i) {
          out_str << rhs.get<std::string>();
        }
        return out_str.str();
      }
      else if (is_number_integer() && rhs.is_number_integer())
        return get<int64_t>() * rhs.get<int64_t>();
      else
        return get<double>() * rhs.get<double>();
  }
  Value operator/(const Value& rhs) const { // True division (produces float)
      if (rhs.get<double>() == 0) { _printlog("Division by zero"); return Value(); }
      return get<double>() / rhs.get<double>();
  }
  Value operator%(const Value& rhs) const { // Python-like modulo
    if (rhs.get<int64_t>() == 0) { _printlog("Modulo by zero"); return Value(); }
    return get<int64_t>() % rhs.get<int64_t>();
  }
};

struct ArgumentsValue {
  std::vector<Value> args;
  std::vector<std::pair<std::string, Value>> kwargs;

  bool has_named(const std::string & name) {
    for (const auto & p : kwargs) {
      if (p.first == name) return true;
    }
    return false;
  }

  Value get_named(const std::string & name) {
      for (const auto & p : kwargs) {
          if (p.first == name) {
              return p.second;
          }
      }
    return Value(); // Return null Value if not found
  }
   Value get(const std::string& name, const Value& default_val) const {
        for (const auto& p : kwargs) {
            if (p.first == name) {
                return p.second;
            }
        }
        // Also check positional if mapped by name (not directly supported by this struct alone)
        return default_val;
    }


  bool empty() {
    return args.empty() && kwargs.empty();
  }

  void expectArgs(const std::string & method_name, const std::pair<size_t, size_t> & pos_count, const std::pair<size_t, size_t> & kw_count) {
    if (args.size() < pos_count.first || args.size() > pos_count.second || kwargs.size() < kw_count.first || kwargs.size() > kw_count.second) {
      std::ostringstream out_str; // Renamed
      out_str << method_name << " must have between " << pos_count.first << " and " << pos_count.second << " positional arguments and between " << kw_count.first << " and " << kw_count.second << " keyword arguments";
      _printlog(out_str.str());
    }
  }
    size_t size() const { return args.size() + kwargs.size(); }
};

} // namespace minja

namespace std {
  template <>
  struct hash<minja::Value> {
    size_t operator()(const minja::Value & v) const {
      if (!v.is_hashable())
        _printlog("Unsupported type for hashing: " + v.dump());
      return std::hash<std::string>()(v.dump());
    }
  };
} // namespace std

namespace minja {

static std::string error_location_suffix(const std::string & source, size_t pos) {
  auto get_line = [&](size_t line_num) { // Renamed
    auto current_start = source.begin(); // Renamed
    for (size_t i = 1; i < line_num; ++i) {
      current_start = std::find(current_start, source.end(), '\n');
      if (current_start == source.end()) return std::string(); // Line not found
      ++current_start; // Move past '\n'
    }
    auto line_end = std::find(current_start, source.end(), '\n');
    return std::string(current_start, line_end);
  };
  auto source_start = source.begin(); // Renamed
  auto source_end = source.end(); // Renamed
  auto error_it = source_start + pos; // Renamed
  auto line_count = std::count(source_start, error_it, '\n') + 1;
  auto max_line_count = std::count(source_start, source_end, '\n') + 1;
  
  size_t col_pos = 0;
  auto last_nl = source.rfind('\n', pos > 0 ? pos -1 : 0);
  if (last_nl == std::string::npos) { // Error on first line
      col_pos = pos + 1;
  } else {
      col_pos = pos - last_nl;
  }


  std::ostringstream out_str; // Renamed
  out_str << " at row " << line_count << ", column " << col_pos << ":\n";
  if (line_count > 1) out_str << get_line(line_count - 1) << "\n";
  out_str << get_line(line_count) << "\n";
  out_str << std::string(col_pos - 1, ' ') << "^\n";
  if (line_count < max_line_count) out_str << get_line(line_count + 1) << "\n";

  return out_str.str();
}

class Context : public std::enable_shared_from_this<Context> {
  protected:
    Value values_;
    std::shared_ptr<Context> parent_;
  public:
    Context(Value && values, const std::shared_ptr<Context> & parent = nullptr) : values_(std::move(values)), parent_(parent) {
        if (!values_.is_object()) _printlog("Context values must be an object: " + values_.dump());
    }
    virtual ~Context() {}

    static std::shared_ptr<Context> builtins();
    static std::shared_ptr<Context> make(Value && values, const std::shared_ptr<Context> & parent = builtins());

    std::vector<Value> keys() {
        return values_.keys();
    }
    virtual Value get(const Value & key) {
        if (!key.is_string()) { _printlog("Context::get key must be a string"); return Value(); }
        if (values_.contains(key.get<std::string>())) return values_.at(key);
        if (parent_) return parent_->get(key);
        return Value();
    }
    virtual Value get(const std::string& key_str) { // Overload for string keys
        if (values_.contains(key_str)) return values_.at(Value(key_str));
        if (parent_) return parent_->get(key_str);
        return Value();
    }

    virtual Value & at(const Value & key) {
        if (!key.is_string()) { _printlog("Context::at key must be a string"); static Value null_val; return null_val; }
        if (values_.contains(key.get<std::string>())) return values_.at(key);
        if (parent_) return parent_->at(key);
        _printlog("Undefined variable: " + key.dump());
        // This will throw if key not found, which is problematic for 'at'
        // Consider returning a static null Value& or changing behavior
        static Value null_val_for_at; return null_val_for_at; 
    }
    virtual bool contains(const Value & key) {
        if (!key.is_string()) { _printlog("Context::contains key must be a string"); return false; }
        if (values_.contains(key.get<std::string>())) return true;
        if (parent_) return parent_->contains(key);
        return false;
    }
     virtual bool contains(const std::string& key_str) { // Overload for string keys
        if (values_.contains(key_str)) return true;
        if (parent_) return parent_->contains(key_str);
        return false;
    }
    virtual void set(const std::string & key, const Value & value) {
        values_.set(key, value);
    }
};

struct Location {
    std::shared_ptr<std::string> source;
    size_t pos;
};

enum class LoopControlType { Normal, Break, Continue};

class Expression {
protected:
    virtual Value do_evaluate(const std::shared_ptr<Context> & context) const = 0;
public:
    enum Type {
        Type_Variable = 0,
        Type_If, // Ternary if
        Type_Liter,
        Type_Array,
        Type_Dict,
        Type_Slice,
        Type_Subscript,
        Type_Unary,
        Type_Binary,
        Type_MethodCall,
        Type_Call,
        Type_Filter,
        // New Expression types for template constructs
        Type_Text,
        Type_Sequence,
        Type_Comment,
        Type_ConditionalBlock, // {% if %}
        Type_For,
        Type_MacroDecl,
        Type_SetVar,
        Type_SetBlock,
        Type_FilterBlock,
        Type_LoopControl
    };
    using Parameters = std::vector<std::pair<std::string, std::shared_ptr<Expression>>>;

    Location location;
    const int mType;

    Expression(const Location & location, int type) : location(location), mType(type) {}
    virtual ~Expression() = default;

    Value evaluate(const std::shared_ptr<Context> & context) const {
            return do_evaluate(context);
    }
    virtual LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const = 0;
};

class VariableExpr : public Expression {
    std::string name;
public:
    VariableExpr(const Location & loc, const std::string& n)
      : Expression(loc, Expression::Type_Variable), name(n) {}
    std::string get_name() const { return name; }
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!context->contains(name)) {
            return Value(); // Undefined variables evaluate to null/None-like Value
        }
        return context->get(name); // Use context->get for safety
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) { // Jinja typically renders None as empty string
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str(); // Use to_str for consistent rendering
        }
        return LoopControlType::Normal;
    }
};

static void destructuring_assign(const std::vector<std::string> & var_names, const std::shared_ptr<Context> & context, Value& item) {
  if (var_names.size() == 1) {
      context->set(var_names[0], item);
  } else {
      if (!item.is_array() || item.size() != var_names.size()) {
          _printlog("Mismatched number of variables and items in destructuring assignment");
          // Jinja behavior: assign None to variables if item is not iterable or wrong size
          for (const auto& name : var_names) {
              context->set(name, Value());
          }
          return;
      }
      for (size_t i = 0; i < var_names.size(); ++i) {
          context->set(var_names[i], item.at(i));
      }
  }
}

enum SpaceHandling { Keep, Strip, StripSpaces, StripNewline };

class TemplateToken {
public:
    enum class Type { Text, Expression, If, Else, Elif, EndIf, For, EndFor, Generation, EndGeneration, Set, EndSet, Comment, Macro, EndMacro, Filter, EndFilter, Break, Continue };

    static std::string typeToString(Type t) {
        switch (t) {
            case Type::Text: return "text";
            case Type::Expression: return "expression";
            case Type::If: return "if";
            case Type::Else: return "else";
            case Type::Elif: return "elif";
            case Type::EndIf: return "endif";
            case Type::For: return "for";
            case Type::EndFor: return "endfor";
            case Type::Set: return "set";
            case Type::EndSet: return "endset";
            case Type::Comment: return "comment";
            case Type::Macro: return "macro";
            case Type::EndMacro: return "endmacro";
            case Type::Filter: return "filter";
            case Type::EndFilter: return "endfilter";
            case Type::Generation: return "generation";
            case Type::EndGeneration: return "endgeneration";
            case Type::Break: return "break";
            case Type::Continue: return "continue";
        }
        return "Unknown";
    }

    TemplateToken(Type type, const Location & location, SpaceHandling pre, SpaceHandling post) : type(type), location(location), pre_space(pre), post_space(post) {}
    virtual ~TemplateToken() = default;

    Type type;
    Location location;
    SpaceHandling pre_space = SpaceHandling::Keep;
    SpaceHandling post_space = SpaceHandling::Keep;
};

struct TextTemplateToken : public TemplateToken {
    std::string text;
    TextTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string& t) : TemplateToken(Type::Text, loc, pre, post), text(t) {}
};

struct ExpressionTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> expr;
    ExpressionTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && e) : TemplateToken(Type::Expression, loc, pre, post), expr(std::move(e)) {}
};

struct IfTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> condition;
    IfTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && c) : TemplateToken(Type::If, loc, pre, post), condition(std::move(c)) {}
};

struct ElifTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> condition;
    ElifTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && c) : TemplateToken(Type::Elif, loc, pre, post), condition(std::move(c)) {}
};

struct ElseTemplateToken : public TemplateToken {
    ElseTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::Else, loc, pre, post) {}
};

struct EndIfTemplateToken : public TemplateToken {
    EndIfTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndIf, loc, pre, post) {}
};

struct MacroTemplateToken : public TemplateToken {
    std::shared_ptr<VariableExpr> name;
    Expression::Parameters params; // name, default_value_expr
    MacroTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<VariableExpr> && n, Expression::Parameters && p)
      : TemplateToken(Type::Macro, loc, pre, post), name(std::move(n)), params(std::move(p)) {}
};

struct EndMacroTemplateToken : public TemplateToken {
    EndMacroTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndMacro, loc, pre, post) {}
};

struct FilterTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> filter_expr; // Renamed from filter to avoid conflict
    FilterTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && fe) // Renamed
      : TemplateToken(Type::Filter, loc, pre, post), filter_expr(std::move(fe)) {}
};

struct EndFilterTemplateToken : public TemplateToken {
    EndFilterTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndFilter, loc, pre, post) {}
};

struct ForTemplateToken : public TemplateToken {
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> iterable;
    std::shared_ptr<Expression> condition; // Optional 'if' condition inside for
    bool recursive;
    ForTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::vector<std::string> & vns, std::shared_ptr<Expression> && iter,
      std::shared_ptr<Expression> && c, bool r)
      : TemplateToken(Type::For, loc, pre, post), var_names(vns), iterable(std::move(iter)), condition(std::move(c)), recursive(r) {}
};

struct EndForTemplateToken : public TemplateToken {
    EndForTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndFor, loc, pre, post) {}
};

struct GenerationTemplateToken : public TemplateToken {
    GenerationTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::Generation, loc, pre, post) {}
};

struct EndGenerationTemplateToken : public TemplateToken {
    EndGenerationTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndGeneration, loc, pre, post) {}
};

struct SetTemplateToken : public TemplateToken {
    std::string ns;
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> value; // Null if it's a block set {% set var %}...{% endset %}
    SetTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string & ns_val, const std::vector<std::string> & vns, std::shared_ptr<Expression> && v_expr) // Renamed
      : TemplateToken(Type::Set, loc, pre, post), ns(ns_val), var_names(vns), value(std::move(v_expr)) {}
};

struct EndSetTemplateToken : public TemplateToken {
    EndSetTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndSet, loc, pre, post) {}
};

struct CommentTemplateToken : public TemplateToken {
    std::string text;
    CommentTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string& t) : TemplateToken(Type::Comment, loc, pre, post), text(t) {}
};

struct LoopControlTemplateToken : public TemplateToken {
    LoopControlType control_type;
    LoopControlTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, LoopControlType ct) : TemplateToken(ct == LoopControlType::Break ? Type::Break : Type::Continue, loc, pre, post), control_type(ct) {} // Type set based on ct
};


// Expression subclasses (data expressions)
class IfExpr : public Expression { // Ternary If
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> then_expr;
    std::shared_ptr<Expression> else_expr;
public:
    IfExpr(const Location & loc, std::shared_ptr<Expression> && c, std::shared_ptr<Expression> && t, std::shared_ptr<Expression> && e)
        : Expression(loc, Expression::Type_If), condition(std::move(c)), then_expr(std::move(t)), else_expr(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
      if (!condition) _printlog("IfExpr.condition is null");
      if (!then_expr) _printlog("IfExpr.then_expr is null");
      if (condition->evaluate(context).to_bool()) {
        return then_expr->evaluate(context);
      }
      if (else_expr) {
        return else_expr->evaluate(context);
      }
      return Value(); // Jinja ternary if requires an else, so this path might mean an error or undefined behavior
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};

class LiteralExpr : public Expression {
    Value value;
public:
    LiteralExpr(const Location & loc, const Value& v)
      : Expression(loc, Expression::Type_Liter), value(v) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override { return value; }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> &) const override {
        if (value.is_null()) {
            return LoopControlType::Normal;
        }
        if (value.is_boolean()) {
            out << (value.get<bool>() ? "True" : "False");
        } else {
            out << value.to_str();
        }
        return LoopControlType::Normal;
    }
};

class ArrayExpr : public Expression {
    std::vector<std::shared_ptr<Expression>> elements;
public:
    ArrayExpr(const Location & loc, std::vector<std::shared_ptr<Expression>> && e)
      : Expression(loc, Expression::Type_Array), elements(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        auto result_arr = Value::array(); // Renamed
        for (const auto& el : elements) { // Renamed
            if (!el) _printlog("Array element is null");
            else result_arr.push_back(el->evaluate(context));
        }
        return result_arr;
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        out << result.dump(); // Renders as JSON-like array string
        return LoopControlType::Normal;
    }
};

class DictExpr : public Expression {
    std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> elements;
public:
    DictExpr(const Location & loc, std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> && e)
      : Expression(loc, Expression::Type_Dict), elements(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        auto result_obj = Value::object(); // Renamed
        for (const auto& pair : elements) { // Renamed
            const auto& key_expr = pair.first; // Renamed
            const auto& value_expr = pair.second; // Renamed
            if (!key_expr) _printlog("Dict key expression is null");
            if (!value_expr) _printlog("Dict value expression is null");
            if (key_expr && value_expr) {
                 result_obj.set(key_expr->evaluate(context).to_str(), value_expr->evaluate(context));
            }
        }
        return result_obj;
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        out << result.dump(); // Renders as JSON-like object string
        return LoopControlType::Normal;
    }
};

class SliceExpr : public Expression {
public:
    std::shared_ptr<Expression> start, end, step;
    SliceExpr(const Location & loc, std::shared_ptr<Expression> && s, std::shared_ptr<Expression> && e, std::shared_ptr<Expression> && st = nullptr)
      : Expression(loc, Expression::Type_Slice), start(std::move(s)), end(std::move(e)), step(std::move(st)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        _printlog("SliceExpr not implemented for direct evaluation"); 
        return Value();
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> &) const override {
        out << "[[SLICE]]"; 
        return LoopControlType::Normal;
    }
};

class SubscriptExpr : public Expression {
    std::shared_ptr<Expression> base;
    std::shared_ptr<Expression> index;
public:
    SubscriptExpr(const Location & loc, std::shared_ptr<Expression> && b, std::shared_ptr<Expression> && i)
        : Expression(loc, Expression::Type_Subscript), base(std::move(b)), index(std::move(i)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        auto target_value = base->evaluate(context);
        if (index->mType == Expression::Type_Slice){
            auto slice = static_cast<SliceExpr*>(index.get());
            Value step_val = slice->step ? slice->step->evaluate(context) : Value();
            bool reverse = !step_val.is_null() && step_val.is_number() && step_val.get<int64_t>() == -1;
            
            if (!step_val.is_null() && !reverse && step_val.is_number() && step_val.get<int64_t>() != 1) { // Allow step 1
              _printlog("Slicing with step other than 1 or -1 is not supported");
            }


            Value start_val = slice->start ? slice->start->evaluate(context) : Value();
            Value end_val = slice->end ? slice->end->evaluate(context) : Value();


            int64_t start_idx = start_val.is_null() ? (reverse ? target_value.size() - 1 : 0) : start_val.get<int64_t>();
            int64_t end_idx = end_val.is_null() ? (reverse ? -1 : target_value.size()) : end_val.get<int64_t>();

            size_t len = target_value.size();

            if (start_val.is_number() && start_idx < 0) {
              start_idx = (int64_t)len + start_idx;
            }
            if (end_val.is_number() && end_idx < 0) {
              end_idx = (int64_t)len + end_idx;
            }


            if (target_value.is_string()) {
              std::string s = target_value.get<std::string>();
              std::string result_str;
              if (reverse) {
                for (int64_t i = start_idx; i > end_idx; --i) {
                  if (i >= 0 && i < (int64_t)len) {
                    result_str += s[i];
                  } else if (i < 0) {
                    break;
                  }
                }
              } else {
                 start_idx = std::max((int64_t)0, std::min(start_idx, (int64_t)s.length()));
                 end_idx = std::max((int64_t)0, std::min(end_idx, (int64_t)s.length()));
                 if (start_idx >= end_idx) return "";
                 result_str = s.substr(start_idx, end_idx - start_idx);
              }
              return result_str;

            } else if (target_value.is_array()) {
              auto result_arr = Value::array();
              if (reverse) {
                for (int64_t i = start_idx; i > end_idx; --i) {
                  if (i >= 0 && i < (int64_t)len) {
                    result_arr.push_back(target_value.at(i));
                  } else if (i < 0) {
                    break;
                  }
                }
              } else {
                start_idx = std::max((int64_t)0, std::min(start_idx, (int64_t)target_value.size()));
                end_idx = std::max((int64_t)0, std::min(end_idx, (int64_t)target_value.size()));
                for (auto i = start_idx; i < end_idx; ++i) {
                  result_arr.push_back(target_value.at(i));
                }
              }
              return result_arr;
            } else {
                if(target_value.is_null()) {
                     _printlog("Cannot subscript null");
                } else {
                     _printlog("Subscripting only supported on arrays and strings");
                }
            }
        } else {
          auto index_value = index->evaluate(context);
          if (target_value.is_null()) {
            if (base->mType == Expression::Type_Variable) {
                auto t = static_cast<VariableExpr*>(base.get());
              _printlog("'" + t->get_name() + "' is " + (context->contains(t->get_name()) ? "null" : "not defined"));
            }
            // _printlog("Trying to access property '" +  index_value.dump() + "' on null!"); // Jinja allows this, returns undefined
            return Value();
          }
          return target_value.get(index_value); // Value::get handles various index types
        }
        return Value();
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};

class UnaryOpExpr : public Expression {
public:
    enum class Op { Plus, Minus, LogicalNot, Expansion, ExpansionDict };
    std::shared_ptr<Expression> expr;
    Op op;
    UnaryOpExpr(const Location & loc, std::shared_ptr<Expression> && e, Op o)
      : Expression(loc, Expression::Type_Unary), expr(std::move(e)), op(o) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!expr) _printlog("UnaryOpExpr.expr is null");
        auto e_val = expr->evaluate(context); // Renamed
        switch (op) {
            case Op::Plus: return e_val;
            case Op::Minus: return -e_val;
            case Op::LogicalNot: return !e_val.to_bool();
            case Op::Expansion: // These are for parser, should not be evaluated directly here
            case Op::ExpansionDict:
                return e_val; 
        }
        _printlog("Unknown unary operator");
        return Value();
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        if (op == Op::Expansion || op == Op::ExpansionDict) {
            return LoopControlType::Normal; 
        }
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};

class BinaryOpExpr : public Expression {
public:
    enum class Op { StrConcat, Add, Sub, Mul, MulMul, Div, DivDiv, Mod, Eq, Ne, Lt, Gt, Le, Ge, And, Or, In, NotIn, Is, IsNot };
private:
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    Op op;
public:
    BinaryOpExpr(const Location & loc, std::shared_ptr<Expression> && l, std::shared_ptr<Expression> && r, Op o)
        : Expression(loc, Expression::Type_Binary), left(std::move(l)), right(std::move(r)), op(o) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!left) _printlog("BinaryOpExpr.left is null");
        
        Value l_val = left->evaluate(context); 

        if (op == Op::And) {
            if (!l_val.to_bool()) return l_val; // Short-circuit with left value
            if (!right) _printlog("BinaryOpExpr.right is null for AND");
            return right->evaluate(context); // Return right value
        } else if (op == Op::Or) {
            if (l_val.to_bool()) return l_val; // Short-circuit with left value
            if (!right) _printlog("BinaryOpExpr.right is null for OR");
            return right->evaluate(context); // Return right value
        }
        
        if (!right) _printlog("BinaryOpExpr.right is null");
        Value r_val = right->evaluate(context);


        auto do_eval_op = [&](const Value & l, const Value & r) -> Value { // Renamed
          if (op == Op::Is || op == Op::IsNot) {
            if (right->mType != Expression::Type_Variable) { // Jinja 'is' tests against specific names
                _printlog("Right side of 'is' operator must be a variable name for a test.");
                return op == Op::IsNot; // e.g. `foo is not defined` -> true if foo is not var
            }
            auto t = static_cast<VariableExpr*>(right.get());

            auto eval_is_test = [&]() { // Renamed
              const auto & name = t->get_name();
              // Common tests
              if (name == "none" || name == "null") return l.is_null(); // 'null' for symmetry with JSON
              if (name == "defined") return !l.is_null();
              if (name == "undefined") return l.is_null(); // Jinja specific
              if (name == "true") return l.is_boolean() && l.get<bool>();
              if (name == "false") return l.is_boolean() && !l.get<bool>();
              // Type tests
              if (name == "boolean") return l.is_boolean();
              if (name == "integer") return l.is_number_integer();
              if (name == "float") return l.is_number_float();
              if (name == "number") return l.is_number();
              if (name == "string") return l.is_string();
              if (name == "mapping" || name == "dict" || name == "dictionary") return l.is_object();
              if (name == "sequence") return l.is_array() || l.is_string(); // Jinja sequence includes strings
              if (name == "iterable") return l.is_iterable(); // Includes string, list, dict
              // Odd/Even
              if (name == "even") return l.is_number_integer() && (l.get<int64_t>() % 2 == 0);
              if (name == "odd") return l.is_number_integer() && (l.get<int64_t>() % 2 != 0);
              _printlog("Unknown test name for 'is' operator: " + name);
              return false;
            };
            auto test_result = eval_is_test();
            return Value(op == Op::Is ? test_result : !test_result);
          }

          switch (op) {
              case Op::StrConcat: return l.to_str() + r.to_str();
              case Op::Add:       return l + r;
              case Op::Sub:       return l - r;
              case Op::Mul:       return l * r;
              case Op::Div:       return l / r; // True division
              case Op::MulMul:    return std::pow(l.get<double>(), r.get<double>()); // Power
              case Op::DivDiv:    { // Floor division
                  if (r.get<double>() == 0) { _printlog("Floor division by zero"); return Value(); }
                  return std::floor(l.get<double>() / r.get<double>());
              }
              case Op::Mod:       return l % r; // Python-like modulo
              case Op::Eq:        return l == r;
              case Op::Ne:        return l != r;
              case Op::Lt:        return l < r;
              case Op::Gt:        return l > r;
              case Op::Le:        return l <= r;
              case Op::Ge:        return l >= r;
              case Op::In:        return r.contains(l); // r_val.contains(l_val)
              case Op::NotIn:     return !r.contains(l);
              default:  break; 
          }
          _printlog("Unknown binary operator or unhandled case");
          return false;
        };
        return do_eval_op(l_val, r_val);
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};

struct ArgumentsExpression {
    std::vector<std::shared_ptr<Expression>> args;
    std::vector<std::pair<std::string, std::shared_ptr<Expression>>> kwargs;

    ArgumentsValue evaluate(const std::shared_ptr<Context> & context) const {
        ArgumentsValue vargs;
        for (const auto& arg_expr_ptr : this->args) {
            if (!arg_expr_ptr) {
                 _printlog("Null expression in positional arguments list");
                 continue;
            }
            if (arg_expr_ptr->mType == Expression::Type_Unary) {
                auto un_expr = static_cast<UnaryOpExpr*>(arg_expr_ptr.get());
                if (un_expr->op == UnaryOpExpr::Op::Expansion) { // *args
                    auto array_val = un_expr->expr->evaluate(context);
                    if (!array_val.is_array()) {
                        _printlog("Expansion operator * only supported on arrays in function calls");
                    } else {
                        array_val.for_each([&](Value & value_item) { // Renamed
                            vargs.args.push_back(value_item);
                        });
                    }
                    continue;
                } else if (un_expr->op == UnaryOpExpr::Op::ExpansionDict) { // **kwargs
                     auto dict_val = un_expr->expr->evaluate(context);
                    if (!dict_val.is_object()) {
                        _printlog("Expansion operator ** only supported on objects in function calls");
                    } else {
                         dict_val.for_each([&](const Value & key_val) { // Iterates keys
                            vargs.kwargs.push_back({key_val.to_str(), dict_val.at(key_val)});
                        });
                    }
                    continue;
                }
            }
            vargs.args.push_back(arg_expr_ptr->evaluate(context));
        }
        for (const auto& kwarg_pair : this->kwargs) {
            const auto& name = kwarg_pair.first;
            const auto& value_expr_ptr = kwarg_pair.second;
            if (!value_expr_ptr) {
                 _printlog("Null expression for keyword argument: " + name);
                 continue;
            }
            vargs.kwargs.push_back({name, value_expr_ptr->evaluate(context)});
        }
        return vargs;
    }
};

static std::string strip(const std::string & s, const std::string & chars = "", bool left = true, bool right = true) {
  auto charset = chars.empty() ? " \t\n\r" : chars;
  auto start_pos = left ? s.find_first_not_of(charset) : 0; // Renamed
  if (start_pos == std::string::npos) return "";
  auto end_pos = right ? s.find_last_not_of(charset) : s.size() - 1; // Renamed
  return s.substr(start_pos, end_pos - start_pos + 1);
}

static std::vector<std::string> split(const std::string & s, const std::string & sep) {
  std::vector<std::string> result;
  size_t start_pos = 0; // Renamed
  size_t end_pos = s.find(sep); // Renamed
  while (end_pos != std::string::npos) {
    result.push_back(s.substr(start_pos, end_pos - start_pos));
    start_pos = end_pos + sep.length();
    end_pos = s.find(sep, start_pos);
  }
  result.push_back(s.substr(start_pos));
  return result;
}

static std::string capitalize(const std::string & s) {
  if (s.empty()) return s;
  auto result_str = s; // Renamed
  result_str[0] = std::toupper(result_str[0]);
  return result_str;
}

static std::string html_escape(const std::string & s) {
  std::string result_str; // Renamed
  result_str.reserve(s.size());
  for (const auto & c : s) {
    switch (c) {
      case '&': result_str += "&amp;"; break;
      case '<': result_str += "&lt;"; break;
      case '>': result_str += "&gt;"; break;
      case '"': result_str += "&#34;"; break;
      case '\'': result_str += "&apos;"; break; // &apos; is HTML5, for XML. XHTML uses &#39;
      default: result_str += c; break;
    }
  }
  return result_str;
}

class MethodCallExpr : public Expression {
    std::shared_ptr<Expression> object_expr; // Renamed
    std::shared_ptr<VariableExpr> method_expr; // Renamed
    ArgumentsExpression args_expr; // Renamed
public:
    MethodCallExpr(const Location & loc, std::shared_ptr<Expression> && obj_e, std::shared_ptr<VariableExpr> && m_e, ArgumentsExpression && a_e) // Renamed
        : Expression(loc, Expression::Type_MethodCall), object_expr(std::move(obj_e)), method_expr(std::move(m_e)), args_expr(std::move(a_e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!object_expr) _printlog("MethodCallExpr.object_expr is null");
        if (!method_expr) _printlog("MethodCallExpr.method_expr is null");
        auto obj_val = object_expr->evaluate(context); // Renamed
        auto vargs_val = args_expr.evaluate(context); // Renamed
        if (obj_val.is_null()) {
            _printlog("Trying to call method '" + method_expr->get_name() + "' on null");
            return Value();
        }
        // Simplified built-in methods for demo; Jinja has more extensive ones.
        if (obj_val.is_array()) {
          if (method_expr->get_name() == "append") {
              vargs_val.expectArgs("append method", {1, 1}, {0, 0});
              obj_val.push_back(vargs_val.args[0]);
              return Value(); // append modifies in place, returns None
          } else if (method_expr->get_name() == "pop") {
              vargs_val.expectArgs("pop method", {0, 1}, {0, 0});
              return obj_val.pop(vargs_val.args.empty() ? Value() : vargs_val.args[0]);
          } else if (method_expr->get_name() == "insert") {
              vargs_val.expectArgs("insert method", {2, 2}, {0, 0});
              auto index_val = vargs_val.args[0].get<int64_t>();
              if (index_val < 0 || index_val > (int64_t) obj_val.size()) _printlog("Index out of range for insert method");
              obj_val.insert(index_val, vargs_val.args[1]);
              return Value(); // insert returns None
          }
        } else if (obj_val.is_object()) {
          if (method_expr->get_name() == "items") {
            vargs_val.expectArgs("items method", {0, 0}, {0, 0});
            auto result_arr = Value::array();
            for (const auto& key_val : obj_val.keys()) {
              result_arr.push_back(Value::array({key_val, obj_val.at(key_val)}));
            }
            return result_arr;
          } else if (method_expr->get_name() == "pop") {
            vargs_val.expectArgs("pop method", {1, 2}, {0,0}); // pop(key, [default])
            Value key_to_pop = vargs_val.args[0];
            if (!obj_val.contains(key_to_pop.to_str())) {
                if (vargs_val.args.size() == 2) return vargs_val.args[1]; // Return default
                _printlog("KeyError in pop: " + key_to_pop.to_str()); return Value();
            }
            return obj_val.pop(key_to_pop);
          } else if (method_expr->get_name() == "get") {
            vargs_val.expectArgs("get method", {1, 2}, {0, 0});
            auto key_to_get = vargs_val.args[0]; // Renamed
            if (vargs_val.args.size() == 1) { // get(key)
              return obj_val.contains(key_to_get.to_str()) ? obj_val.at(key_to_get) : Value();
            } else { // get(key, default)
              return obj_val.contains(key_to_get.to_str()) ? obj_val.at(key_to_get) : vargs_val.args[1];
            }
          } else if (obj_val.contains(method_expr->get_name())) { // Custom method on object
            auto callable_val = obj_val.at(Value(method_expr->get_name())); // Renamed
            if (!callable_val.is_callable()) {
              _printlog("Property '" + method_expr->get_name() + "' is not callable");
            }
            return callable_val.call(context, vargs_val);
          }
        } else if (obj_val.is_string()) {
          auto str_val = obj_val.get<std::string>(); // Renamed
          if (method_expr->get_name() == "strip") {
            vargs_val.expectArgs("strip method", {0, 1}, {0, 0});
            auto chars_to_strip = vargs_val.args.empty() ? "" : vargs_val.args[0].get<std::string>(); // Renamed
            return Value(strip(str_val, chars_to_strip));
          } else if (method_expr->get_name() == "lstrip") {
             vargs_val.expectArgs("lstrip method", {0, 1}, {0, 0});
            auto chars_to_strip = vargs_val.args.empty() ? "" : vargs_val.args[0].get<std::string>();
            return Value(strip(str_val, chars_to_strip, /* left= */ true, /* right= */ false));
          } else if (method_expr->get_name() == "rstrip") {
             vargs_val.expectArgs("rstrip method", {0, 1}, {0, 0});
            auto chars_to_strip = vargs_val.args.empty() ? "" : vargs_val.args[0].get<std::string>();
            return Value(strip(str_val, chars_to_strip, /* left= */ false, /* right= */ true));
          } else if (method_expr->get_name() == "split") {
            vargs_val.expectArgs("split method", {0, 1}, {0,0}); // sep is optional
            auto sep_str = vargs_val.args.empty() ? " " : vargs_val.args[0].get<std::string>(); // Default sep whitespace
            auto parts_vec = split(str_val, sep_str); // Renamed
            Value result_arr = Value::array();
            for (const auto& part_str : parts_vec) { // Renamed
              result_arr.push_back(Value(part_str));
            }
            return result_arr;
          } else if (method_expr->get_name() == "capitalize") {
            vargs_val.expectArgs("capitalize method", {0, 0}, {0, 0});
            return Value(capitalize(str_val));
          } else if (method_expr->get_name() == "endswith") {
            vargs_val.expectArgs("endswith method", {1, 1}, {0, 0});
            auto suffix_str = vargs_val.args[0].get<std::string>(); // Renamed
            return suffix_str.length() <= str_val.length() && std::equal(suffix_str.rbegin(), suffix_str.rend(), str_val.rbegin());
          } else if (method_expr->get_name() == "startswith") {
            vargs_val.expectArgs("startswith method", {1, 1}, {0, 0});
            auto prefix_str = vargs_val.args[0].get<std::string>(); // Renamed
            return prefix_str.length() <= str_val.length() && std::equal(prefix_str.begin(), prefix_str.end(), str_val.begin());
          } else if (method_expr->get_name() == "title") {
            vargs_val.expectArgs("title method", {0, 0}, {0, 0});
            auto res_str = str_val; // Renamed
            bool new_word = true;
            for (char &c : res_str) {
                if (std::isspace(c)) {
                    new_word = true;
                } else if (new_word) {
                    c = std::toupper(c);
                    new_word = false;
                } else {
                    c = std::tolower(c);
                }
            }
            return res_str;
          }
        }
        _printlog("Unknown method '" + method_expr->get_name() + "' for type " + obj_val.dump());
        return Value();
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};

class CallExpr : public Expression {
public:
    std::shared_ptr<Expression> object_expr; // Renamed
    ArgumentsExpression args_expr; // Renamed
    CallExpr(const Location & loc, std::shared_ptr<Expression> && obj_e, ArgumentsExpression && a_e) // Renamed
        : Expression(loc, Expression::Type_Call), object_expr(std::move(obj_e)), args_expr(std::move(a_e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!object_expr) {
            _printlog("CallExpr.object_expr is null");
            return Value();
        }
        auto obj_val = object_expr->evaluate(context); // Renamed
        if (!obj_val.is_callable()) {
             _printlog("Object is not callable: " + obj_val.dump(2) + error_location_suffix(*(location.source), location.pos));
            return Value();
        }
        auto vargs_val = args_expr.evaluate(context); // Renamed
        return obj_val.call(context, vargs_val);
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) { // Render nothing for null from a call (e.g. macro not returning)
            return LoopControlType::Normal;
        }
        // If it's a macro call that rendered something, it will be a string.
        // If it's a function call returning a structured type, use to_str for consistency.
        out << result.to_str(); 
        return LoopControlType::Normal;
    }
};

class FilterExpr : public Expression {
    std::vector<std::shared_ptr<Expression>> parts; // input | filter1 | filter2(arg) ...
public:
    FilterExpr(const Location & loc, std::vector<std::shared_ptr<Expression>> && p)
      : Expression(loc, Expression::Type_Filter), parts(std::move(p)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (parts.empty()) return Value();
        
        Value current_value = parts[0]->evaluate(context); // Initial value
        
        for (size_t i = 1; i < parts.size(); ++i) {
          auto& filter_part_expr = parts[i];
          if (!filter_part_expr) { _printlog("FilterExpr part is null"); continue; }

          if (filter_part_expr->mType == Expression::Type_Call) { // e.g., | filter(arg1, arg2)
              auto call_expr = static_cast<CallExpr*>(filter_part_expr.get());
              Value filter_fn = call_expr->object_expr->evaluate(context); // Evaluate the filter name/object
              if (!filter_fn.is_callable()) {
                  _printlog("Filter '" + call_expr->object_expr->evaluate(context).dump() + "' is not callable." + error_location_suffix(*(location.source), filter_part_expr->location.pos) );
                  return Value(); // Stop processing
              }
              ArgumentsValue filter_args_val = call_expr->args_expr.evaluate(context);
              // Prepend current_value as the first positional argument to the filter
              filter_args_val.args.insert(filter_args_val.args.begin(), current_value);
              current_value = filter_fn.call(context, filter_args_val);

          } else { // e.g., | filter  (filter is an identifier)
              Value filter_fn = filter_part_expr->evaluate(context);
              if (!filter_fn.is_callable()) {
                   _printlog("Filter '" + filter_part_expr->evaluate(context).dump() + "' is not callable." + error_location_suffix(*(location.source), filter_part_expr->location.pos));
                   return Value(); // Stop processing
              }
              ArgumentsValue filter_args_val;
              filter_args_val.args.push_back(current_value); // Current value is the only argument
              current_value = filter_fn.call(context, filter_args_val);
          }
        }
        return current_value;
    }

    void prepend(std::shared_ptr<Expression> && e) {
        parts.insert(parts.begin(), std::move(e));
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        Value result = do_evaluate(context);
        if (result.is_null()) {
            return LoopControlType::Normal;
        }
        if (result.is_boolean()) {
            out << (result.get<bool>() ? "True" : "False");
        } else {
            out << result.to_str();
        }
        return LoopControlType::Normal;
    }
};


// New Expression subclasses for template constructs

class TextExpr : public Expression {
public:
    std::string text;
    TextExpr(const Location & loc, const std::string& t)
      : Expression(loc, Expression::Type_Text), text(t) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(text); // Text itself is a value, though not typically "evaluated" further
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> &) const override {
        out << text;
        return LoopControlType::Normal;
    }
};

class SequenceExpr : public Expression {
public:
    std::vector<std::shared_ptr<Expression>> children;
    SequenceExpr(const Location & loc, std::vector<std::shared_ptr<Expression>> && c)
      : Expression(loc, Expression::Type_Sequence), children(std::move(c)) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); 
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        for (const auto& child : children) {
            if (child) {
                auto control_type = child->render(out, context);
                if (control_type != LoopControlType::Normal) {
                    return control_type; // Propagate break/continue
                }
            }
        }
        return LoopControlType::Normal;
    }
};

class CommentExpr : public Expression {
public:
    CommentExpr(const Location & loc, const std::string& /*text_content*/) // text_content not stored
      : Expression(loc, Expression::Type_Comment) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Comments evaluate to nothing
    }
    LoopControlType render(std::ostringstream &, const std::shared_ptr<Context> &) const override {
        return LoopControlType::Normal; // Comments render nothing
    }
};

class ConditionalBlockExpr : public Expression { // For {% if ... %} blocks
public:
    // Each pair is (condition_expr, body_expr). Else clause has nullptr for condition_expr.
    std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> cascade;
    ConditionalBlockExpr(const Location & loc, std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> && c)
        : Expression(loc, Expression::Type_ConditionalBlock), cascade(std::move(c)) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Block statements don't typically evaluate to a value themselves
    }
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
      for (const auto& branch : cascade) {
          bool enter_branch = true;
          if (branch.first) { // Condition is present (if/elif, not an else clause)
            enter_branch = branch.first->evaluate(context).to_bool();
          }
          if (enter_branch) {
            if (!branch.second) _printlog("ConditionalBlockExpr branch body is null" + error_location_suffix(*(location.source), location.pos));
            else {
                // Body is an Expression (likely SequenceExpr), call its render
                auto control_type = branch.second->render(out, context); 
                if (control_type != LoopControlType::Normal) return control_type; // Propagate
            }
            return LoopControlType::Normal; 
          }
      }
        return LoopControlType::Normal;
    }
};

class ForExpr : public Expression {
public:
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> iterable_expr;
    std::shared_ptr<Expression> condition_expr; 
    std::shared_ptr<Expression> body_expr;
    bool recursive;
    std::shared_ptr<Expression> else_body_expr;

    ForExpr(const Location & loc, std::vector<std::string> && vns, std::shared_ptr<Expression> && iter_e,
            std::shared_ptr<Expression> && cond_e, std::shared_ptr<Expression> && body_e, bool rec, std::shared_ptr<Expression> && else_e)
      : Expression(loc, Expression::Type_For), var_names(std::move(vns)), iterable_expr(std::move(iter_e)),
        condition_expr(std::move(cond_e)), body_expr(std::move(body_e)), recursive(rec), else_body_expr(std::move(else_e)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value();
    }

    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
      if (!iterable_expr) _printlog("ForExpr.iterable_expr is null");
      if (!body_expr) _printlog("ForExpr.body_expr is null");

      auto iterable_value = iterable_expr->evaluate(context);
      Value::CallableType loop_function_val; 

      std::function<LoopControlType(Value&, const std::shared_ptr<Context>&)> visit = 
          [&](Value& current_iterable_items, const std::shared_ptr<Context>& current_loop_context_parent) {
          auto filtered_items = Value::array();
          if (!current_iterable_items.is_null() && current_iterable_items.is_iterable()) {
            current_iterable_items.for_each([&](Value & item_val) { // Renamed
                auto temp_eval_context = Context::make(Value::object(), current_loop_context_parent);
                destructuring_assign(var_names, temp_eval_context, item_val);
                if (!condition_expr || condition_expr->evaluate(temp_eval_context).to_bool()) {
                  filtered_items.push_back(item_val);
                }
            });
          }

          if (filtered_items.empty()) {
            if (else_body_expr) {
              return else_body_expr->render(out, current_loop_context_parent);
            }
          } else {
              auto loop_vars_obj = Value::object(); 
              if (recursive) {
                  loop_vars_obj.set("self", Value::callable(loop_function_val));
              }
              
              auto loop_context = Context::make(Value::object(), current_loop_context_parent);
              loop_context->set("loop", loop_vars_obj);


              size_t num_items = filtered_items.size();
              loop_vars_obj.set("length", (int64_t) num_items);
              
              size_t cycle_idx_val = 0;
              loop_vars_obj.set("cycle", Value::callable([&cycle_idx_val](const std::shared_ptr<Context> &, ArgumentsValue & cycle_args) {
                  if (cycle_args.args.empty()) _printlog("cycle() expects at least 1 argument");
                  Value item_to_cycle = cycle_args.args[cycle_idx_val % cycle_args.args.size()];
                  cycle_idx_val++;
                  return item_to_cycle;
              }));


              for (size_t i = 0; i < num_items; ++i) {
                  auto & item_val = filtered_items.at(i); // Renamed
                  destructuring_assign(var_names, loop_context, item_val); 

                  loop_vars_obj.set("index", (int64_t) i + 1);
                  loop_vars_obj.set("index0", (int64_t) i);
                  loop_vars_obj.set("revindex", (int64_t) (num_items - i));
                  loop_vars_obj.set("revindex0", (int64_t) (num_items - i - 1));
                  loop_vars_obj.set("first", i == 0);
                  loop_vars_obj.set("last", i == (num_items - 1));
                  loop_vars_obj.set("previtem", i > 0 ? filtered_items.at(i - 1) : Value());
                  loop_vars_obj.set("nextitem", i < num_items - 1 ? filtered_items.at(i + 1) : Value());
                  
                  auto control = body_expr->render(out, loop_context);
                  if (control == LoopControlType::Break) return LoopControlType::Normal; 
                  if (control == LoopControlType::Continue) continue;
                  if (control != LoopControlType::Normal) return control; 
              }
          }
          return LoopControlType::Normal;
      };

      if (recursive) {
        loop_function_val = [&](const std::shared_ptr<Context> & call_context, ArgumentsValue & args) {
            if (args.args.size() != 1 || !args.kwargs.empty() || !args.args[0].is_iterable()) {
                _printlog("Recursive loop() expects exactly 1 positional iterable argument");
                return Value(); 
            }
            auto & items_for_recursion = args.args[0];
            std::ostringstream recursive_out; 
            visit(items_for_recursion, call_context); 
            return Value(recursive_out.str()); 
        };
      }
      return visit(iterable_value, context);
  }
};


class MacroDeclExpr : public Expression {
public:
    std::shared_ptr<VariableExpr> name_expr;
    Expression::Parameters params; // name, default_value_expr
    std::shared_ptr<Expression> body_expr; // Body is an Expression
    std::unordered_map<std::string, size_t> named_param_positions;

    MacroDeclExpr(const Location & loc, std::shared_ptr<VariableExpr> && n_expr, Expression::Parameters && p, std::shared_ptr<Expression> && b_expr) // Renamed
        : Expression(loc, Expression::Type_MacroDecl), name_expr(std::move(n_expr)), params(std::move(p)), body_expr(std::move(b_expr)) {
        for (size_t i = 0; i < params.size(); ++i) {
          const auto & param_name_str = params[i].first;
          if (!param_name_str.empty()) {
            named_param_positions[param_name_str] = i;
          }
        }
    }

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Macro definition itself doesn't evaluate to a printable value
    }

    LoopControlType render(std::ostringstream &, const std::shared_ptr<Context> & context_of_definition) const override { // Renamed
        if (!name_expr) _printlog("MacroDeclExpr.name_expr is null");
        if (!body_expr) _printlog("MacroDeclExpr.body_expr is null");

        const MacroDeclExpr* self = this; // Capture this for lambda

        auto callable_val = Value::callable([self, context_of_definition](const std::shared_ptr<Context> & call_time_context, ArgumentsValue & args) {
            auto execution_context = Context::make(Value::object(), context_of_definition); // Macros inherit from definition context
            
            std::vector<bool> param_set(self->params.size(), false);

            for (size_t i = 0; i < args.args.size(); ++i) {
                if (i >= self->params.size()) {
                     _printlog("Too many positional arguments for macro " + self->name_expr->get_name());
                     break; 
                }
                execution_context->set(self->params[i].first, args.args[i]);
                param_set[i] = true;
            }

            for (const auto& kwarg : args.kwargs) {
                auto it_pos = self->named_param_positions.find(kwarg.first); // Renamed
                if (it_pos == self->named_param_positions.end()) {
                    _printlog("Unknown keyword argument '" + kwarg.first + "' for macro " + self->name_expr->get_name());
                    continue; 
                }
                if (param_set[it_pos->second]) {
                     _printlog("Argument '" + kwarg.first + "' provided both positionally and as keyword for macro " + self->name_expr->get_name());
                     continue; 
                }
                execution_context->set(kwarg.first, kwarg.second);
                param_set[it_pos->second] = true;
            }

            for (size_t i = 0; i < self->params.size(); ++i) {
                if (!param_set[i]) {
                    if (self->params[i].second) { 
                        execution_context->set(self->params[i].first, self->params[i].second->evaluate(context_of_definition)); 
                    } else {
                         execution_context->set(self->params[i].first, Value()); // Undefined param
                    }
                }
            }
            
            std::ostringstream macro_out_str; // Renamed
            self->body_expr->render(macro_out_str, execution_context); // Body is an Expression
            return Value(macro_out_str.str()); // Macro call evaluates to the rendered string of its body
        });
        context_of_definition->set(name_expr->get_name(), callable_val);
        return LoopControlType::Normal; // Defining a macro renders nothing itself
    }
};


class SetVarExpr : public Expression { // For {% set var = value %}
public:
    std::string ns; 
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> value_expr;

    SetVarExpr(const Location & loc, const std::string & namespace_val, const std::vector<std::string> & vns, std::shared_ptr<Expression> && val_e) // Renamed
      : Expression(loc, Expression::Type_SetVar), ns(namespace_val), var_names(vns), value_expr(std::move(val_e)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Set statement doesn't evaluate to a printable value
    }

    LoopControlType render(std::ostringstream &, const std::shared_ptr<Context> & context) const override {
      if (!value_expr) { _printlog("SetVarExpr.value_expr is null." + error_location_suffix(*(location.source), location.pos)); return LoopControlType::Normal; }
      Value val_to_set = value_expr->evaluate(context);

      if (!ns.empty()) {
        if (var_names.size() != 1) {
          _printlog("Namespaced set only supports a single variable name." + error_location_suffix(*(location.source), location.pos));
          return LoopControlType::Normal; 
        }
        auto & single_var_name = var_names[0];
        Value ns_object = context->get(ns); // Get the namespace object
        if (!ns_object.is_object()) {
          _printlog("Namespace '" + ns + "' is not an object or does not exist." + error_location_suffix(*(location.source), location.pos));
          return LoopControlType::Normal; 
        }
        ns_object.set(single_var_name, val_to_set); // Set on the namespace object
      } else {
        destructuring_assign(var_names, context, val_to_set);
      }
      return LoopControlType::Normal; // Set renders nothing
    }
};


class SetBlockExpr : public Expression { // For {% set var %}...body...{% endset %}
public:
    std::string name; // Single variable name
    std::shared_ptr<Expression> body_expr; // Body of the set block, an Expression

    SetBlockExpr(const Location & loc, const std::string & n, std::shared_ptr<Expression> && b_expr) // Renamed
      : Expression(loc, Expression::Type_SetBlock), name(n), body_expr(std::move(b_expr)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Set block doesn't evaluate to a printable value itself
    }

    LoopControlType render(std::ostringstream &, const std::shared_ptr<Context> & context) const override {
      if (!body_expr) {
          _printlog("SetBlockExpr.body_expr is null" + error_location_suffix(*(location.source), location.pos));
          context->set(name, Value("")); 
          return LoopControlType::Normal;
      }
      std::ostringstream block_out_str; // Renamed
      body_expr->render(block_out_str, context); // Render the body (which is an Expression)
      context->set(name, Value(block_out_str.str()));
      return LoopControlType::Normal; // Set block renders nothing itself
    }
};

class FilterBlockExpr : public Expression { // For {% filter name %}...body...{% endfilter %}
public:
    std::shared_ptr<Expression> filter_expr; // The filter to apply (can be complex e.g. name(args))
    std::shared_ptr<Expression> body_expr;   // The body content, an Expression

    FilterBlockExpr(const Location & loc, std::shared_ptr<Expression> && f_expr, std::shared_ptr<Expression> && b_expr) // Renamed
        : Expression(loc, Expression::Type_FilterBlock), filter_expr(std::move(f_expr)), body_expr(std::move(b_expr)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Filter block itself doesn't have a direct evaluation value for {{ }}
    }

    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        if (!filter_expr) _printlog("FilterBlockExpr.filter_expr is null" + error_location_suffix(*(location.source), location.pos));
        if (!body_expr) _printlog("FilterBlockExpr.body_expr is null" + error_location_suffix(*(location.source), location.pos));

        // 1. Render the body to a temporary string
        std::ostringstream body_out_str; // Renamed
        if (body_expr) {
            body_expr->render(body_out_str, context); // Body is an Expression
        }
        Value body_content_val(body_out_str.str()); // Renamed

        // 2. Evaluate the filter_expr to get the filter function
        if (!filter_expr) { // Should not happen if check above passed
            out << body_content_val.to_str(); // No filter, render body as is
            return LoopControlType::Normal;
        }
        
        Value filter_fn_val; // Renamed
        ArgumentsValue filter_call_args; // Arguments for the filter function call

        if (filter_expr->mType == Expression::Type_Call) { // Filter is like | name(arg1, arg2)
            auto call_e = static_cast<CallExpr*>(filter_expr.get());
            filter_fn_val = call_e->object_expr->evaluate(context);
            filter_call_args = call_e->args_expr.evaluate(context);
        } else { // Filter is like | name
            filter_fn_val = filter_expr->evaluate(context);
        }

        if (!filter_fn_val.is_callable()) {
            _printlog("Filter expression in FilterBlockExpr did not evaluate to a callable: " + filter_fn_val.dump() + error_location_suffix(*(location.source), filter_expr->location.pos));
            out << body_content_val.to_str(); // Fallback: render body without filter
            return LoopControlType::Normal;
        }
        
        // 3. Prepend the rendered body content as the first argument to the filter
        filter_call_args.args.insert(filter_call_args.args.begin(), body_content_val);
        
        // 4. Call the filter
        Value filtered_result_val = filter_fn_val.call(context, filter_call_args); // Renamed
        out << filtered_result_val.to_str();
        return LoopControlType::Normal;
    }
};

class LoopControlExpr : public Expression { // For break/continue
public:
    LoopControlType control_type_;
    LoopControlExpr(const Location & loc, LoopControlType ct)
      : Expression(loc, Expression::Type_LoopControl), control_type_(ct) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        return Value(); // Not evaluated
    }
    LoopControlType render(std::ostringstream &, const std::shared_ptr<Context> &) const override {
        return control_type_; // Signal to the loop renderer
    }
};


class Parser {
private:
    using CharIterator = std::string::const_iterator;

    std::shared_ptr<std::string> template_str;
    CharIterator start, end, it;
    Options options;

    Parser(const std::shared_ptr<std::string>& t_str, const Options & opts) : template_str(t_str), options(opts) { // Renamed
      if (!template_str) _printlog("Template string is null");
      start = it = this->template_str->begin();
      end = this->template_str->end();
    }

    bool consumeSpaces(SpaceHandling space_handling = SpaceHandling::Strip) {
      if (space_handling == SpaceHandling::Strip) {
        while (it != end && std::isspace(*it)) ++it;
      }
      return true;
    }

    std::shared_ptr<std::string> parseString() {
      auto doParse = [&](char quote) -> std::shared_ptr<std::string> {
        if (it == end || *it != quote) return nullptr;
        std::string result_str; // Renamed
        bool escape = false;
        for (++it; it != end; ++it) {
          if (escape) {
            escape = false;
            switch (*it) {
              case 'n': result_str += '\n'; break;
              case 'r': result_str += '\r'; break;
              case 't': result_str += '\t'; break;
              case 'b': result_str += '\b'; break;
              case 'f': result_str += '\f'; break;
              case '\\': result_str += '\\'; break;
              default:
                if (*it == quote) {
                  result_str += quote;
                } else {
                  result_str += *it;
                }
                break;
            }
          } else if (*it == '\\') {
            escape = true;
          } else if (*it == quote) {
              ++it;
              return std::make_shared<std::string>(result_str);
          } else {
            result_str += *it;
          }
        }
        _printlog("Unterminated string literal." + error_location_suffix(*template_str, get_location().pos - result_str.length() -1));
        return nullptr; // Unterminated string
      };

      consumeSpaces();
      if (it == end) return nullptr;
      if (*it == '"') return doParse('"');
      if (*it == '\'') return doParse('\'');
      return nullptr;
    }

    json parseNumber(CharIterator& it_ref, const CharIterator& end_ref) { 
        auto before_num = it_ref; // Renamed
        auto num_start = it_ref;
        bool hasDecimal = false;
        bool hasExponent = false;

        if (it_ref != end_ref && (*it_ref == '-' || *it_ref == '+')) ++it_ref;

        CharIterator non_digit_it = it_ref; 

        while(non_digit_it != end_ref && std::isdigit(*non_digit_it)) {
            non_digit_it++;
        }


        if (non_digit_it == it_ref && (non_digit_it == end_ref || (*non_digit_it != '.' || (non_digit_it +1 == end_ref || !std::isdigit(*(non_digit_it+1))) )) ) { 
             it_ref = before_num;
             return json();
        }


        it_ref = non_digit_it; 

        if (it_ref != end_ref && *it_ref == '.') {
            hasDecimal = true;
            ++it_ref; 
            CharIterator exponent_it = it_ref;
            while(exponent_it != end_ref && std::isdigit(*exponent_it)) {
                 exponent_it++;
            }
            // Allow number like "1." (where exponent_it == it_ref after '.')
            // But not just "." if nothing was before. num_start would be equal to it_ref before '.'
            if (exponent_it == it_ref && num_start == (it_ref-1) ) { 
                it_ref = before_num;
                return json();
            }
            it_ref = exponent_it;
        }


        if (it_ref != end_ref && (*it_ref == 'e' || *it_ref == 'E')) {
            // Check if there was at least one digit before 'e' or a decimal point
            if (num_start == it_ref && !hasDecimal) { // e.g. "e10" is not valid, but ".e10" could be if grammar allows
                 it_ref = before_num; return json();
            }
            hasExponent = true;
            ++it_ref; 
            if (it_ref != end_ref && (*it_ref == '-' || *it_ref == '+')) ++it_ref; 
            
            CharIterator final_it = it_ref;
            while(final_it != end_ref && std::isdigit(*final_it)) {
                 final_it++;
            }
            if (final_it == it_ref) { 
                it_ref = before_num;
                return json();
            }
            it_ref = final_it;
        }
        
        std::string str_num(num_start, it_ref); // Renamed
        if (str_num.empty() || (str_num.length() == 1 && (str_num[0] == '-' || str_num[0] == '+')) || str_num == ".") {
            it_ref = before_num;
            return json();
        }

        try {
            if (hasExponent || hasDecimal) {
                double v_num = std::stod(str_num); // Renamed
                return json(v_num);
            }
            int64_t v_num = std::stoll(str_num); // Renamed
            return json(v_num);
        } catch (const std::out_of_range& oor) {
             _printlog("Number out of range: " + str_num + error_location_suffix(*template_str, (size_t)std::distance(start, num_start)));
             it_ref = before_num;
             return json();
        } catch (const std::invalid_argument& ia) {
            _printlog("Invalid number format: " + str_num + error_location_suffix(*template_str, (size_t)std::distance(start, num_start)));
            it_ref = before_num;
            return json();
        }
    }


    std::shared_ptr<Value> parseConstant() {
      auto start_iter = it; 
      consumeSpaces();
      Location constant_loc = get_location(); // Get location after spaces for more accuracy
      if (it == end) { it = start_iter; return nullptr; }

      if (*it == '"' || *it == '\'') {
        auto str_val = parseString(); 
          if (str_val) return std::make_shared<Value>(*str_val);
          it = start_iter; 
          return nullptr;
      }
      
      static std::regex prim_tok(R"(true\b|True\b|false\b|False\b|None\b|none\b)"); // Added 'none'
      auto token_str = consumeToken(prim_tok); // consumeToken uses current 'it' (after spaces)
      if (!token_str.empty()) {
        if (token_str == "true" || token_str == "True") return std::make_shared<Value>(true);
        if (token_str == "false" || token_str == "False") return std::make_shared<Value>(false);
        if (token_str == "None" || token_str == "none") return std::make_shared<Value>(nullptr); // Treat 'none' as None
      } else {
          // If no keyword, try parsing a number from current 'it'
      }


      auto number_val = parseNumber(it, end); 
      if (!number_val.is_null()) return std::make_shared<Value>(number_val);

      it = start_iter; 
      return nullptr;
    }

    bool peekSymbols(const std::vector<std::string> & symbols) const {
        auto temp_it = it; // Use a temporary iterator for peeking
        // consumeSpaces(SpaceHandling::Keep); // Don't advance main 'it' for peeking spaces. consumeToken will handle it.
        for (const auto & symbol : symbols) {
            if (std::distance(temp_it, end) >= (int64_t) symbol.size() && std::string(temp_it, temp_it + symbol.size()) == symbol) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> consumeTokenGroups(const std::regex & regex, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto current_it_start = it;
        if (space_handling == SpaceHandling::Strip) consumeSpaces();
        
        std::smatch match;
        std::string s_to_search(it, end); // More robust to create string for searching
        if (std::regex_search(s_to_search, match, regex, std::regex_constants::match_continuous)) { 
             if (match.position(0) == 0) { 
                it += match.length(0);
                std::vector<std::string> ret;
                for (size_t i = 0; i < match.size(); ++i) {
                    ret.push_back(match[i].str());
                }
                return ret;
            }
        }
        it = current_it_start; 
        return {};
    }
    std::string consumeToken(const std::regex & regex, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto current_it_start = it;
        if (space_handling == SpaceHandling::Strip) consumeSpaces();
        
        std::smatch match;
        std::string s_to_search(it, end);
        if (std::regex_search(s_to_search, match, regex, std::regex_constants::match_continuous)) {
             if (match.position(0) == 0) {
                it += match.length(0);
                return match[0].str();
            }
        }
        it = current_it_start;
        return "";
    }

    std::string consumeToken(const std::string & token, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto current_it_start = it;
        if (space_handling == SpaceHandling::Strip) consumeSpaces();
        if (std::distance(it, end) >= (int64_t) token.size() && std::string(it, it + token.size()) == token) {
            it += token.size();
            return token;
        }
        it = current_it_start;
        return "";
    }

    std::shared_ptr<Expression> parseExpression(bool allow_if_expr = true) {
        auto left_expr = parseLogicalOr(); // Renamed
        if (!left_expr || it == end) return left_expr;


        if (!allow_if_expr) return left_expr;

        auto if_loc = get_location();
        static std::regex if_tok(R"(if\b)");
        // Need to ensure 'if' is not part of a larger identifier and is followed by space/operator
        auto temp_it_for_if_check = it;
        consumeSpaces(SpaceHandling::Keep); // Don't advance main 'it' for peeking
        if (std::distance(temp_it_for_if_check, end) >= 2 && std::string(temp_it_for_if_check, temp_it_for_if_check + 2) == "if" &&
            (std::distance(temp_it_for_if_check, end) == 2 || !std::isalnum(*(temp_it_for_if_check+2)) ) ) {
            // Potential 'if', try to consume
            if (consumeToken(if_tok).empty()) { // This will handle spaces correctly
                 return left_expr; 
            }
        } else {
            return left_expr;
        }
        
        auto condition_expr = parseLogicalOr(); // Renamed
        if (!condition_expr) _printlog("Expected condition in ternary 'if' expression" + error_location_suffix(*template_str, if_loc.pos));

        std::shared_ptr<Expression> else_branch_expr; // Renamed
        static std::regex else_tok(R"(else\b)");
        auto else_loc = get_location();
        if (!consumeToken(else_tok).empty()) {
          else_branch_expr = parseExpression(false); 
          if (!else_branch_expr) _printlog("Expected 'else' expression in ternary 'if'" + error_location_suffix(*template_str, else_loc.pos));
        } else {
             _printlog("Expected 'else' in ternary 'if' expression" + error_location_suffix(*template_str, else_loc.pos)); 
        }
        return std::make_shared<IfExpr>(if_loc, std::move(condition_expr), std::move(left_expr), std::move(else_branch_expr));
    }

    Location get_location() const {
        return {template_str, (size_t) std::distance(start, it)};
    }

    std::shared_ptr<Expression> parseLogicalOr() {
        auto left_expr = parseLogicalAnd(); // Renamed
        if (!left_expr) return nullptr; 

        static std::regex or_tok(R"(or\b)");
        while (true) {
            auto op_loc = get_location();
            if (consumeToken(or_tok).empty()) break;
            auto right_expr = parseLogicalAnd(); // Renamed
            if (!right_expr) _printlog("Expected right side of 'or' expression" + error_location_suffix(*template_str, op_loc.pos));
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), BinaryOpExpr::Op::Or);
        }
        return left_expr;
    }

    std::shared_ptr<Expression> parseLogicalNot() {
        static std::regex not_tok(R"(not\b)");
        auto op_loc = get_location();
        if (!consumeToken(not_tok).empty()) {
          auto sub_expr = parseLogicalNot(); // Renamed
          if (!sub_expr) _printlog("Expected expression after 'not' keyword" + error_location_suffix(*template_str, op_loc.pos));
          return std::make_shared<UnaryOpExpr>(op_loc, std::move(sub_expr), UnaryOpExpr::Op::LogicalNot);
        }
        return parseLogicalCompare();
    }

    std::shared_ptr<Expression> parseLogicalAnd() {
        auto left_expr = parseLogicalNot(); // Renamed
        if (!left_expr) return nullptr;

        static std::regex and_tok(R"(and\b)");
        while (true) {
            auto op_loc = get_location();
            if (consumeToken(and_tok).empty()) break;
            auto right_expr = parseLogicalNot(); // Renamed
            if (!right_expr) _printlog("Expected right side of 'and' expression" + error_location_suffix(*template_str, op_loc.pos));
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), BinaryOpExpr::Op::And);
        }
        return left_expr;
    }

    std::shared_ptr<Expression> parseLogicalCompare() {
        auto left_expr = parseStringConcat(); // Renamed                                   
        if (!left_expr) return nullptr;

        static std::regex compare_tok(R"(==|!=|<=?|>=?|in\b|not\s+in\b|is\s+not\b|is\b)"); 
        
        while(true) {
            auto op_loc = get_location();
            std::string op_str = consumeToken(compare_tok);
            if (op_str.empty()) break;

            BinaryOpExpr::Op op_enum; // Renamed
            std::shared_ptr<Expression> right_expr; // Renamed

            if (op_str == "is") {
                op_enum = BinaryOpExpr::Op::Is;
                right_expr = parseIdentifier(); 
                if (!right_expr) _printlog("Expected type name after 'is' operator" + error_location_suffix(*template_str, op_loc.pos));
            } else if (op_str == "is not") {
                op_enum = BinaryOpExpr::Op::IsNot;
                right_expr = parseIdentifier(); 
                 if (!right_expr) _printlog("Expected type name after 'is not' operator" + error_location_suffix(*template_str, op_loc.pos));
            } else {
                right_expr = parseStringConcat(); 
                if (!right_expr) _printlog("Expected right side of '" + op_str + "' expression" + error_location_suffix(*template_str, op_loc.pos));
                
                if (op_str == "==") op_enum = BinaryOpExpr::Op::Eq;
                else if (op_str == "!=") op_enum = BinaryOpExpr::Op::Ne;
                else if (op_str == "<") op_enum = BinaryOpExpr::Op::Lt;
                else if (op_str == ">") op_enum = BinaryOpExpr::Op::Gt;
                else if (op_str == "<=") op_enum = BinaryOpExpr::Op::Le;
                else if (op_str == ">=") op_enum = BinaryOpExpr::Op::Ge;
                else if (op_str == "in") op_enum = BinaryOpExpr::Op::In;
                else if (op_str == "not in") op_enum = BinaryOpExpr::Op::NotIn;
                else { _printlog("Unknown comparison operator: " + op_str + error_location_suffix(*template_str, op_loc.pos)); return left_expr; } 
            }
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), op_enum);
        }
        return left_expr;
    }


    Expression::Parameters parseParameters() { 
        consumeSpaces();
        auto paren_loc = get_location();
        if (consumeToken("(").empty()) _printlog("Expected opening parenthesis in parameter list" + error_location_suffix(*template_str, paren_loc.pos));

        Expression::Parameters result_params; // Renamed
        bool first_param = true; // Renamed
        while (it != end) {
            consumeSpaces();
            if (!consumeToken(")").empty()) {
                return result_params;
            }
            if (!first_param) {
                auto comma_loc = get_location();
                if (consumeToken(",").empty()) _printlog("Expected comma or closing parenthesis in parameter list" + error_location_suffix(*template_str, comma_loc.pos));
            }
            first_param = false;

            auto ident_expr = parseIdentifier(); // Renamed
            if (!ident_expr) _printlog("Expected parameter name in parameter list" + error_location_suffix(*template_str, get_location().pos));
            
            std::shared_ptr<Expression> default_val_expr; // Renamed
            if (!consumeToken("=").empty()) {
                default_val_expr = parseExpression(false); 
                if (!default_val_expr) _printlog("Expected default value expression for parameter " + ident_expr->get_name() + error_location_suffix(*template_str, get_location().pos));
            }
            result_params.emplace_back(ident_expr->get_name(), std::move(default_val_expr));
        }
        _printlog("Expected closing parenthesis in parameter list" + error_location_suffix(*template_str, paren_loc.pos));
        return result_params; 
    }

    ArgumentsExpression parseCallArgs() { 
        consumeSpaces();
        auto paren_loc = get_location();
        if (consumeToken("(").empty()) _printlog("Expected opening parenthesis in call arguments" + error_location_suffix(*template_str, paren_loc.pos));

        ArgumentsExpression result_args; // Renamed
        bool first_arg = true; // Renamed
        while (it != end) {
            consumeSpaces();
            if (!consumeToken(")").empty()) {
                return result_args;
            }
            if (!first_arg) {
                 auto comma_loc = get_location();
                if (consumeToken(",").empty()) _printlog("Expected comma or closing parenthesis in call arguments" + error_location_suffix(*template_str, comma_loc.pos));
            }
            first_arg = false;

            auto ident_loc = get_location();
            auto kwarg_name_str = consumeToken(R"([a-zA-Z_]\w*(?=\s*=))"); // Renamed
            
            if (!kwarg_name_str.empty()) {
                 consumeToken("="); 
                 auto value_expr = parseExpression(true); // Renamed
                 if (!value_expr) _printlog("Expected expression for keyword argument '" + kwarg_name_str + "'" + error_location_suffix(*template_str, get_location().pos));
                 result_args.kwargs.emplace_back(kwarg_name_str, std::move(value_expr));
            } else {
                auto expr_val = parseExpression(true); // Renamed
                if (!expr_val) _printlog("Expected expression in call arguments" + error_location_suffix(*template_str, get_location().pos));
                result_args.args.emplace_back(std::move(expr_val));
            }
        }
        _printlog("Expected closing parenthesis in call arguments" + error_location_suffix(*template_str, paren_loc.pos));
        return result_args; 
    }


    std::shared_ptr<VariableExpr> parseIdentifier() {
        static std::regex ident_regex(R"((?!(?:not|is|and|or|del|in|if|else|true|True|false|False|None|none)\b)[a-zA-Z_]\w*)"); 
        auto location = get_location();
        auto ident_str = consumeToken(ident_regex); // Renamed
        if (ident_str.empty())
          return nullptr;
        return std::make_shared<VariableExpr>(location, ident_str);
    }

    std::shared_ptr<Expression> parseStringConcat() {
        auto left_expr = parseMathPlusMinus(); 
        if (!left_expr) return nullptr;

        static std::regex concat_tok(R"(~(?!\}))"); 
        while (true) {
            auto op_loc = get_location();
            if (consumeToken(concat_tok).empty()) break;
            auto right_expr = parseMathPlusMinus(); 
            if (!right_expr) _printlog("Expected right side of '~' (string concat) expression" + error_location_suffix(*template_str, op_loc.pos));
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), BinaryOpExpr::Op::StrConcat);
        }
        return left_expr;
    }
    
    std::shared_ptr<Expression> parseMathPlusMinus() {
        auto left_expr = parseMathMulDivMod(); // Renamed for clarity
        if (!left_expr) return nullptr;

        static std::regex plus_minus_tok(R"(\+|-(?![}%#]\}))"); 
        while (true) {
            auto op_loc = get_location();
            std::string op_str = consumeToken(plus_minus_tok);
            if (op_str.empty()) break;
            
            auto right_expr = parseMathMulDivMod(); // Renamed
            if (!right_expr) _printlog("Expected right side of '" + op_str + "' expression" + error_location_suffix(*template_str, op_loc.pos));
            auto op_enum = op_str == "+" ? BinaryOpExpr::Op::Add : BinaryOpExpr::Op::Sub; // Renamed
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), op_enum);
        }
        return left_expr;
    }

    std::shared_ptr<Expression> parseMathMulDivMod() { // Renamed from parseMathMulDiv
        auto left_expr = parseMathPow(); // Power has higher precedence
        if (!left_expr) return nullptr;

        static std::regex mul_div_mod_tok(R"(\*|//|/|%(?!\}))"); // Removed ** from here
                                                              
        while(true) {
            auto op_loc = get_location();
            std::string op_str = consumeToken(mul_div_mod_tok);
             if (op_str.empty()) { 
                if (consumeToken("|").empty()) break; 
                auto filter_part_expr = parseMathPow(); // Filters apply to result of power
                if (!filter_part_expr) _printlog("Expected filter expression after '|'" + error_location_suffix(*template_str, op_loc.pos));

                if (left_expr->mType == Expression::Type_Filter) { 
                    auto existing_filter = std::static_pointer_cast<FilterExpr>(left_expr);
                    existing_filter->parts.push_back(filter_part_expr);
                } else { 
                    std::vector<std::shared_ptr<Expression>> parts_vec; // Renamed
                    parts_vec.emplace_back(std::move(left_expr));
                    parts_vec.emplace_back(std::move(filter_part_expr));
                    left_expr = std::make_shared<FilterExpr>(op_loc, std::move(parts_vec));
                }
                continue; 
            }

            auto right_expr = parseMathPow(); 
            if (!right_expr) _printlog("Expected right side of '" + op_str + "' expression" + error_location_suffix(*template_str, op_loc.pos));
            
            BinaryOpExpr::Op op_enum; // Renamed
            if (op_str == "*") op_enum = BinaryOpExpr::Op::Mul;
            else if (op_str == "/") op_enum = BinaryOpExpr::Op::Div;
            else if (op_str == "//") op_enum = BinaryOpExpr::Op::DivDiv;
            else if (op_str == "%") op_enum = BinaryOpExpr::Op::Mod;
            else { _printlog("Internal parser error, unknown mul/div/mod op: " + op_str); return left_expr; }
            
            left_expr = std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), op_enum);
        }
        return left_expr;
    }
     std::shared_ptr<Expression> parseMathPow() { 
        auto left_expr = parseMathUnaryPlusMinus(); // Renamed
        if (!left_expr) return nullptr;

        auto op_loc = get_location();
        if (consumeToken("**").empty()) {
            return left_expr; 
        }
        
        auto right_expr = parseMathPow(); // Renamed
        if (!right_expr) _printlog("Expected right side of '**' (power) expression" + error_location_suffix(*template_str, op_loc.pos));
        
        return std::make_shared<BinaryOpExpr>(op_loc, std::move(left_expr), std::move(right_expr), BinaryOpExpr::Op::MulMul);
    }


    std::shared_ptr<Expression> call_func(const std::string & name, ArgumentsExpression && args) const {
        return std::make_shared<CallExpr>(get_location(), std::make_shared<VariableExpr>(get_location(), name), std::move(args));
    }

    std::shared_ptr<Expression> parseMathUnaryPlusMinus() {
        static std::regex unary_plus_minus_tok(R"(\+|-(?![}%#]\}))"); 
        auto op_loc = get_location();
        auto op_str = consumeToken(unary_plus_minus_tok);
        
        auto expr_val = parseExpansion(); // Renamed
        if (!expr_val) {
            if (!op_str.empty()) _printlog("Expected expression after unary '" + op_str + "'" + error_location_suffix(*template_str, op_loc.pos));
            return nullptr;
        }

        if (!op_str.empty()) {
            auto op_enum = op_str == "+" ? UnaryOpExpr::Op::Plus : UnaryOpExpr::Op::Minus; // Renamed
            return std::make_shared<UnaryOpExpr>(op_loc, std::move(expr_val), op_enum);
        }
        return expr_val;
    }

    std::shared_ptr<Expression> parseExpansion() { 
      static std::regex expansion_tok(R"(\*\*?)"); 
      auto op_loc = get_location();
      auto op_str = consumeToken(expansion_tok);
      
      auto expr_val = parsePrimaryExpression(); // Renamed
      if (!expr_val) {
            if(!op_str.empty()) _printlog("Expected expression after expansion operator '" + op_str + "'" + error_location_suffix(*template_str, op_loc.pos));
            return nullptr;
      }
      if (op_str.empty()) return expr_val;
      
      return std::make_shared<UnaryOpExpr>(op_loc, std::move(expr_val), op_str == "*" ? UnaryOpExpr::Op::Expansion : UnaryOpExpr::Op::ExpansionDict);
    }

    std::shared_ptr<Expression> parsePrimaryExpression() {
        auto base_expr = parseAtom(); 
        if (!base_expr) return nullptr;

        while (it != end) {
            auto current_loc = get_location(); // Location of the . or [ or ( operator
            consumeSpaces(SpaceHandling::Keep); 
            
            if (consumeToken(".").empty()) { 
                if (consumeToken("[").empty()) { 
                    // Check for call only if it's not part of a tuple/array start, e.g. `(foo)(bar)` vs `(foo, bar)`
                    // This is simplified; a full solution might need more context.
                    // If `base_expr` could be a tuple/array and `(` follows, it's ambiguous.
                    // Jinja usually doesn't have `(a,b)(c)` syntax. `a | b (c)` is more common.
                    // So, if '(' follows directly, assume it's a call on base_expr.
                    if (peekSymbols({ "("})) { 
                        auto call_args = parseCallArgs();
                        base_expr = std::make_shared<CallExpr>(current_loc, std::move(base_expr), std::move(call_args));
                        continue; 
                    }
                    break; 
                }
                // Subscript "[]"
                std::shared_ptr<Expression> index_expr_val; // Renamed
                auto slice_start_loc = get_location();
                std::shared_ptr<Expression> start_slice, end_slice, step_slice;
                bool first_colon = false, second_colon = false;

                if (!peekSymbols({ ":" })) { 
                    start_slice = parseExpression(true); 
                }
                if (consumeToken(":").empty()) { 
                    index_expr_val = std::move(start_slice);
                    if (!index_expr_val) _printlog("Expected index expression inside []" + error_location_suffix(*template_str, slice_start_loc.pos));
                } else { 
                    first_colon = true;
                    if (!peekSymbols({ ":", "]" })) { 
                        end_slice = parseExpression(true);
                    }
                    if (!consumeToken(":").empty()) { 
                        second_colon = true;
                        if (!peekSymbols({ "]" })) {
                           step_slice = parseExpression(true);
                        }
                    }
                    if (first_colon || second_colon) { // Must have at least one colon for it to be a slice
                         index_expr_val = std::make_shared<SliceExpr>(slice_start_loc, std::move(start_slice), std::move(end_slice), std::move(step_slice));
                    } else { 
                        _printlog("Invalid slice syntax, found ':' but no slice parts." + error_location_suffix(*template_str, slice_start_loc.pos));
                        index_expr_val = std::move(start_slice); // Treat as simple index if only `expr:` was sort of parsed.
                    }
                }
                 auto bracket_loc = get_location();
                if (consumeToken("]").empty()) _printlog("Expected closing bracket ']' in subscript/slice" + error_location_suffix(*template_str, bracket_loc.pos));
                base_expr = std::make_shared<SubscriptExpr>(current_loc, std::move(base_expr), std::move(index_expr_val));

            } else { // Attribute access "."
                auto attr_name_expr = parseIdentifier();
                if (!attr_name_expr) _printlog("Expected identifier after '.'" + error_location_suffix(*template_str, current_loc.pos));
                
                auto paren_peek_loc = get_location();
                consumeSpaces(SpaceHandling::Keep); 
                if (peekSymbols({ "("})) { // Method call
                    auto call_args = parseCallArgs(); 
                    base_expr = std::make_shared<MethodCallExpr>(current_loc, std::move(base_expr), std::move(attr_name_expr), std::move(call_args));
                } else { // Attribute access
                    auto key_literal = std::make_shared<LiteralExpr>(attr_name_expr->location, Value(attr_name_expr->get_name()));
                    base_expr = std::make_shared<SubscriptExpr>(current_loc, std::move(base_expr), std::move(key_literal));
                }
            }
        }
        return base_expr;
    }

    std::shared_ptr<Expression> parseAtom() {
        auto location = get_location();
        auto constant_val = parseConstant(); // Renamed
        if (constant_val) return std::make_shared<LiteralExpr>(location, *constant_val);

        auto ident_expr = parseIdentifier(); // Renamed
        if (ident_expr) return ident_expr;
        
        auto paren_loc = get_location();
        if (!consumeToken("(").empty()) {
            auto expr_in_paren = parseExpression(true); // Renamed
            if (!expr_in_paren) _printlog("Expected expression inside parentheses" + error_location_suffix(*template_str, paren_loc.pos));
            auto close_paren_loc = get_location();
            if (consumeToken(")").empty()) _printlog("Expected closing parenthesis ')'" + error_location_suffix(*template_str, close_paren_loc.pos));
            return expr_in_paren; 
        }

        auto array_lit_expr = parseArray(); // Renamed
        if (array_lit_expr) return array_lit_expr;

        auto dict_lit_expr = parseDictionary(); // Renamed
        if (dict_lit_expr) return dict_lit_expr;
        
        return nullptr; 
    }


    std::shared_ptr<Expression> parseBracedExpressionOrArray() {
        _printlog("parseBracedExpressionOrArray is deprecated, use parseAtom for () and parseArray for []");
        return nullptr;
    }

    std::shared_ptr<Expression> parseArray() {
        auto bracket_loc = get_location();
        if (consumeToken("[").empty()) return nullptr;

        std::vector<std::shared_ptr<Expression>> elements_vec; // Renamed
        bool first_el = true; // Renamed
        while (it != end) {
            consumeSpaces();
            if (!consumeToken("]").empty()) {
                return std::make_shared<ArrayExpr>(bracket_loc, std::move(elements_vec));
            }
            if (!first_el) {
                auto comma_loc = get_location();
                if (consumeToken(",").empty()) _printlog("Expected comma or closing bracket ']' in array" + error_location_suffix(*template_str, comma_loc.pos));
            }
            first_el = false;
            
            auto elem_expr_val = parseExpression(true); // Renamed
            if (!elem_expr_val) { /* _printlog("Expected expression as array element" + error_location_suffix(*template_str, get_location().pos)); */  /* Allow trailing comma with no expr */ }
            else { elements_vec.push_back(std::move(elem_expr_val)); }
        }
        _printlog("Expected closing bracket ']' for array" + error_location_suffix(*template_str, bracket_loc.pos));
        return nullptr;
    }

    std::shared_ptr<Expression> parseDictionary() {
        auto brace_loc = get_location();
        if (consumeToken("{").empty()) return nullptr;

        std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> elements_map; // Renamed
        bool first_pair = true; // Renamed
        while (it != end) {
            consumeSpaces();
            if (!consumeToken("}").empty()) {
                return std::make_shared<DictExpr>(brace_loc, std::move(elements_map));
            }
            if (!first_pair) {
                auto comma_loc = get_location();
                if (consumeToken(",").empty()) _printlog("Expected comma or closing brace '}' in dictionary" + error_location_suffix(*template_str, comma_loc.pos));
            }
            first_pair = false;

            auto key_expr_val = parseExpression(false); // Renamed
            if (!key_expr_val) _printlog("Expected key in dictionary" + error_location_suffix(*template_str, get_location().pos));
            
            auto colon_loc = get_location();
            if (consumeToken(":").empty()) _printlog("Expected colon ':' between key and value in dictionary" + error_location_suffix(*template_str, colon_loc.pos));
            
            auto value_expr_val = parseExpression(true); // Renamed
            if (!value_expr_val) _printlog("Expected value in dictionary" + error_location_suffix(*template_str, get_location().pos));
            if(key_expr_val && value_expr_val) elements_map.emplace_back(std::move(key_expr_val), std::move(value_expr_val));
        }
        _printlog("Expected closing brace '}' for dictionary" + error_location_suffix(*template_str, brace_loc.pos));
        return nullptr;
    }


    SpaceHandling parsePreSpace(const std::string& s) const {
        if (s == "-")
          return SpaceHandling::Strip;
        return SpaceHandling::Keep;
    }

    SpaceHandling parsePostSpace(const std::string& s) const {
        if (s == "-") return SpaceHandling::Strip;
        return SpaceHandling::Keep;
    }

    using TemplateTokenVector = std::vector<std::shared_ptr<TemplateToken>>;
    using TemplateTokenIterator = TemplateTokenVector::const_iterator;

    std::vector<std::string> parseVarNames() {
      static std::regex varnames_regex(R"(((?:\w+)(?:\s*,\s*(?:\w+))*)\s*)");

      std::vector<std::string> group_match; // Renamed
      auto loc = get_location();
      if ((group_match = consumeTokenGroups(varnames_regex)).empty()) _printlog("Expected variable names" + error_location_suffix(*template_str, loc.pos));
      std::vector<std::string> varnames_list; // Renamed
      std::istringstream iss(group_match[1]);
      std::string varname_item; // Renamed
      while (std::getline(iss, varname_item, ',')) {
        varnames_list.push_back(strip(varname_item));
      }
      return varnames_list;
    }

    std::string unexpected(const TemplateToken & token) const {
      return std::string("Unexpected " + TemplateToken::typeToString(token.type)
        + error_location_suffix(*template_str, token.location.pos));
    }
    std::string unterminated(const TemplateToken & token) const {
      return std::string("Unterminated " + TemplateToken::typeToString(token.type)
        + error_location_suffix(*template_str, token.location.pos));
    }

    TemplateTokenVector tokenize() {
      static std::regex comment_tok(R"(\{#([-~]?)([\s\S]*?)([-~]?)#\})");
      static std::regex expr_open_regex(R"(\{\{([-~])?)");
      static std::regex block_open_regex(R"(^\{%([-~])?\s*)"); 
      static std::regex block_keyword_tok(R"((if|else|elif|endif|for|endfor|generation|endgeneration|set|endset|block|endblock|macro|endmacro|filter|endfilter|break|continue)\b)");
      static std::regex non_text_open_regex(R"(\{\{|\{%|\{#)");
      static std::regex expr_close_regex(R"(\s*([-~])?\}\})");
      static std::regex block_close_regex(R"(\s*([-~])?%\})");

      TemplateTokenVector tokens_list; // Renamed
      std::vector<std::string> matched_group; // Renamed
      std::string text_val; // Renamed
      std::smatch regex_match; // Renamed

        while (it != end) {
          auto current_location = get_location(); // Renamed
          auto pre_token_it = it; // Renamed

          if (!(matched_group = consumeTokenGroups(comment_tok, SpaceHandling::Keep)).empty()) {
            auto pre_sp = parsePreSpace(matched_group[1]); // Renamed
            auto content_str = matched_group[2]; // Renamed
            auto post_sp = parsePostSpace(matched_group[3]); // Renamed
            tokens_list.push_back(std::make_shared<CommentTemplateToken>(current_location, pre_sp, post_sp, content_str));
          } else if (!(matched_group = consumeTokenGroups(expr_open_regex, SpaceHandling::Keep)).empty()) {
            auto pre_sp = parsePreSpace(matched_group[1]);
            auto expr_val = parseExpression(); // Renamed
             if (!expr_val) _printlog("Failed to parse expression inside {{ ... }}" + error_location_suffix(*template_str, current_location.pos));

            auto close_loc = get_location(); // For error reporting
            if ((matched_group = consumeTokenGroups(expr_close_regex)).empty()) {
              _printlog("Expected closing expression tag '}}'" + error_location_suffix(*template_str, close_loc));
            }

            auto post_sp = parsePostSpace(matched_group.size() > 1 ? matched_group[1] : "");
            tokens_list.push_back(std::make_shared<ExpressionTemplateToken>(current_location, pre_sp, post_sp, std::move(expr_val)));
          } else if (!(matched_group = consumeTokenGroups(block_open_regex, SpaceHandling::Keep)).empty()) {
            auto pre_sp = parsePreSpace(matched_group[1]);
            auto block_kw_loc = get_location(); // Renamed
            std::string keyword_str; // Renamed

            auto parseBlkClose = [&]() -> SpaceHandling { // Renamed
              auto close_blk_loc = get_location(); // Renamed
              if ((matched_group = consumeTokenGroups(block_close_regex)).empty()) _printlog("Expected closing block tag '%}'" + error_location_suffix(*template_str, close_blk_loc));
              return parsePostSpace(matched_group.size() > 1 ? matched_group[1] : "");
            };

            if ((keyword_str = consumeToken(block_keyword_tok)).empty()) _printlog("Expected block keyword (if, for, etc.)" + error_location_suffix(*template_str, block_kw_loc.pos));

            if (keyword_str == "if") {
              auto cond_expr_loc = get_location(); // Renamed
              auto condition_expr = parseExpression(); // Renamed
              if (!condition_expr) _printlog("Expected condition in 'if' block" + error_location_suffix(*template_str, cond_expr_loc.pos));
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<IfTemplateToken>(current_location, pre_sp, post_sp, std::move(condition_expr)));
            } else if (keyword_str == "elif") {
              auto cond_expr_loc = get_location();
              auto condition_expr = parseExpression();
              if (!condition_expr) _printlog("Expected condition in 'elif' block" + error_location_suffix(*template_str, cond_expr_loc.pos));
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<ElifTemplateToken>(current_location, pre_sp, post_sp, std::move(condition_expr)));
            } else if (keyword_str == "else") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<ElseTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "endif") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndIfTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "for") {
              static std::regex recursive_tok(R"(recursive\b)");
              static std::regex if_tok(R"(if\b)");

              auto varnames_list = parseVarNames(); // Renamed
              static std::regex in_tok(R"(in\b)");
              auto in_kw_loc = get_location(); // Renamed
              if (consumeToken(in_tok).empty()) _printlog("Expected 'in' keyword in 'for' block" + error_location_suffix(*template_str, in_kw_loc.pos));
              
              auto iter_expr_loc = get_location(); // Renamed
              auto iterable_expr = parseExpression(false); // Renamed
              if (!iterable_expr) _printlog("Expected iterable expression in 'for' block" + error_location_suffix(*template_str, iter_expr_loc.pos));

              std::shared_ptr<Expression> condition_for_expr; // Renamed
              if (!consumeToken(if_tok).empty()) { 
                auto for_if_expr_loc = get_location(); // Renamed
                condition_for_expr = parseExpression();
                 if (!condition_for_expr) _printlog("Expected condition for 'if' in 'for' block" + error_location_suffix(*template_str, for_if_expr_loc.pos));
              }
              auto recursive_flag = !consumeToken(recursive_tok).empty(); // Renamed

              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<ForTemplateToken>(current_location, pre_sp, post_sp, std::move(varnames_list), std::move(iterable_expr), std::move(condition_for_expr), recursive_flag));
            } else if (keyword_str == "endfor") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndForTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "generation") { 
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<GenerationTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "endgeneration") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndGenerationTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "set") {
              static std::regex namespaced_var_regex(R"((\w+)\s*\.\s*(\w+))");
              std::string ns_str_val; // Renamed
              std::vector<std::string> var_names_list_val; // Renamed
              std::shared_ptr<Expression> value_expr_val; // Renamed
              auto set_var_expr_loc = get_location(); // Renamed

              if (!(matched_group = consumeTokenGroups(namespaced_var_regex)).empty()) {
                ns_str_val = matched_group[1];
                var_names_list_val.push_back(matched_group[2]);
                auto eq_sign_loc = get_location(); // Renamed
                if (consumeToken("=").empty()) _printlog("Expected '=' after namespaced variable in 'set' block" + error_location_suffix(*template_str, eq_sign_loc.pos));
                value_expr_val = parseExpression();
                if (!value_expr_val) _printlog("Expected value expression in namespaced 'set' block" + error_location_suffix(*template_str, get_location().pos));
              } else {
                var_names_list_val = parseVarNames();
                if (!consumeToken("=").empty()) { 
                  value_expr_val = parseExpression();
                  if (!value_expr_val) _printlog("Expected value expression in 'set' block" + error_location_suffix(*template_str, get_location().pos));
                }
              }
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<SetTemplateToken>(current_location, pre_sp, post_sp, ns_str_val, var_names_list_val, std::move(value_expr_val)));
            } else if (keyword_str == "endset") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndSetTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "macro") {
              auto macro_name_expr_loc = get_location(); // Renamed
              auto macroname_expr = parseIdentifier(); // Renamed
              if (!macroname_expr) _printlog("Expected macro name in 'macro' block" + error_location_suffix(*template_str, macro_name_expr_loc.pos));
              auto params_list = parseParameters(); // Renamed
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<MacroTemplateToken>(current_location, pre_sp, post_sp, std::move(macroname_expr), std::move(params_list)));
            } else if (keyword_str == "endmacro") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndMacroTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "filter") {
              auto filter_call_expr_loc = get_location(); // Renamed
              auto filter_call_expr = parseExpression(); // Renamed
              if (!filter_call_expr) _printlog("Expected filter expression in 'filter' block" + error_location_suffix(*template_str, filter_call_expr_loc.pos));
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<FilterTemplateToken>(current_location, pre_sp, post_sp, std::move(filter_call_expr)));
            } else if (keyword_str == "endfilter") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<EndFilterTemplateToken>(current_location, pre_sp, post_sp));
            } else if (keyword_str == "break" || keyword_str == "continue") {
              auto post_sp = parseBlkClose();
              tokens_list.push_back(std::make_shared<LoopControlTemplateToken>(current_location, pre_sp, post_sp, keyword_str == "break" ? LoopControlType::Break : LoopControlType::Continue));
            } else {
              _printlog("Unexpected or unimplemented block keyword: " + keyword_str + error_location_suffix(*template_str, block_kw_loc.pos));
            }
          } else { 
            std::string search_str(it, end); // Renamed
            if (std::regex_search(search_str, regex_match, non_text_open_regex)) {
                if (regex_match.position(0) == 0 && it != pre_token_it) { 
                     _printlog("Internal tokenizer error: regex search found match at current position but not consumed.");
                     it = end; 
                } else if (regex_match.position(0) > 0) { 
                    text_val = std::string(it, it + regex_match.position(0));
                    it += regex_match.position(0);
                    tokens_list.push_back(std::make_shared<TextTemplateToken>(current_location, SpaceHandling::Keep, SpaceHandling::Keep, text_val));
                } else if (it == pre_token_it) { 
                     _printlog("Tokenizer stuck. Unrecognized sequence at: " + std::string(it, it + std::min((ptrdiff_t)20, std::distance(it, end))) + error_location_suffix(*template_str, get_location().pos));
                     it = end; 
                }
            } else { 
                text_val = std::string(it, end);
                it = end;
                if (!text_val.empty()) { 
                    tokens_list.push_back(std::make_shared<TextTemplateToken>(current_location, SpaceHandling::Keep, SpaceHandling::Keep, text_val));
                }
            }
          }
           if (it == pre_token_it && it != end) { 
                _printlog("Parser stuck at: " + std::string(it, it + std::min((ptrdiff_t)20, std::distance(it,end))) + error_location_suffix(*template_str, get_location().pos) );
                it = end; 
            }
        }
        return tokens_list;
    }

    std::shared_ptr<Expression> parseTemplate( // Changed return type
          const TemplateTokenIterator & all_tokens_begin, // Renamed
          TemplateTokenIterator & current_token_it, // Renamed
          const TemplateTokenIterator & all_tokens_end, // Renamed
          bool fully = false) const {
        std::vector<std::shared_ptr<Expression>> children_exprs; // Renamed
        Location first_child_loc = get_location(); // Default location if no children
        if (current_token_it != all_tokens_end) {
            first_child_loc = (*current_token_it)->location;
        }


        while (current_token_it != all_tokens_end) {
          const auto start_token_for_node_it = current_token_it; // Renamed
          const auto & token = *(current_token_it++);
          if (children_exprs.empty()) { // Update location to the first actual token being processed
             first_child_loc = token->location;
          }


            if (token->type == TemplateToken::Type::If) {
                auto if_token = static_cast<IfTemplateToken*>(token.get());
              std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> cascade_exprs; // Renamed
              cascade_exprs.emplace_back(if_token->condition, parseTemplate(all_tokens_begin, current_token_it, all_tokens_end));

              while (current_token_it != all_tokens_end && (*current_token_it)->type == TemplateToken::Type::Elif) {
                  auto elif_token = static_cast<ElifTemplateToken*>((*(current_token_it++))->get());
                  cascade_exprs.emplace_back(elif_token->condition, parseTemplate(all_tokens_begin, current_token_it, all_tokens_end));
              }

              if (current_token_it != all_tokens_end && (*current_token_it)->type == TemplateToken::Type::Else) {
                current_token_it++; // Consume Else token
                cascade_exprs.emplace_back(nullptr, parseTemplate(all_tokens_begin, current_token_it, all_tokens_end));
              }
              if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndIf) {
                  MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
              }
              children_exprs.emplace_back(std::make_shared<ConditionalBlockExpr>(token->location, std::move(cascade_exprs)));
            } else if (token->type == TemplateToken::Type::For) {
                auto for_token = static_cast<ForTemplateToken*>(token.get());
              auto body_expr = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end); // Renamed
              std::shared_ptr<Expression> else_body_expr_val; // Renamed
              if (current_token_it != all_tokens_end && (*current_token_it)->type == TemplateToken::Type::Else) { 
                current_token_it++; 
                else_body_expr_val = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end); 
              }
              if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndFor) {
                  MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
              }
              children_exprs.emplace_back(std::make_shared<ForExpr>(token->location, for_token->var_names, for_token->iterable, for_token->condition, std::move(body_expr), for_token->recursive, std::move(else_body_expr_val)));
            } else if(token->type == TemplateToken::Type::Generation) { // Treat as passthrough
              auto body_expr = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end);
              if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndGeneration) {
                  MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
              }
              if (body_expr) children_exprs.emplace_back(std::move(body_expr)); 
            } else if(token->type == TemplateToken::Type::Text) {
                auto text_token = static_cast<TextTemplateToken*>(token.get());
              // TODO: Implement space handling based on options and surrounding tokens.
              // For now, just pass the text.
              children_exprs.emplace_back(std::make_shared<TextExpr>(token->location, text_token->text));
            } else if(token->type == TemplateToken::Type::Expression) {
                auto expr_token = static_cast<ExpressionTemplateToken*>(token.get());
                if (expr_token->expr) { // Make sure expr is not null
                     children_exprs.emplace_back(expr_token->expr); // Use the expression directly
                }
            } else if(token->type == TemplateToken::Type::Set) {
                auto set_token = static_cast<SetTemplateToken*>(token.get());
                if (set_token->value) { 
                  children_exprs.emplace_back(std::make_shared<SetVarExpr>(token->location, set_token->ns, set_token->var_names, set_token->value));
                } else { 
                  auto value_body_expr = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end); // Renamed
                  if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndSet) {
                      MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
                  }
                  if (!set_token->ns.empty()) _printlog("Namespaced set not supported for block set" + error_location_suffix(*template_str, token->location.pos));
                  if (set_token->var_names.size() != 1) _printlog("Multiple variable assignment not supported for block set" + error_location_suffix(*template_str, token->location.pos));
                  children_exprs.emplace_back(std::make_shared<SetBlockExpr>(token->location, set_token->var_names[0], std::move(value_body_expr)));
                }
            } else if(token->type == TemplateToken::Type::Macro) {
                auto macro_token = static_cast<MacroTemplateToken*>(token.get());
              auto body_expr = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end);
              if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndMacro) {
                  MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
              }
              children_exprs.emplace_back(std::make_shared<MacroDeclExpr>(token->location, macro_token->name, macro_token->params, std::move(body_expr)));
            } else if(token->type == TemplateToken::Type::Filter) {
                auto filter_token = static_cast<FilterTemplateToken*>(token.get());
                auto body_expr = parseTemplate(all_tokens_begin, current_token_it, all_tokens_end);
                if (current_token_it == all_tokens_end || (*(current_token_it++))->type != TemplateToken::Type::EndFilter) {
                    MNN_ERROR("%s\n", unterminated(**start_token_for_node_it).c_str());
                }
                children_exprs.emplace_back(std::make_shared<FilterBlockExpr>(token->location, filter_token->filter_expr, std::move(body_expr)));
            } else if(token->type == TemplateToken::Type::Comment) {
                children_exprs.emplace_back(std::make_shared<CommentExpr>(token->location, static_cast<CommentTemplateToken*>(token.get())->text));
            } else if(token->type == TemplateToken::Type::Break || token->type == TemplateToken::Type::Continue) {
                auto ctrl_token = static_cast<LoopControlTemplateToken*>(token.get());
                children_exprs.emplace_back(std::make_shared<LoopControlExpr>(token->location, ctrl_token->control_type));
            } else { 
                bool stop_parsing_current_level = false; // Renamed
                switch (token->type) {
                    case TemplateToken::Type::EndSet:
                    case TemplateToken::Type::EndFor:
                    case TemplateToken::Type::EndMacro:
                    case TemplateToken::Type::EndFilter:
                    case TemplateToken::Type::EndIf:
                    case TemplateToken::Type::Else:
                    case TemplateToken::Type::Elif:
                    case TemplateToken::Type::EndGeneration:
                        current_token_it--; 
                        stop_parsing_current_level = true;
                        break;
                    default:
                        MNN_ERROR("%s\n", unexpected(**(current_token_it-1)).c_str());
                }
                if (stop_parsing_current_level) {
                    break; 
                }
          }
        }
        if (fully && current_token_it != all_tokens_end) { 
            MNN_ERROR("%s\n", unexpected(**current_token_it).c_str());
        }

        if (children_exprs.empty()) {
          return std::make_shared<TextExpr>(first_child_loc, std::string()); 
        } else if (children_exprs.size() == 1) {
          return std::move(children_exprs[0]); 
        } else {
          return std::make_shared<SequenceExpr>(children_exprs[0]->location, std::move(children_exprs));
        }
    }


public:

    static std::shared_ptr<Expression> parse(const std::string& template_str_in, const Options & options_in) { // Changed return type
        Parser parser_instance(std::make_shared<std::string>(normalize_newlines(template_str_in)), options_in); // Renamed
        auto tokens_list = parser_instance.tokenize(); // Renamed
        TemplateTokenIterator token_begin_it = tokens_list.begin(); // Renamed
        auto current_token_processing_it = token_begin_it; // Renamed
        TemplateTokenIterator token_end_it = tokens_list.end(); // Renamed
        return parser_instance.parseTemplate(token_begin_it, current_token_processing_it, token_end_it, /* fully= */ true);
    }
};

// Top-level rendering function
static std::string render_template(const std::string& template_str, const Options& options, const std::shared_ptr<Context>& context) {
    auto root_expression = Parser::parse(template_str, options);
    if (!root_expression) {
        // Parser::parse logs errors internally if it can't form a tree
        // or if the template is empty resulting in a null TextExpr initially.
        // _printlog("Template parsing resulted in a null root expression.");
        return ""; // Return empty string for null root expression
    }
    std::ostringstream out_stream;
    root_expression->render(out_stream, context);
    return out_stream.str();
}


static Value simple_function(const std::string & fn_name, const std::vector<std::string> & params, const std::function<Value(const std::shared_ptr<Context> &, Value & args)> & fn) {
  std::map<std::string, size_t> named_positions;
  for (size_t i = 0, n = params.size(); i < n; i++) named_positions[params[i]] = i;

  return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) -> Value {
    auto args_obj = Value::object(); 
    std::vector<bool> provided_args(params.size(), false);
    for (size_t i = 0, n = args.args.size(); i < n; i++) {
      auto & arg_val = args.args[i]; // Renamed
      if (i < params.size()) {
        args_obj.set(params[i], arg_val);
        provided_args[i] = true;
      } else {
        _printlog("Too many positional params for " + fn_name);
      }
    }
      for (auto & iter_pair : args.kwargs) { // Renamed
          auto& name_str = iter_pair.first; // Renamed
          auto& value_item = iter_pair.second; // Renamed
      auto named_pos_it = named_positions.find(name_str);
      if (named_pos_it == named_positions.end()) {
        _printlog("Unknown argument " + name_str + " for function " + fn_name);
      } else { 
        provided_args[named_pos_it->second] = true;
        args_obj.set(name_str, value_item);
      }
    }
    return fn(context, args_obj);
  });
}

inline std::shared_ptr<Context> Context::builtins() {
  auto globals_obj = Value::object(); // Renamed

  globals_obj.set("tojson", simple_function("tojson", { "value", "indent" }, [](const std::shared_ptr<Context> &, Value & args) {
    return Value(args.at(Value("value")).dump(args.get(Value("indent"), Value(-1LL)).get<int64_t>(), /* to_json= */ true));
  }));
  globals_obj.set("items", simple_function("items", { "object" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto items_arr = Value::array();
    if (args.contains(Value("object"))) {
      auto & obj_val = args.at(Value("object"));
      if (obj_val.is_string()) { 
          rapidjson::Document doc;
          doc.Parse(obj_val.get<std::string>().c_str());
          if (doc.IsObject()) {
            for (auto& kv_pair : doc.GetObject()) { // Renamed
                items_arr.push_back(Value::array({Value(kv_pair.name.GetString()), Value(kv_pair.value)}));
            }
          } else { _printlog("'items' filter on string: string is not a valid JSON object.");}
      } else if (obj_val.is_object()) {
        for (auto & key_val_item : obj_val.keys()) { // Renamed
          items_arr.push_back(Value::array({key_val_item, obj_val.at(key_val_item)}));
        }
      } else if (!obj_val.is_null()) {
          _printlog("'items' filter expects an object or JSON string, got: " + obj_val.dump());
      }
    }
    return items_arr;
  }));
  globals_obj.set("last", simple_function("last", { "items" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto items_val = args.at(Value("items"));
    if (!items_val.is_array() && !items_val.is_string()) _printlog("'last' filter expects a sequence (list or string).");
    if (items_val.empty()) return Value(); 
    if (items_val.is_string()) {
        std::string s_val = items_val.get<std::string>(); // Renamed
        return Value(std::string(1,s_val.back()));
    }
    return items_val.at(items_val.size() - 1);
  }));
  globals_obj.set("trim", simple_function("trim", { "text" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto & text_val = args.at(Value("text"));
    return text_val.is_null() ? text_val : Value(strip(text_val.to_str()));
  }));
  auto char_transform_function = [](const std::string & name, int(*fn_ptr)(int)) { // Renamed
    return simple_function(name, { "text" }, [=](const std::shared_ptr<Context> &, Value & args) {
      auto text_val = args.at(Value("text"));
      if (text_val.is_null()) return text_val;
      std::string res_str;
      auto str_to_transform = text_val.to_str();
      std::transform(str_to_transform.begin(), str_to_transform.end(), std::back_inserter(res_str), fn_ptr);
      return Value(res_str);
    });
  };
  globals_obj.set("lower", char_transform_function("lower", ::tolower));
  globals_obj.set("upper", char_transform_function("upper", ::toupper));
  globals_obj.set("default", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    if (args.args.size() < 1 || args.args.size() > 2 ) {
         _printlog("default filter expects 1 or 2 positional arguments."); return Value();
    }
    auto & value_to_check = args.args[0];
    Value default_return_val = (args.args.size() == 2) ? args.args[1] : Value(""); 

    bool check_boolean_emptiness = false;
    if (args.has_named("boolean")) {
        check_boolean_emptiness = args.get_named("boolean").to_bool();
    }
    
    if (value_to_check.is_null()) return default_return_val;
    if (check_boolean_emptiness) { // Jinja's boolean=True logic
        if (value_to_check.is_string() && value_to_check.get<std::string>().empty()) return default_return_val;
        if (value_to_check.is_array() && value_to_check.empty()) return default_return_val;
        if (value_to_check.is_object() && value_to_check.empty()) return default_return_val;
        // For other types, if not null, they are considered "true" in this context
        // unless explicitly false (like a boolean False or number 0 if we extend this logic)
        if (!value_to_check.to_bool()) return default_return_val; // General truthiness check
    }
    return value_to_check; 
  }));
  auto escape_fn = simple_function("escape", { "text" }, [](const std::shared_ptr<Context> &, Value & args) {
    return Value(html_escape(args.at(Value("text")).to_str()));
  });
  globals_obj.set("e", escape_fn);
  globals_obj.set("escape", escape_fn);
  globals_obj.set("joiner", simple_function("joiner", { "sep" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto sep_str = args.get(Value("sep"), Value("")).to_str();
    auto first_call_flag = std::make_shared<bool>(true); // Renamed
    return Value::callable([sep_str, first_call_flag](const std::shared_ptr<Context> &, ArgumentsValue &) -> Value {
      if (*first_call_flag) {
        *first_call_flag = false;
        return Value("");
      }
      return Value(sep_str);
    });
  }));
  globals_obj.set("count", simple_function("count", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items_val = args.at(Value("items"));
      return Value((int64_t) items_val.size());
  }));
  globals_obj.set("dictsort", simple_function("dictsort", { "value" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto & dict_val = args.at(Value("value"));
    if (!dict_val.is_object()) { _printlog("dictsort expects a dictionary/object."); return Value::array(); }
    auto keys_list = dict_val.keys(); 
    std::sort(keys_list.begin(), keys_list.end()); 
    auto result_arr = Value::array();
    for (auto & key_item : keys_list) {
      result_arr.push_back(Value::array({key_item, dict_val.at(key_item)}));
    }
    return result_arr;
  }));
  globals_obj.set("join", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    if (args.args.empty()) { _printlog("join filter expects at least one argument (the list/sequence)."); return Value("");}
    Value& items_to_join = args.args[0];
    if (!items_to_join.is_iterable()) { _printlog("join filter's first argument must be iterable."); return Value(""); }

    std::string delimiter_str = (args.args.size() > 1) ? args.args[1].to_str() : ""; // Renamed
    
    std::shared_ptr<std::string> attribute_key_str; // Renamed
    if(args.has_named("attribute")) {
        attribute_key_str = std::make_shared<std::string>(args.get_named("attribute").to_str());
    }

    std::ostringstream oss_join; // Renamed
    bool first_item_flag = true; // Renamed
    items_to_join.for_each([&](Value& item_val){ // Renamed
        if (!first_item_flag) {
            oss_join << delimiter_str;
        }
        first_item_flag = false;
        if (attribute_key_str) {
            if (item_val.is_object() && item_val.contains(*attribute_key_str)) {
                oss_join << item_val.at(Value(*attribute_key_str)).to_str();
            } 
        } else {
            oss_join << item_val.to_str();
        }
    });
    return Value(oss_join.str());
  }));
  globals_obj.set("namespace", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    auto ns_obj = Value::object();
    if (!args.args.empty()) _printlog("namespace() does not take positional arguments.");
    
    for (auto & kw_pair : args.kwargs) {
          ns_obj.set(kw_pair.first, kw_pair.second);
      }
    return ns_obj;
  }));
  auto equalto_fn = simple_function("equalto", { "expected", "actual" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at(Value("actual")) == args.at(Value("expected"));
  });
  globals_obj.set("equalto", equalto_fn);
  globals_obj.set("==", equalto_fn); 
  globals_obj.set("length", simple_function("length", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items_val = args.at(Value("items"));
      return Value((int64_t) items_val.size());
  }));
  globals_obj.set("safe", simple_function("safe", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at(Value("value")).to_str(); 
  }));
  globals_obj.set("string", simple_function("string", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at(Value("value")).to_str();
  }));
  globals_obj.set("int", simple_function("int", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at(Value("value")).to_int();
  }));
  globals_obj.set("list", simple_function("list", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items_val = args.at(Value("items"));
      if (!items_val.is_iterable()) { _printlog("'list' filter expects an iterable."); return Value::array(); }
      if (items_val.is_array()) return items_val; 

      auto result_arr = Value::array();
      items_val.for_each([&](Value& item_val){ result_arr.push_back(item_val); });
      return result_arr;
  }));
  globals_obj.set("unique", simple_function("unique", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items_val = args.at(Value("items"));
      if (!items_val.is_array()) { _printlog("'unique' filter expects an array."); return Value::array(); }
      
      std::vector<Value> unique_items_vec; 
      std::unordered_set<Value> seen_set; 

      for (size_t i = 0; i < items_val.size(); i++) {
        auto current_item_val = items_val.at(i); // Renamed
        if (seen_set.find(current_item_val) == seen_set.end()) {
          seen_set.insert(current_item_val);
          unique_items_vec.push_back(current_item_val);
        }
      }
      return Value::array(unique_items_vec);
  }));
  auto make_filter_callable = [](const Value & filter_fn_val, Value & extra_filter_args_val) -> Value::CallableType {
    return [filter_fn_val, extra_filter_args_val](const std::shared_ptr<Context> & current_context, ArgumentsValue & call_args_for_item) {
      ArgumentsValue actual_call_args;
      actual_call_args.args.push_back(call_args_for_item.args[0]); 
      for(size_t i=0; i < extra_filter_args_val.size(); ++i) {
          actual_call_args.args.push_back(extra_filter_args_val.at(i));
      }
      actual_call_args.kwargs = call_args_for_item.kwargs; 
      return filter_fn_val.call(current_context, actual_call_args);
    };
  };
  auto select_or_reject_logic = [make_filter_callable](bool is_select_op) {
    return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
      if (args.args.size() < 1) { _printlog((is_select_op ? "select" : "reject") + std::string(" filter expects at least 1 argument (the list).")); return Value::array(); }
      Value& items_list = args.args[0];
      if (!items_list.is_iterable()) { _printlog("First argument to " + (is_select_op ? "select" : "reject") + " must be iterable."); return Value::array(); }

      Value filter_fn_val;
      Value extra_filter_args_val = Value::array(); 

      if (args.args.size() >= 2) {
          filter_fn_val = args.args[1]; 
          if (filter_fn_val.is_string()) { 
              filter_fn_val = context->get(filter_fn_val.get<std::string>());
          }
          for (size_t i = 2; i < args.args.size(); ++i) { 
              extra_filter_args_val.push_back(args.args[i]);
          }
      } else {
          filter_fn_val = Value::callable([](const std::shared_ptr<Context>&, ArgumentsValue& item_args){
              return item_args.args[0].to_bool();
          });
      }

      if (!filter_fn_val.is_callable()) { _printlog("Filter for " + (is_select_op ? "select" : "reject") + " is not callable."); return Value::array(); }
      
      auto filter_call_fn = make_filter_callable(filter_fn_val, extra_filter_args_val); // Renamed
      
      auto result_arr = Value::array();
      items_list.for_each([&](Value& item_val){ // Renamed
          ArgumentsValue item_arg_wrapper; 
          item_arg_wrapper.args.push_back(item_val);
          
          Value filter_result_val = filter_call_fn(context, item_arg_wrapper); // Renamed
          if (filter_result_val.to_bool() == is_select_op) {
              result_arr.push_back(item_val);
          }
      });
      return result_arr;
    });
  };
  globals_obj.set("select", select_or_reject_logic(/* is_select_op= */ true));
  globals_obj.set("reject", select_or_reject_logic(/* is_select_op= */ false));

  globals_obj.set("map", Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
    if (args.args.empty()) { _printlog("map filter expects at least one argument (the list)."); return Value::array(); }
    Value& items_list = args.args[0];
    if (!items_list.is_iterable()) { _printlog("First argument to map must be iterable."); return Value::array(); }

    Value result_arr = Value::array();

    if (args.has_named("attribute")) {
        std::string attr_key_str = args.get_named("attribute").to_str(); // Renamed
        Value default_val_for_attr; // Renamed
        bool has_default_for_attr = false;
        if (args.has_named("default")) {
            default_val_for_attr = args.get_named("default");
            has_default_for_attr = true;
        }
        items_list.for_each([&](Value& item_val){ // Renamed
            if(item_val.is_object() && item_val.contains(attr_key_str)) {
                result_arr.push_back(item_val.at(Value(attr_key_str)));
            } else if (has_default_for_attr) {
                result_arr.push_back(default_val_for_attr);
            } else {
                result_arr.push_back(Value());
            }
        });
    } else if (args.args.size() >= 2) { 
        Value filter_fn_val = args.args[1];
        if (filter_fn_val.is_string()) {
            filter_fn_val = context->get(filter_fn_val.get<std::string>());
        }
        if (!filter_fn_val.is_callable()) { _printlog("Filter for map is not callable."); return Value::array(); }
        
        Value extra_filter_args_val = Value::array();
        for (size_t i = 2; i < args.args.size(); ++i) {
            extra_filter_args_val.push_back(args.args[i]);
        }
        auto map_call_fn = make_filter_callable(filter_fn_val, extra_filter_args_val); // Renamed

        items_list.for_each([&](Value& item_val){ // Renamed
            ArgumentsValue item_arg_wrapper;
            item_arg_wrapper.args.push_back(item_val);
            result_arr.push_back(map_call_fn(context, item_arg_wrapper));
        });
    } else {
         _printlog("Invalid arguments for map filter.");
         return Value::array(); 
    }
    return result_arr;
  }));
  globals_obj.set("indent", simple_function("indent", { "text", "width", "first" }, [](const std::shared_ptr<Context> &, Value & args) {
    std::string text_to_indent = args.at(Value("text")).to_str();
    int width_val = args.get(Value("width"), Value(4LL)).get<int64_t>(); 
    bool indent_first_line_flag = args.get(Value("first"), Value(false)).get<bool>(); // Renamed

    std::string indent_prefix_str(width_val, ' '); // Renamed
    std::ostringstream oss_indent; // Renamed
    std::istringstream iss_lines(text_to_indent); // Renamed
    std::string line_str; // Renamed
    bool is_first_line_in_stream = true;

    while (std::getline(iss_lines, line_str)) {
        if (!is_first_line_in_stream) {
            oss_indent << "\n";
        }
        if (is_first_line_in_stream && indent_first_line_flag) {
            oss_indent << indent_prefix_str;
        } else if (!is_first_line_in_stream && !line_str.empty()) { // Don't indent empty lines unless first
             oss_indent << indent_prefix_str;
        }
        oss_indent << line_str;
        is_first_line_in_stream = false;
    }
    if (!text_to_indent.empty() && text_to_indent.back() == '\n' && text_to_indent.length() > 1) { 
        oss_indent << "\n";
    }
    return Value(oss_indent.str());
  }));
  auto select_or_reject_attr_logic = [make_filter_callable](bool is_select_op) {
    return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
        if (args.args.size() < 2) { _printlog((is_select_op ? "selectattr" : "rejectattr") + std::string(" expects at least 2 arguments.")); return Value::array(); }
        Value& items_list = args.args[0];
        if (!items_list.is_iterable()) { _printlog("First argument to " + (is_select_op ? "selectattr" : "rejectattr") + " must be iterable."); return Value::array(); }
        std::string attr_name_str = args.args[1].to_str();

        Value test_fn_val;
        Value extra_test_args_val = Value::array();

        if (args.args.size() >= 3) { 
            test_fn_val = args.args[2];
            if (test_fn_val.is_string()) {
                test_fn_val = context->get(test_fn_val.get<std::string>());
            }
            for (size_t i = 3; i < args.args.size(); ++i) {
                extra_test_args_val.push_back(args.args[i]);
            }
        } else { 
            test_fn_val = Value::callable([](const std::shared_ptr<Context>&, ArgumentsValue& item_args){
                return item_args.args[0].to_bool(); 
            });
        }

        if (!test_fn_val.is_callable()) { _printlog("Test for " + (is_select_op ? "selectattr" : "rejectattr") + " is not callable."); return Value::array(); }
        auto test_call_fn = make_filter_callable(test_fn_val, extra_test_args_val); // Renamed

        Value result_arr = Value::array();
        items_list.for_each([&](Value& item_val){ // Renamed
            Value attr_val_item; // Renamed
            if (item_val.is_object() && item_val.contains(attr_name_str)) {
                attr_val_item = item_val.at(Value(attr_name_str));
            } 
            ArgumentsValue attr_val_wrapper;
            attr_val_wrapper.args.push_back(attr_val_item);
            
            if (test_call_fn(context, attr_val_wrapper).to_bool() == is_select_op) {
                result_arr.push_back(item_val);
            }
        });
        return result_arr;
    });
  };
  globals_obj.set("selectattr", select_or_reject_attr_logic(/* is_select_op= */ true));
  globals_obj.set("rejectattr", select_or_reject_attr_logic(/* is_select_op= */ false));

  globals_obj.set("range", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    int64_t start_r = 0, stop_r = 0, step_r = 1; // Renamed
    if (args.args.empty() || args.args.size() > 3) {
        _printlog("range() takes 1 to 3 arguments."); return Value::array();
    }
    if (args.args.size() == 1) {
        stop_r = args.args[0].to_int();
    } else { 
        start_r = args.args[0].to_int();
        stop_r = args.args[1].to_int();
        if (args.args.size() == 3) {
            step_r = args.args[2].to_int();
        }
    }
    if (step_r == 0) { _printlog("range() step argument cannot be zero."); return Value::array(); }

    auto result_arr = Value::array();
    if (step_r > 0) {
        for (int64_t i = start_r; i < stop_r; i += step_r) {
            result_arr.push_back(Value(i));
        }
    } else { 
        for (int64_t i = start_r; i > stop_r; i += step_r) {
            result_arr.push_back(Value(i));
        }
    }
    return result_arr;
  }));

  return std::make_shared<Context>(std::move(globals_obj));
}

inline std::shared_ptr<Context> Context::make(Value && values, const std::shared_ptr<Context> & parent) {
  return std::make_shared<Context>(values.is_null() ? Value::object() : std::move(values), parent);
}

}  // namespace minja

#ifndef JSON_H__
#define JSON_H__

#include <map>
#include <vector>
#include <type_traits>

enum JSONType {
  EMPTY,
  STRING,
  BOOLEAN,
  NUMERIC,
  OBJECT,
  LIST
};

class JSON {
  std::map<String, JSON*> mapping;
  std::vector<JSON*> list;
  String raw_content;
  byte type;
  static int count;
  int id;

public:
  JSON();
  JSON(const JSON& other);
  template<typename T>
  JSON(const T& input);
  template<typename T>
  JSON& resetContent(const T& input);
  JSON(JSON&& other)
  noexcept;
  JSON& operator=(const JSON& other);
  JSON& operator=(JSON&& other) noexcept;
  ~JSON();
  void clear();
  template<typename T>
  JSON& operator[](const T& index);
  String listToStr() const;
  String mapToStr() const;
  String toString(const bool partOfSomeObject = false) const;
  int32_t toInt();
  float toFloat();
  double toDouble();
  std::pair<int, String> skipOverhead(int& index);
  std::pair<int, String> skipEnd(int& index);
  std::pair<int, String> skip(int& index, bool enclosed = false);
  std::pair<int, String> skipString(int& index, bool enclosed = false);
  std::pair<int, String> skipObject(int& index);
  bool endSymbol(char ch);
  char complement(char ch);
  bool isObject(String& x);
  bool isArray(String& x);
  void push_back(const JSON& element);
  void push_front(const JSON& element);
  std::vector<JSON*>::iterator begin();
  std::vector<JSON*>::iterator end();
  JSON& front();
  JSON& back();
  bool operator!=(const JSON& other) const;
  bool operator!();
  operator bool();
  bool operator==(const JSON& other) const;
  int findIndex(std::function<bool(JSON&)> predicate);
  JSON& find(std::function<bool(JSON&)> predicate);
  void findAndReplace(std::function<bool(JSON&)> predicate, const JSON& newValue);
  JSON map(std::function<JSON(const JSON&)> predicate);
  void forEach(std::function<void(JSON&)> predicate);
  JSON& remove(std::function<bool(JSON&)> predicate);
  JSON& remove(int);
  JSON& remove(std::function<bool(const String&, JSON&)> predicate);
  void update(std::function<bool(const String&, JSON&)> predicate, const JSON& newValue);
  void update(std::function<bool(JSON&)> predicate, const JSON& newValue);
  template<typename T>
  bool contains(const T&);
  void sort(std::function<bool(JSON*, JSON*)> predicate) {
    if (type == JSONType::LIST) {
      std::sort(list.begin(), list.end(), predicate);
    }
  }
  static bool isJSON(const String&);
  int size() {
    switch (type) {
      case JSONType::OBJECT: return mapping.size();
      case JSONType::LIST: return list.size();
    }
    return raw_content.length();
  }
  JSON pop();
  static JSON nullJSON;
};
JSON JSON::nullJSON;


int JSON::count = 0;

JSON::JSON()
  : type(JSONType::OBJECT) {
  id = count;
  count++;
}

JSON::JSON(const JSON& other)
  : type(other.type), raw_content(other.raw_content) {
  id = count;
  count++;

  if (other.type == JSONType::OBJECT) {
    for (const auto& entry : other.mapping) {
      mapping[entry.first] = new JSON(*entry.second);
    }
  } else if (other.type == JSONType::LIST) {
    for (JSON* item : other.list) {
      list.push_back(new JSON(*item));
    }
  } else {
    this->raw_content = other.raw_content;
  }
}

template<typename T>
JSON::JSON(const T& input) {
  resetContent(input);
}
template<typename T>
JSON& JSON::resetContent(const T& input) {
  id = count;
  count++;
  raw_content = String(input);
  raw_content.trim();
  this->clear();
  if (isObject(raw_content)) {
    this->type = JSONType::OBJECT;
    int index = 1;
    while (index < raw_content.length()) {
      auto [i, overhead] = skipOverhead(index);
      auto [j, key] = skip(index);
      key.replace("\"", "");

      skipOverhead(index);
      auto [l, value] = skip(index);

      if (key.length()) {
        auto valueType = mapping[key] = new JSON(value);
      }
      skipOverhead(index);
      skipEnd(index);
    }
  } else if (isArray(raw_content)) {
    this->type = JSONType::LIST;
    int index = 1;
    while (index < raw_content.length()) {
      auto [i, overhead] = skipOverhead(index);
      auto [j, value] = skip(index);
      if (value.length()) {
        list.push_back(new JSON(value));
      }
      skipOverhead(index);
      auto [n, ends] = skipEnd(index);
    }
    raw_content = "";
  } else {
    if constexpr (std::is_same<T, bool>::value) {
      raw_content = input ? "true" : "false";
      this->type = JSONType::BOOLEAN;
    } else if constexpr (std::is_arithmetic<T>::value) {
      this->type = JSONType::NUMERIC;
    } else if constexpr (std::is_floating_point<T>::value) {
      this->type = JSONType::NUMERIC;
    } else if constexpr (std::is_same<T, String>::value) {
      this->type = JSONType::STRING;
    } else {
      this->type = JSONType::STRING;
    }
  }
  return *this;
}

JSON::JSON(JSON&& other) noexcept
  : mapping(std::move(other.mapping)), list(std::move(other.list)),
    raw_content(std::move(other.raw_content)), type(other.type) {
  id = count;
  count++;
  other.type = JSONType::STRING;
}

bool JSON::operator!() {
  return *this == nullJSON;
}

JSON::operator bool() {
  return *this != nullJSON;
}

JSON& JSON::operator=(const JSON& other) {
  id = count;
  count++;
  if (this != &other) {
    clear();
    type = other.type;
    if (type == JSONType::OBJECT) {
      for (const auto& entry : other.mapping) {
        mapping[entry.first] = new JSON(*entry.second);
      }
    } else if (type == JSONType::LIST) {
      for (JSON* item : other.list) {
        list.push_back(new JSON(*item));
      }
    } else {
      raw_content = other.toString();
    }
  }
  return *this;
}

JSON& JSON::operator=(JSON&& other) noexcept {
  id = count;
  count++;
  if (this != &other) {
    clear();
    type = other.type;
    switch (type) {
      case JSONType::OBJECT:
        {
          mapping = std::move(other.mapping);
        }
        break;
      case JSONType::LIST:
        {
          list = std::move(other.list);
        }
        break;
      default:
        {
          raw_content = other.raw_content;
        }
        break;
    }
  }
  return *this;
}

JSON::~JSON() {
  clear();
}

void JSON::clear() {
  switch (type) {
    case JSONType::OBJECT:
      {
        for (auto& entry : mapping) {
          delete entry.second;
        }
        mapping.clear();
      }
      break;

    case JSONType::LIST:
      {
        for (JSON* item : list) {
          delete item;
        }
        list.clear();
      }
  }
}

template<typename T>
JSON& JSON::operator[](const T& index) {
  if (type == JSONType::OBJECT) {
    auto it = mapping.find(String(index));
    if (it == mapping.end()) {
      // If key doesn't exist, create a new JSON object with default value
      JSON* newJSON = new JSON();
      mapping[String(index)] = newJSON;
      return *newJSON;
    }
    return *mapping[String(index)];
  } else if (type == JSONType::LIST) {
    int idx = String(index).toInt();
    if (idx >= 0 && idx < list.size()) {
      return *list[idx];
    }
  }
  return *this;
}

String JSON::listToStr() const {
  String data = "[";
  int index = 0;
  for (auto valuePtr : list) {
    if (index++ > 0) {
      data += ',';
    }
    data += valuePtr->toString(true);
  }
  data += "]";
  return data;
}

String JSON::mapToStr() const {
  String data = "{";
  int index = 0;
  for (auto& entry : mapping) {
    auto [key, valuePtr] = entry;
    if (index++) {
      data += ',';
    }
    data += String('"') + key + '"' + ':' + valuePtr->toString(true);
  }
  data += "}";
  return data;
}

String JSON::toString(const bool partOfSomeObject) const {
  switch (type) {
    case JSONType::OBJECT: return mapToStr();
    case JSONType::LIST: return listToStr();
    case JSONType::STRING:
      {
        if (partOfSomeObject) {
          return String('"') + raw_content + '"';
        }
        return raw_content;
      }
    default: return raw_content;
  }
}

int32_t JSON::toInt() {
  return raw_content.toInt();
}

float JSON::toFloat() {
  return raw_content.toFloat();
}

double JSON::toDouble() {
  return raw_content.toDouble();
}

std::pair<int, String> JSON::skipOverhead(int& index) {
  String response;
  while (index < raw_content.length()) {
    switch (raw_content[index]) {
      case ':':
      case ' ':
      case '\t':
      case ',':
      case '\n':;
        break;
      default: return std::make_pair(index, response);
    }
    response += raw_content[index];
    index++;
  }
  return std::make_pair(index, response);
}

std::pair<int, String> JSON::skipEnd(int& index) {
  String response;
  while (index < raw_content.length()) {
    switch (raw_content[index]) {
      case ']':
      case '}':
      case ' ':
      case '\t':
      case '\n':;
        break;
      default: return std::make_pair(index, response);
    }
    response += raw_content[index];
    index++;
  }
  return std::make_pair(index, response);
}

std::pair<int, String> JSON::skip(int& index, bool enclosed) {
  switch (this->raw_content[index]) {
    case '[':
    case '{':
      return this->skipObject(index);
    default:
      return this->skipString(index, enclosed);
  }
  return std::make_pair(false, "");
}

std::pair<int, String> JSON::skipString(int& index, bool enclosed) {
  String response = "";
  char quote = raw_content[index];
  bool escaped = false;
  if (quote != '"') {
    while (!endSymbol(raw_content[index])) {
      response += raw_content[index++];
    }
    return std::make_pair(index, response);
  }
  response += quote;
  index++;
  while (index < raw_content.length()) {
    char ch = raw_content[index];
    index++;
    response += ch;
    if (ch == '\\') {
      response.remove(response.length() - 1, 1);
      escaped = !escaped;  // Toggle escape mode
    } else if (ch == quote && !escaped) {
      break;  // End of string reached
    } else {
      escaped = false;  // Reset escape mode
    }
  }
  if (!enclosed) {
    if (response[0] == '"') {
      response.remove(0, 1);
    }

    if (response[response.length() - 1] == '"') {
      response.remove(response.length() - 1, 1);
    }
  }
  return std::make_pair(index, response);
}

std::pair<int, String> JSON::skipObject(int& index) {
  int stack = 0;
  char start = raw_content[index];
  char end = complement(start);
  String response = "";
  bool quotedText = false;
  while (index < raw_content.length()) {
    char ch = raw_content[index];

    if (ch == '"') {
      quotedText = true;
      auto [i, value] = skipString(index, true);
      index = i;
      response += value;
      continue;
    }
    response += ch;
    if (ch == start) {
      stack++;
    } else if (ch == end) {
      stack--;
    }
    if (!stack) {
      break;
    }
    index++;
  }
  return std::make_pair(index, response);
}

bool JSON::endSymbol(char ch) {
  return ch == ',' || ch == '}' || ch == ']';
}

char JSON::complement(char ch) {
  switch (ch) {
    case '{': return '}';
    case '[': return ']';
    case '(': return ')';
    case '"': return '"';
    case '<': return '>';
  }
  return '\0';
}

bool JSON::isObject(String& x) {
  if (!x.length()) {
    return false;
  }
  return x[0] == '{';// && x[x.length() - 1] == '}';
}

bool JSON::isArray(String& x) {
  if (!x.length()) {
    return false;
  }
  return x[0] == '[';// && x[x.length() - 1] == ']';
}

void JSON::push_back(const JSON& element) {
  if (type == JSONType::LIST) {
    list.push_back(new JSON(element));
  }
}

void JSON::push_front(const JSON& element) {
  if (type == JSONType::LIST) {
    list.insert(list.begin(), new JSON(element));
  }
}

JSON JSON::pop() {
  JSON* back = list[list.size()-1];
  list.pop_back();
  return *back;
}

std::vector<JSON*>::iterator JSON::begin() {
  if (type == JSONType::LIST && !list.empty()) {
    return list.begin();
  }
  return std::vector<JSON*>::iterator(nullptr);
}

std::vector<JSON*>::iterator JSON::end() {
  if (type == JSONType::LIST && !list.empty()) {
    return list.end();
  }
  return std::vector<JSON*>::iterator(nullptr);
}

JSON& JSON::front() {
  if (type == JSONType::LIST && !list.empty()) {
    return *list.front();
  }
  return *this;
}

JSON& JSON::back() {
  if (type == JSONType::LIST) {
    if (!list.empty()) {
      return *list.back();
    }
  }
  return *new JSON("");
}

bool JSON::operator!=(const JSON& other) const {
  return !(*this == other);
}

bool JSON::operator==(const JSON& other) const {
  if (raw_content != other.raw_content) {
    return false;
  }

  if (list.size() != other.list.size()) {
    return false;
  }

  for (size_t i = 0; i < list.size(); ++i) {
    if (*list[i] != *other.list[i]) {
      return false;
    }
  }

  if (mapping.size() != other.mapping.size()) {
    return false;
  }

  for (const auto& entry : mapping) {
    auto it = other.mapping.find(entry.first);
    if (it == other.mapping.end() || *entry.second != *it->second) {
      return false;
    }
  }

  return true;
}

int JSON::findIndex(std::function<bool(JSON&)> predicate) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (predicate(*list[i])) {
      return i;
    }
  }
  return -1;  // Not found
}

JSON& JSON::find(std::function<bool(JSON&)> predicate) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (predicate(*list[i])) {
      return *list[i];
    }
  }
  return nullJSON;  // Not found, return reference to self
}

void JSON::findAndReplace(std::function<bool(JSON&)> predicate, const JSON& newValue) {
  if (type == JSONType::LIST) {
    for (size_t i = 0; i < list.size(); ++i) {
      if (predicate(*list[i])) {
        delete list[i];
        list[i] = new JSON(newValue);
        return;
      }
    }
  }
}

JSON JSON::map(std::function<JSON(const JSON&)> predicate) {
  JSON result("[]");
  for (size_t i = 0; i < list.size(); ++i) {
    result.push_back(predicate(*list[i]));
  }
  return result;
}

void JSON::forEach(std::function<void(JSON&)> predicate) {
  for (size_t i = 0; i < list.size(); ++i) {
    predicate(*list[i]);
  }
}

JSON& JSON::remove(std::function<bool(JSON&)> predicate) {
  if (type == JSONType::LIST) {
    auto it = list.begin();
    while (it != list.end()) {
      if (predicate(**it)) {
        delete *it;
        it = list.erase(it);
      } else {
        ++it;
      }
    }
  } else if (type == JSONType::OBJECT) {
    auto it = mapping.begin();
    while (it != mapping.end()) {
      if (predicate(*it->second)) {
        delete it->second;
        it = mapping.erase(it);
      } else {
        ++it;
      }
    }
  }
  return *this;
}

JSON& JSON::remove(std::function<bool(const String&, JSON&)> predicate) {
  if (type == JSONType::OBJECT) {
    auto it = mapping.begin();
    while (it != mapping.end()) {
      if (predicate(it->first, *it->second)) {
        delete it->second;
        it = mapping.erase(it);
      } else {
        ++it;
      }
    }
  }
  return *this;
}

JSON& JSON::remove(int index) {
  if (type == JSONType::LIST) {
    int i = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (i == index) {
        list.erase(it);
        break;
      }
      ++i;
    }
  }
  return *this;
}

void JSON::update(std::function<bool(const String&, JSON&)> predicate, const JSON& newValue) {
  if (type == JSONType::OBJECT) {
    for (auto& entry : mapping) {
      if (predicate(entry.first, *entry.second)) {
        delete entry.second;
        entry.second = new JSON(newValue);
      }
    }
  }
}

void JSON::update(std::function<bool(JSON&)> predicate, const JSON& newValue) {
  if (type == JSONType::LIST) {
    for (auto& item : list) {
      if (predicate(*item)) {
        delete item;
        item = new JSON(newValue);
      }
    }
  } else if (type == JSONType::OBJECT) {
    for (auto& entry : mapping) {
      if (predicate(*entry.second)) {
        delete entry.second;
        entry.second = new JSON(newValue);
      }
    }
  }
}

template<typename T>
bool JSON::contains(const T& key) {
  if (type == JSONType::OBJECT) {
    for (auto [_key, _] : mapping) {
      if (_key == key) {
        return true;
      }
    }
  } else if (type == JSONType::LIST) {
    for (auto item : list) {
      if (*item == key) {
        return true;
      }
    }
  }
  return false;
}

bool JSON::isJSON(const String& str) {
  char start = str[0];
  char end = str[str.length() - 1];
  return start == '[' && end == ']' || start == '{' && end == '}';
}
#endif
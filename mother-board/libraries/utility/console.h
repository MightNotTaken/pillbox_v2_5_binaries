#ifndef CONSOLE_H__
#define CONSOLE_H__

#include <Arduino.h>
#include <iterator> // for std::begin and std::end

template<typename T>
struct has_show_method {
  private:
    typedef char YesType[1];
    typedef char NoType[2];

    template<typename C>
    static YesType& test(decltype(&C::toString));

    template<typename>
    static NoType& test(...);

  public:
    static const bool value = sizeof(test<T>(nullptr)) == sizeof(YesType);
};

template<typename T>
struct has_begin_end {
  private:
    template<typename U>
    static auto test(U* p) -> decltype(std::begin(*p), std::end(*p), std::true_type {});

    template<typename>
    static std::false_type test(...);

  public:
    static const bool value = decltype(test<T>(nullptr))::value;
};

class Console {
  public:
    template <typename... Args>
    void log(Args... args) {
      logHelper(args...);
    }

  private:
    char separator = ' ';
    template <typename T, typename... Args>
    void logHelper(T first, Args... rest) {
      if constexpr (has_show_method<T>::value) {
        logHelper(first.toString());
      } else if constexpr (std::is_same<String, T>::value) {
        // Check if the argument is a String and try to parse it as JSON
        if (isJson(first)) {
          prettifyJson(first);
        } else {
          Serial.print(first);
        }
      } else if constexpr (has_begin_end<T>::value) {
        
          for (auto it = first.begin(); it != first.end(); ++it) {
            logHelper(*it);
          }
      } else {
        Serial.print(first);
      }
      Serial.print(separator);
      logHelper(rest...);
    }

    void logHelper() {
      Serial.println();
    }

    bool isJson(const String& str) {
      // Very basic JSON detection logic
      return str.startsWith("{") && str.endsWith("}") || str.startsWith("[") && str.endsWith("]");
    }

    void prettifyJson(const String& jsonString) {
      int indentLevel = 0;
      bool inQuote = false;

      for (size_t i = 0; i < jsonString.length(); i++) {
        char c = jsonString[i];

        if (c == '"') {
          inQuote = !inQuote;
        }

        if (!inQuote) {
          if (c == '{' || c == '[') {
            Serial.print(c);
            Serial.println();
            indentLevel += 2;
            printIndent(indentLevel);
          } else if (c == '}' || c == ']') {
            indentLevel -= 2;
            Serial.println();
            printIndent(indentLevel);
            Serial.print(c);
          } else if (c == ',') {
            Serial.println(c);
            printIndent(indentLevel);
          } else if (c == ':') {
            Serial.print(c);
            Serial.print(' ');
          } else {
            Serial.print(c);
          }
        } else {
          Serial.print(c);
        }
      }
    }

    void printIndent(int indentLevel) {
      for (int i = 0; i < indentLevel; i++) {
        Serial.print(' ');
      }
    }
} console;

#endif

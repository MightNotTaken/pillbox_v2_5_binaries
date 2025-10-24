#ifndef DATABASE_H__
#define DATABASE_H__

#define FORMAT_SPIFFS_IF_FAILED   true

#include <FS.h>
#include <Wire.h>

#ifndef ESP32
#define FILE_WRITE   "w"
#define FILE_READ    "r"
#define FILE_APPEND  "a"
#else
#include <SPIFFS.h>
#endif

namespace Database {
    fs::FS& fs = SPIFFS;
    String _payload;
    bool begin() {
      Wire.begin();
      return SPIFFS.begin(
        #ifdef ESP32
        FORMAT_SPIFFS_IF_FAILED
        #endif
      );
    }

  void listDir(const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
  }
    bool format() {
      return SPIFFS.format();
    }

    bool createFile(String name) {
      File file = fs.open(name, FILE_WRITE);
      if (file) {
        file.close();
        return true;
      } else {
        return false;
      }
    }

    template <typename T>
    bool writeFile(String name, T data) {
      File file = fs.open(name, FILE_WRITE);
      if (!file) {
        return false;
      }
      if (!file.print(String(data))) {
        file.close();
        return false;
      }
      file.close();
      return true;     
    }

    bool readFile(String name) {
      Database::_payload = "";
      File file = fs.open(name, FILE_READ);
      if (!file) {
        return false;
      }
      int index = 0;
      while (index++ < file.size()) {
        Database::_payload += (char)file.read();
      }
      return true;
    }

    bool hasFile(String name) {
      return fs.exists(name);
    }

    bool renameFile(String original, String newer) {
      if (Database::hasFile(original)) {
        fs.rename(original, newer);
        return true;
      }
      return false;
    }
    
    bool removeFile(String filename) {
      if (Database::hasFile(filename)) {
        fs.remove(filename);
        return true;
      }
      return false;
    }

    String& payload() {
      return Database::_payload;
    }
};



#endif

#ifndef LOGS_H__
#define LOGS_H__
#include <JSON.h>
#include <mac.h>
#include <definitions.h>
#include "mqtt.h"
#include <database.h>
#include "power-source.h"
#include "./time.h"
namespace Logs {
  void loop();
  String indexFile = "/logs/index.txt";
  String vileStatus = "000000";
  String _env = "prod";
  std::function<void()> nextLogAvailableCallback;
  String getEnv();
  String vilePosition;
  IntervalReference tracker;
  class Log {
    public:
      byte vile;
      uint32_t medicineID;
      uint8_t currentBattery;
      String time;
      TimeoutReference fallbackTracker;
      String stringified;
      int attempt;
      Log() {}
      Log(byte vile, uint32_t medicineID)
        : vile(vile), medicineID(medicineID) {
        this->time = getDateTimeStamp();
        this->attempt = 3;
        this->currentBattery = battery.getPercentage();
      }


      String toString() {
        JSON json;
        json["pb_device_unique_id"] = MAC::getMac();
        json["box_comp_id"] = this->vile;
        json["id"] = this->medicineID;
        json["battery_status"] = this->currentBattery;
        json["taken_time"] = this->time;
        json["device_type"] = "pillbox";
        json["cbs"] = vileStatus;
        json["env"] = getEnv();
        return json.toString();
      };

      String toArray(bool live = false, TimeoutReference fallbackID = NULL_REFERENCE) {
        String data = "[";
        data += '"' + MAC::getMac() + '"' + ',';
        data += this->vile;data += ',';
        data += this->medicineID;data += ',';
        data += this->currentBattery;data += ',';
        String tm = this->time.substring(this->time.indexOf(' ') + 1);
        data += '"' + tm + '"' + ',';
        data += '"' + vileStatus + '"' + ',';
        data += '"' + getEnv() + '"' + ',';
        data += live ? "true," : "false,";
        data += '"' + vilePosition + '"';data += ']';
        console.log(data);
        return data;
      };

      void show() {
        console.log(*this);
      }

      String minimal() {
        return String(this->vile) + ',' + this->medicineID + ',' + this->currentBattery + '_' + this->time;
      }

      void construct(String input) {
        sscanf(input.c_str(), "%d,%ld,%d", &this->vile, &this->medicineID, &this->currentBattery);
        this->time = Time::standardize(input.substring(input.indexOf("_") + 1, input.length()));
      }
  };

  
  void setVilePosition(String position) {
    vilePosition = position;
  }

  String getEnv() {
    if (Database::readFile("/env.cnf")) {
      return Database::payload();
    }
    return "prod";
  }

  String getLogFile(int index) {
    return String("/logs/") + index + ".txt";
  }

  void add(byte vile, uint32_t medicineID) {
    Logs::Log log(vile + 1, medicineID);
    console.log("adding new log", log);
    if (wifiMQTT.connected()) {
      wifiMQTT.emit("update-log-v3", log.toArray(true, log.fallbackTracker), true, LogDataType::LOG_ARRAY);
    } else {
      if (!Database::hasFile(indexFile)) {
        Database::writeFile(indexFile, String(1));
        Database::writeFile(getLogFile(1), log.minimal());
      } else {
        if (Database::readFile(indexFile)) {
          int index = Database::payload().toInt() + 1;
          if (index == 30) {
            index = 0;
          }
          Database::writeFile(indexFile, String(index));
          Database::writeFile(Logs::getLogFile(index), log.minimal());
        }
      }
    }
  }

  bool nextLog(std::function<void(String)> callback) {
    if (Database::readFile(indexFile)) {
      int index = Database::payload().toInt();
      if (!index) {
        return false;
      }
      if (Database::readFile(Logs::getLogFile(index))) {
        Logs::Log log;
        console.log("log", index);
        log.construct(Database::payload());
        if (index > 1) {
          Database::writeFile(indexFile, index - 1);
        } else {
          Database::removeFile(indexFile);
        }
        Database::removeFile(Logs::getLogFile(index));
        invoke(callback, log.toArray());
        return true;
      }
    }
    return false;
  }

  void onNextLogAvailable(std::function<void()> callback) {
    Logs::nextLogAvailableCallback = callback;
  }

  void begin(std::function<void()> callback = nullptr) {
    Logs::nextLogAvailableCallback = callback;
    clearInterval(Logs::tracker);
    Logs::tracker = setInterval([]() {
      if (wifiMQTT.connected()) {
        bool moreLogs = Logs::nextLog([](String log) {
          if (wifiMQTT.connected()) {
            console.log("updating log");
            wifiMQTT.emit("update-log-v3", log);
          }
        });
        if (moreLogs) {
          invoke(Logs::nextLogAvailableCallback);
        }
      }
    }, SECONDS(.5));
  }
};
#endif
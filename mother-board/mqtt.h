#ifndef MQTT_BRIDGE_H__
#define MQTT_BRIDGE_H__
#include <serial-interpretter.h>
#include <Arduino.h>
#include <definitions.h>
#include <event-handler.h>
#include <mac.h>
#include <JSON.h>
#include <interval.h>

enum LogDataType {
  LOG_OBJECT,
  LOG_ARRAY,
  LOG_STRING
};

struct EventDataPair {
  String event;
  String data;
  String currentWord;
  bool listening;
  bool currentListeningTo;
  enum {
    EVENT = 0,
    DATA = 1
  };

  void flush() {
    event = "";
    data = "";
    currentWord = "";
    listening = false;
  }


  EventDataPair* update(char ch) {
    if (listening) {
      if (ch == '\n') {
        int eventStart = data.indexOf('[');
        int eventEnd = data.indexOf(']');
        if (eventStart > -1 && eventEnd > -1) {
          event = data.substring(eventStart + 1, eventEnd);
          data = data.substring(eventEnd + 2);
        }
        listening = false;
        return this;
      } else {
        data += ch;
      }
    } else {
      if (ch == ' ') {
        if (currentWord == "arrived") {
          listening = true;
          return nullptr;
        }
        currentWord = "";
      } else {
        currentWord += ch;
      }
    }
    return nullptr;
  }
};

class MQTTBridge : public EventHandler {
  bool status;
  EventDataPair dataListener;
  JSON creds;
  bool listeningToJunk;
public:
  MQTTBridge() {
    listeningToJunk = false;
  }
  void begin() {
    status = false;
    interCom.on("mqtt-data", [this](String rawData) {
      JSON json(rawData);
      String data = json["data"].toString();
      data.replace("\\n", "\n");
      data.replace("nm-", "\nm-");
      console.log(data);
      this->call(json["event"].toString(), data);
    });
  }

  void listen(const String& event, std::function<void(String)> callback) {
    this->on(event, callback);
  }

  void stopListen(const String& event) {
    EventHandler::unsubscribe(event);
  }
  
  void registerDevice() {
    interCom.emit("mqtt-listen", MAC::getMac());
  }
  IntervalReference emit(const String& event, const String& data, bool insured = false, byte type = LogDataType::LOG_STRING) {
    if (insured) {
      switch (type) {
        case LogDataType::LOG_OBJECT: {
          JSON objData(data);
          objData["__mtcb"] = Interval::nextID();
          return setImmediate([event, objData]() {
            console.log(objData);
            interCom.emit("mqtt-emit", event + SEPERATOR + objData.toString());
          }, 5000, 10);
        } break;
        case LogDataType::LOG_ARRAY: {
          JSON objData(data);
          objData.push_back(Interval::nextID());
          return setImmediate([event, objData]() {
            console.log(objData);
            interCom.emit("mqtt-emit", event + SEPERATOR + objData.toString());
          }, 5000, 10);
        } break;
        case LogDataType::LOG_STRING: {
          return setImmediate([event, data]() {
            console.log(data);
            interCom.emit("mqtt-emit", event + SEPERATOR + data + "|" + Interval::nextID());
          }, 5000, 10);
        } break;
      }
    } else {
      interCom.emit("mqtt-emit", event + SEPERATOR + data);
    }
    return NULL_REFERENCE;
  }
  bool connected() {
    return this->status;
  }
  void setStatus(bool status) {
    this->status = status;
  }
  void on(String event, EventCallback callback) {
    EventHandler::on(event, callback);
  }
  void setCredentials(const JSON& credentials) {
    this->creds = credentials;
  }
  void passCredentials() {
    console.log("passing mqtt credentials");
    interCom.emit("mqtt-clientID", creds["id"].toString());
    interCom.emit("mqtt-server", creds["server"].toString()); 
    interCom.emit("mqtt-port", creds["port"].toInt());
    interCom.emit("mqtt-username", creds["username"].toString());
    interCom.emit("mqtt-password", creds["password"].toString());
  }
} wifiMQTT;
#endif
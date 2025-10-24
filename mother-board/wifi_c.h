#ifndef WIFI_H__
#define WIFI_H__
#include <database.h>
#include <console.h>
#include <string-matcher.h>
#include <event-handler.h>
#include <map>
#include <functional>
#include <my-web-server.h>
#include <async-core.h>
#include "mqtt.h"
#include <definitions.h>
#include "audio-player.h"

#define PROVISIONING_PIN                    21
#define WIFI_INDICATOR_PIN                  1

using namespace AsyncCore;
class Wifi_T : public JSON, public EventHandler {
  bool provisioning;
  JSON matchedCredentials;
  JSON attemptedCredentials;
  bool hotspot;
  JSON connectedCreds;
  String minimizedString;
  bool _connected;
  IntervalReference backupWifiLoop;
  TimeoutReference wrongPasswordRemover;
  String history[3];
  JSON credsToPass;
public:
  TimeoutReference disconnectionTracker;
  TimeoutReference wifiAudioTracker;
  OutputGPIO* indicator;
  bool alreadyConnected;
  Wifi_T();
  void update(std::function<bool(JSON&)>, const JSON&);
  void begin(const JSON& = "[]");
  void updateLastConnected(JSON&);
  void save();
  void initializeCredentials(const JSON&);
  void resetCurrentCredentials();
  void turnOnHotspot(String, String);
  void turnOffHotspot();
  void configureRoutes();
  void setupProvisioningKey();
  void activateFallbackMode();
  bool fallbackMode(bool = false);
  bool isProvisioning();
  void sleep();
  void setConnected(const JSON&);
  JSON getConnected();
  void connectToWifi(std::function<void()>, std::function<void()>);
  void connectNow(JSON, std::function<void()>, std::function<void()>);
  bool hotspotActive();
  void resetCredentials();
  void setMinimal(String);
  String getMinimal();
  void setStatus(bool);
  bool isConnected();
  int inFactoryResetMode(bool = false);
  void passNextCred();
  void startConnectionRoutine();
  void stopBackupWifiLoop() {
    clearInterval(backupWifiLoop);
  }
  void parseIfCurrent(String);
  void setAttemptedCredentials(String apName, String apPass) {
    this->attemptedCredentials["apName"] = apName;
    this->attemptedCredentials["apPass"] = apPass;
  }
  JSON getAttemptedCredentials() {
    return this->attemptedCredentials;
  }
  void updateHistory(String);
  void handleWrongPassword(String);
  void cancelWrongPasswordRemove();
};


void Wifi_T::activateFallbackMode() {
  Database::writeFile("/fallback.conf", "1");
}

bool Wifi_T::fallbackMode(bool override) {
  if (Database::hasFile("/fallback.conf")) {
    if (override) {
      Database::removeFile("/fallback.conf");
    }
    return true;
  }
  return false;
}

void Wifi_T::setMinimal(String data) {
  minimizedString = data;
}

String Wifi_T::getMinimal() {
  return minimizedString;
}

int Wifi_T::inFactoryResetMode(bool overwrite) {
  if (Database::hasFile("/reset.conf")) {
    if (overwrite) {
      Database::removeFile("/reset.conf");
    }
    return 1;
  }
  return 0;
}

void Wifi_T::initializeCredentials(const JSON& wifiList = "[]") {
  this->matchedCredentials = "[]";  //wifiList;
}

void Wifi_T::resetCurrentCredentials() {
  this->matchedCredentials = nullJSON;
}

Wifi_T::Wifi_T() {
  hotspot = false;
  _connected = false;
  alreadyConnected = false;
  disconnectionTracker = NULL_REFERENCE;
  resetCurrentCredentials();
}

void Wifi_T::passNextCred() {
  for (int i=credsToPass.size()-1; i>=0; i--) {
    if (!credsToPass[i]["passed"].toInt()) {
      auto wifi = credsToPass[i];
      console.log("passing", wifi);
      interCom.emit("wifi-cred", wifi["apName"].toString() + SEPERATOR + wifi["apPass"].toString());
      break;      
    }
  }
}

void Wifi_T::parseIfCurrent(String buffer) {
  int startIndex = buffer.indexOf("connecting to ");
  if (startIndex > -1) {
    int endIndex = buffer.indexOf(" using password ");
    if (endIndex > -1) {
      String ssid = buffer.substring(startIndex + 14, endIndex);
      String password = buffer.substring(endIndex + 16);
      this->setAttemptedCredentials(ssid, password);
    }
  }
}

void Wifi_T::handleWrongPassword(String ssid) {
  int32_t difference = MINUTES(2) - millis() - SECONDS(7);
  wrongPasswordRemover = setTimeout([ssid, this]() {
    AudioPlayer::play("/wrong-password.mp3");
    console.log("removing", ssid, "from wifi list");
    this->remove([ssid](JSON& credential) {
      return credential["apName"].toString() == ssid;
    });
    console.log(*this);
    this->save();
    setTimeout([this]() {
      this->call("provision", String(0));
    }, 9000);
  }, difference > 0 ? difference: SECONDS(10));
}

void Wifi_T::updateHistory(String buffer) {
  buffer.trim();
  history[0] = history[1];
  history[1] = history[2];
  history[2] = buffer;
  if (history[2] == "scanning") {
    if (history[0].length()) {
      for (int i=0; i<credsToPass[i].size(); i++) {
        String ssid = credsToPass[i]["apName"].toString();
        String password = credsToPass[i]["apPass"].toString();
        if (ssid == history[0] && password == history[1]) {
          credsToPass[i]["passed"] = 1;
          console.log(credsToPass[i]);
          this->call("scan-catch", ssid + '|' + password);
        }
      }
    } else {
      this->call("scan-catch");
    }

    history[0] = "";
    history[1] = "";
    history[2] = "";
  }
}
void Wifi_T::startConnectionRoutine() {
  interCom.onJunkData([this](char ch) {
    static String buffer = "";
    if (ch == '\n') {
      console.log("buffer", buffer);
      this->updateHistory(buffer);
      this->parseIfCurrent(buffer);
      if (
        buffer.indexOf("Interface 0 IP address :") > -1
        ||
        buffer.indexOf("Interface 1 IP address :") > -1
      ) {
        // this->call("connected");
      } else if (
        buffer.indexOf("dissconn reason code: 15") > -1
      ) {
        this->call("wrong-password");
      }
      buffer = "";
    } else {
      buffer += ch;
    }
  });
}


void Wifi_T::begin(const JSON& defaultContent) {
  provisioning = false;
  interCom.on("wifi-connected", [this](String message) {
    this->call("connected", message);
  });
  interCom.on("wifi-disconnected", [this](String message) {
    this->call("disconnect");
    wifiMQTT.setStatus(false);
  });
  indicator = new OutputGPIO(WIFI_INDICATOR_PIN, GPIOType::WS_2811);
  setupProvisioningKey();
  if (!Database::hasFile("/wifi.json")) {
    Database::writeFile("/wifi.json", defaultContent.toString());
  }
  if (Database::readFile("/wifi.json")) {
    console.log("Credentials loaded from database", Database::payload());
    if (JSON::isJSON(Database::payload())) {
      this->resetContent(Database::payload());
    } else {
      Database::writeFile("/wifi.json", defaultContent.toString());
      this->resetContent(defaultContent.toString());
    }
  } else {
    this->resetContent(defaultContent.toString());
  }
  console.log("wifi credential size", this->size());
  console.log("fallbackMode", this->fallbackMode() ? "active" : "not active");
  bool restartAfterWrongWifiAttempt = Database::hasFile("/wrong.conf");
  
  if ((this->size() || this->fallbackMode()) && !restartAfterWrongWifiAttempt) {
    this->startConnectionRoutine();
    int index = 0;
    this->call("disconnect");
    // interCom.emit("wifi-cred", String("Pillbox") + SEPERATOR + "12345678");
    credsToPass.resetContent("[]");
    JSON back;
    if (this->fallbackMode(true)) {
      credsToPass.resetContent("[]");
      back["apName"] = "Pillbox";
      back["apPass"] = "12345678";
    } else {
      credsToPass = this->toString();
      back = this->back();
    }

    while (credsToPass.size() < 3) {
      credsToPass.push_back(back);
    }

    for (int i=0; i<3; i++) {
      credsToPass[i]["passed"] = int(0);
    }
    this->setStatus(false);
    for (int i=0; i<3; i++) {
      String ssid = credsToPass[i]["apName"].toString();
      String password = credsToPass[i]["apPass"].toString();
      console.log("passing credential", ssid, password);
      interCom.emit("battery");
      interCom.emit("wifi-cred", ssid + SEPERATOR + password);
    }
    this->on("scan-catch", [this](String ssid_password) {
      if (ssid_password.length()) {
        String ssid = ssid_password.substring(0, ssid_password.indexOf('|'));
        String password = ssid_password.substring(ssid_password.indexOf('|') + 1);
        console.log(ssid, password, "catched");
      } else {
      }
      this->passNextCred();
    });
    this->call("disconnect");
    this->indicator->setColor(Color::RED);
  } else {
    console.log("calling provision");
    
    setTimeout([this]() {
      this->call("provision", String(2500));
    }, 1000);
  }
}

void Wifi_T::update(std::function<bool(JSON&)> predicate, const JSON& newValue) {
  JSON::update(predicate, newValue);
  this->save();
}
void Wifi_T::save() {
  if (this->toString() == "{}") {
    this->resetContent("[]");
  }
  if (!Database::writeFile("/wifi.json", this->toString())) {
    console.log("unable to save to database");
  }
}

void Wifi_T::updateLastConnected(JSON& current) {
  if (!current["apPass"].toString().length()) {
    return;
  }
  this->remove([&current](JSON wifi) {
    return current["apName"].toString() == wifi["apName"].toString();
  });
  while (this->size() > 4) {
    this->remove(0);
  }
  this->push_back(current);
  this->save();
}

void Wifi_T::resetCredentials() {
  Database::writeFile("/wifi.json", "[]");
}

void Wifi_T::turnOnHotspot(String ssid, String password) {
  hotspot = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), password.c_str());
}

void Wifi_T::turnOffHotspot() {
  WiFi.disconnect(true, true);
  hotspot = false;
}

bool Wifi_T::hotspotActive() {
  return hotspot;
}



void Wifi_T::configureRoutes() {
  webServer.get("/api/wifi/add", [this](Request* request) {
    JSON response;
    int code = 200;
    String apName = request->getParam("apName")->value();
    String apPass = request->getParam("apPass")->value();
    Database::writeFile("/config-just-done.conf", "");
    Database::writeFile("/reset.conf", "");
    console.log(apName, apPass);  
    if (!apName.length()) {
      response["message"] = "Invalid apName";
      response["type"] = "error";
      code = 400;
      return std::make_pair(code, response.toString());
    }
    if (apPass.length() && apPass.length() < 8) {
      response["message"] = "Password length should be atleast 8";
      response["type"] = "error";
      code = 400;
      return std::make_pair(code, response.toString());
    }
    JSON wifi;
    wifi["apName"] = apName;
    wifi["apPass"] = apPass;
    this->remove([apName](JSON& current) {
      return current["apName"] == apName;
    });
    this->push_front(wifi);
    if (this->size() > 3) {
      this->pop();
    }
    this->save();
    response["message"] = "connection okay";
    response["type"] = "success";
    response["data"] = this->toString();
    this->call("config-done");
    
    return std::make_pair(code, response.toString());
  });
  webServer.get("/api/wifi/id", [this](Request* request) {
    JSON response;
    int code = 200;
    console.log("/delete called");
    if (request->hasParam("id")) {
      int id = request->getParam("id")->value().toInt();
      this->remove(id);
      this->save();
      code = 200;
      response["message"] = "okay";
      response["type"] = "success";
    } else {
      code = 400;
      response["message"] = "id expected";
      response["type"] = "error";
    }
    return std::make_pair(code, response.toString());
  });
  webServer.get("/api/wifi/config", [this](Request* request) {
    JSON response = this->toString();
    console.log("response", response);
    while (response.size() < 3) {
      JSON dummy;
      dummy["apName"] = "";
      dummy["apPass"] = "";
      response.push_back(dummy);
    }
    return std::make_pair(200, response.toString());
  });
  webServer.post("/api/wifi/config", [this](Request* request) {
    JSON response = this->toString();
    while (response.size() < 3) {
      JSON dummy;
      dummy["apName"] = "";
      dummy["apPass"] = "";
      response.push_back(dummy);
    }
    return std::make_pair(200, response.toString());
  });
}


bool Wifi_T::isProvisioning() {
  return this->provisioning;
}

void Wifi_T::setupProvisioningKey() {
  console.log("setting up provisioning key");
  InputGPIO* provisioningKey = new InputGPIO(PROVISIONING_PIN);
  provisioningKey->onStateLow([this, provisioningKey]() {
    if (!this->provisioning) {
      setTimeout([this, provisioningKey]() {
        if (provisioningKey->getCurrentState() == LOW) {
          this->call("stop-provision");
          this->activateFallbackMode();
          AudioPlayer::play("/fallback.mp3");
          setTimeout([]() {
            ESP.restart();
          }, 3000);
        }
      }, 7000);
      console.log("calling provision");
      this->call("provision",  String(4000));
      this->provisioning = true;
    } else {
      // this->call("hotspot-close");
      // setTimeout([]() {
      //   ESP.restart();
      // }, 3000);
    }
  });
  GPIOConfig::registerInput(provisioningKey);
}

void Wifi_T::connectNow(JSON matches, std::function<void()> success, std::function<void()> fail) {
  if (!matches.size()) {
    invoke(fail);
    return;
  }
  auto current = matches[0];
  matches.remove(0);
  String ssid = current["apName"].toString();
  String password = current["apPass"].toString();
  WiFi.begin(ssid.c_str(), password.c_str());
  static IntervalReference intervalTracker;
  static int count = 0;
  intervalTracker = setInterval([intervalTracker, this, matches, count, success, fail]() {
    count ++;
    console.log("count", count);
    if (WiFi.status() == WL_CONNECTED) {
      invoke(success);
      clearInterval(intervalTracker);
      return;
    }
    if (count == 15) {
      connectNow(matches, success, fail);
      clearInterval(intervalTracker);
    }
  }, 1000);
  
}

void Wifi_T::connectToWifi(std::function<void()> success, std::function<void()> fail) {
  WiFi.scanNetworks(true, true);
  static IntervalReference networkTracker;
  clearInterval(networkTracker);
  networkTracker = setInterval([this, networkTracker, success, fail]() {
    int networkFound = WiFi.scanComplete();
    console.log("scanning");
    if (!networkFound || networkFound == WIFI_SCAN_FAILED) {
      invoke(fail);
      console.log("clearing interval");
      clearInterval(networkTracker);
      return;
    }
    JSON matched("[]");
    for (int i = 0; i < networkFound; ++i) {
      String ssid = WiFi.SSID(i);
      auto response = this->find([i](JSON& cred) {
        StringMatcher st(cred["apName"].toString(), WiFi.SSID(i));
        float percentage = st.getPercentage();
        return percentage > 90;
      });
      if (response) {
        console.log("matched", response);
        matched.push_back(response);
      }
    }
    if (matched.size()) {
      connectNow(matched, success, fail);      
      clearInterval(networkTracker);
    }
  }, 500);
}

void Wifi_T::sleep() {
  interCom.emit("wifi-sleep");
}
void Wifi_T::setConnected(const JSON& c) {
  this->connectedCreds = c;
}


bool Wifi_T::isConnected() {
  return this->_connected;
}

void Wifi_T::cancelWrongPasswordRemove() {
  clearTimeout(this->wrongPasswordRemover);
}

void Wifi_T::setStatus(bool connected) {
  this->_connected = connected;
}

JSON Wifi_T::getConnected() {
  return this->connectedCreds;
}

Wifi_T wifi;
#endif
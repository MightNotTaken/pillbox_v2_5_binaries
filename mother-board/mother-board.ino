#include <definitions.h>
#include <serial-interpretter.h>
#include <async-core.h>
#include <console.h>
#include <JSON.h>
#include <output-gpio.h>
#include <gpio-config.h>
#include <alarm.h>
#include "wifi_c.h"
#include "mqtt.h"
#include <mac.h>
#include "time.h"
#include "log.h"
#include "power-source.h"
#include "viles.h"
#include "medicines.h"
#include <database.h>
#include "config.h"
#include <OTA.h>
#include <my-web-server.h>
#include "rtl.h"
#include "audio-player.h"
#include <http.h>
#include "pill-counter.h"
#include "customer-care.h"

using namespace AsyncCore;

#define DEVICE_POWER_KEY           2

void setup() {
  Configuration::Device::powerOn();
  Serial.begin(115200);
  Serial.println("Starting esp");
  GPIOConfig::begin();
  MAC::load();
  Database::begin();
  uint32_t totalMemory = SPIFFS.totalBytes();
  console.log("memory size:", totalMemory, "bytes");
  Database::listDir("/", 0);
  Configuration::begin();
  RTL::listenToReset(19);
  RTL::reset();
  PillCounters::loop();
  interCom.begin();
  AudioPlayer::begin("");
  battery.begin();

  battery.startTracker = setInterval([]() {
    console.log("asking for battery");
    interCom.emit("battery");
  }, 100);

  interCom.on("battery", [](String adc) {
    clearInterval(battery.startTracker);
    console.log("battery", millis(), adc);
    
    int voltage = battery.getVoltage(adc.toInt());
    int percentage = battery.getPercentageFromMV(voltage);

    console.log("battery:", percentage, "%");

    if (percentage < 5 && !battery.isCharging()) {
      console.log("battery critically low");
      AudioPlayer::play("/battery-critically-low.mp3");
      setTimeout([]() {
        AudioPlayer::play("/turn-off.mp3");
        setTimeout([]() {
          Configuration::Device::powerOff();
        }, 3500);
      }, 2000);
    } else {
      if (Database::hasFile("/config-just-done.conf")) {
        Database::removeFile("/config-just-done.conf");
      } else if (wifi.fallbackMode()) {
        console.log("starting in fallback mode");
      } else {
        AudioPlayer::begin("/welcome.mp3");
      }
    }
    battery.listen();
  });

  
  setInterval([]() {
    // console.log(getTimeStamp());
  }, 1000);

  initializeBattery();
  
  interCom.on("ready", [](String message) {
    console.log("intercom established");
    Serial.println(message);

    setTimeout([]() {
      setupMQTT();
      setupWifi();
      registerEvents();
      Time::begin();
      initializeOTAEvents();
      Time::onSync([](String stamp) {
        console.log(stamp);
        clearInterval(Configuration::Device::connectionTracker);
        Logs::begin();
      });
    }, 5000);
  });

  Configuration::Device::powerButton->onStateHigh([]() {
    static bool used = false;
    if (used) {
      return;
    }
    used = true;
    console.log("button released");
    Configuration::Device::powerButton->onStateHigh([]() {});
  });
  AudioPlayer::play("/welcome.mp3");
}

void loop() {
  interCom.loop();
  // AsyncCore::loop();
  // GPIOConfig::listen();
  apCtrl.loop();
}

void registerEvents() {
  Time::listenToUTC();
  Viles::listenStatusChange();
  Medicines::refreshDataListener();
  OTA::listenToUpdates(DEVICE_TYPE);
  Configuration::Device::listenToChanges();
  customerCare.begin();
  wifiMQTT.listen(MAC::getMac() + "/volume", [](String data) {
    int volume = data.toInt();
    AudioPlayer::setVolume(volume);
    console.log("volume set to", AudioPlayer::getVolume());
  });
  wifiMQTT.listen(MAC::getMac() + "/add-wifi", [](String data) {
    JSON credential(data);
    static JSON toSave;
    toSave["apName"] = credential["apName"].toString();
    toSave["apPass"] = credential["apPass"].toString();
    wifi.remove([&toSave](JSON& cred) {
      return cred["apName"].toString() == toSave["apName"].toString();
    });
    wifi.push_back(toSave);
    wifi.save();
    JSON response;
    response["mac"] = MAC::getMac();
    String current = wifi.getAttemptedCredentials()["apName"].toString();
    JSON list("[]");
    for (int i=0; i<wifi.size(); i++) {
      JSON c("[]");
      c.push_back(wifi[i]["apName"].toString());
      c.push_back(wifi[i]["apName"].toString() == current ? 1 : 0);
      list.push_back(c);
    }
    response["wifi"] = list.toString();
    wifiMQTT.emit("wifi-added", response.toString(), true, LogDataType::LOG_OBJECT);
  } );

  wifiMQTT.listen(MAC::getMac() + "/remove-wifi", [](String data) {
    JSON credential(data);
    JSON response;
    static JSON toSave;
    toSave["apName"] = credential["apName"].toString();
    wifi.remove([&toSave](JSON& cred) {
      return cred["apName"].toString() == toSave["apName"].toString();
    });
    wifi.save();
    response["mac"] = MAC::getMac();
    String current = wifi.getAttemptedCredentials()["apName"].toString();
    JSON list("[]");
    for (int i=0; i<wifi.size(); i++) {
      JSON c("[]");
      c.push_back(wifi[i]["apName"].toString());
      c.push_back(wifi[i]["apName"].toString() == current ? 1 : 0);
      list.push_back(c);
    }
    response["wifi"] = list.toString();
    wifiMQTT.emit("wifi-removed", response.toString(), true, LogDataType::LOG_OBJECT);
  });

  wifiMQTT.listen(MAC::getMac() + "/activate-config", [](String data) {
    JSON response;
    response["mac"] = MAC::getMac();
    response["hotspot"]["apName"] = PRODUCT_NAME + "_" + MAC::getMac();
    response["hotspot"]["apPass"] = "12345678";
    IntervalReference itr = wifiMQTT.emit("config-done", response.toString(), true, LogDataType::LOG_OBJECT);
    wifiMQTT.on(String(itr), [](String _) {
      wifi.call("provision");
    });
  });

  wifiMQTT.listen(MAC::getMac() + "/clear-timeout", [](String data) {
    TimeoutReference timeout = data.toInt();
    clearTimeout(timeout);
  });

  wifiMQTT.listen(MAC::getMac() + "/shutdown", [](String audio) {
    AudioPlayer::play("/turn-off.mp3");
    JSON response;
    response["mac"] = MAC::getMac();
    response["online"] = 0;
    response["env"] = Logs::getEnv();
    response["battery"] = battery.getPercentage();
    response["reason"] = "server-turn-off";
    wifiMQTT.emit("shutdown-v2", response.toString());
    setTimeout([]() {
      Configuration::Device::powerOff();
    }, SECONDS(3));
  });

  wifiMQTT.listen(MAC::getMac() + "/clear-interval", [](String data) {
    IntervalReference interval = data.toInt();
    wifiMQTT.call(data);
    clearInterval(interval);
  });
}

void setupWifi() {
  wifi.on("connected", [](String _) {
    clearTimeout(wifi.disconnectionTracker);
    if (wifi.isProvisioning()) {
      return;
    }
    console.log("wifi connected", _);
    wifi.setConnected(JSON(_));
    if (!wifi.isConnected()) {
      wifi.cancelWrongPasswordRemove();
      JSON credentials = wifi.toString();
      JSON list("[]");
      JSON response;
      JSON current = wifi.getAttemptedCredentials();
      JSON updatedCreds("[]");
      updatedCreds.push_back(current);
      for (int i=0; i<credentials.size(); i++) {
        JSON cred("[]");
        cred.push_back(credentials[i]["apName"].toString());
        cred.push_back(credentials[i]["apPass"].toString());
        cred.push_back(credentials[i]["apName"].toString() == current["apName"].toString() ? 1 : 0);
        if (credentials[i]["apName"].toString() != current["apName"].toString()) {
          updatedCreds.push_back(credentials[i]);
        }
        list.push_back(cred.toString());
      }
      response["wifi"] = list;
      response["mac"] = MAC::getMac();
      response["env"] = Logs::getEnv();
      wifi.setMinimal(list.toString());
      wifi.resetContent(updatedCreds.toString());
      console.log("before saving", wifi);
      wifi.save();
    }
    wifi.setStatus(true);
  });
  wifi.on("disconnect", [](String _) {
    if (millis() < 10000) {
      return;
    }
    console.log("will disconnect after 60000 ms");
    clearTimeout(wifi.disconnectionTracker);
    wifi.disconnectionTracker = setTimeout([]() {
      wifi.setStatus(false);
      wifi.alreadyConnected = false;
      if (wifi.hotspotActive()) {
        return;
      }
      wifi.indicator->setColor(Color::RED);
      wifi.indicator->turnOn();
      AudioPlayer::play("/wifi-disconnected.mp3");
    }, 60000);
  });
  wifi.on("wrong-password", [](String _) {
    if (wifi.isProvisioning()) {
      return;
    }
    static bool captured = false;
    if (captured) {
      return;
    }
    captured = true;
    String attempted = wifi.getAttemptedCredentials()["apName"].toString();
    console.log("attempted", attempted);
    wifi.handleWrongPassword(attempted);
  });
  wifi.on("hotspot-active", [](String ms) {
    wifi.indicator->turnOn();
    wifi.indicator->setColor(Color::BLUE);
    AudioPlayer::play("/ready-to-pair.mp3");
  });
  wifi.on("provision", [](String ms) {
    static TimeoutReference itr;

    wifi.indicator->turnOn();
    wifi.indicator->setColor(Color::BLUE);
    
    itr = setTimeout([ms]() {
      console.log("provisioning stating after", ms, "ms");
      interCom.emit("wifi-sleep");
      wifi.turnOnHotspot(PRODUCT_NAME + "_" + MAC::getMac(), "12345678");
      AudioPlayer::registerAudioRoute();
      OTA::activateOTAInConfigMode();
      TabCount::beginRoutes();
      Viles::beginRoutes();
      Configuration::Device::beginRoutes();
      customerCare.beginWebServerRoutes();
      wifi.configureRoutes();
      webServer.begin(DEVICE_TYPE);
      wifi.call("hotspot-active", ms);
    }, ms.toInt());
    
    wifi.on("stop-provision", [&itr](String _) {
      console.log("provisioning stoped");
      clearTimeout(itr);
    });
  });
  wifi.on("config-done", [](String _) {
    AudioPlayer::play("/pair-success.mp3");
    setTimeout([]() {
      ESP.restart();
    }, SECONDS(4));
    Viles::blinkAll();
  });
  wifi.on("shutdown", [](String _) {
    console.log("unregistering wifi events");
    wifi.unsubscribe("connected");
    wifi.unsubscribe("disconnect");
    wifi.unsubscribe("wrong-password");
    wifi.unsubscribe("hotspot-active");
    wifi.unsubscribe("provision");
    wifi.unsubscribe("config-done");
    wifi.indicator->turnOff();
  });
  wifi.begin();
}

void setupMQTT() {
  console.log("setup mqtt");
  wifiMQTT.begin();
  wifiMQTT.passCredentials();
  // wifiMQTT.registerDevice();
  interCom.on("mqtt-connected", [](String message) {
    if (wifi.isProvisioning()) {
      return;
    }
    wifiMQTT.setStatus(true);
    console.log("mqtt connected successfully");
    wifiMQTT.registerDevice();
    clearInterval(Configuration::Device::connectionTracker);
    console.log("emitting data");
    JSON config = Configuration::Device::toString();
    config["wifi"] = wifi.getMinimal();
    config["vp"] = Viles::updateVilePosition(false);
    config["rst"] = wifi.inFactoryResetMode(true);
    config["ch"] = battery.isCharging() ? 1: 0;
    wifiMQTT.emit("connect-v3", config.toString(), true, LogDataType::LOG_OBJECT);
    console.log("handshake established");
    clearInterval(wifi.wifiAudioTracker);
    if (!wifi.alreadyConnected) {
      AudioPlayer::play("/wifi-connected.mp3");
    } else {
      console.log("not playing wifi connected again");
    }
    
    wifi.indicator->setColor(Color::GREEN);
    wifi.indicator->setMaxIntensity(10);
    wifi.indicator->turnOn();
    wifi.alreadyConnected = true;
  });

  interCom.on("shutdown", [](String _) {
    console.log("unregistering mqtt events");
    interCom.unsubscribe("mqtt-connected");
  });
}

void initializeBattery() {
  static TimeoutReference tracker;
  setInterval([]() {
    battery.updatePercentage();
  }, MINUTES(5));
  battery.on("alert", [&tracker](String current) {
    clearTimeout(tracker);
    if (battery.isCharging()) {
      return;
    }
    setTimeout([&tracker, current]() {
      console.log("current:", current.toInt());
      switch (current.toInt()) {
        case 75: AudioPlayer::play("/battery-75.mp3");
          break;
        case 50: AudioPlayer::play("/battery-50.mp3");
          break;
        case 25: AudioPlayer::play("/battery-25.mp3");
          break;
        case 20: AudioPlayer::play("/battery-20.mp3");
          break;
        case 15: AudioPlayer::play("/battery-15.mp3");
          break;
        case 10: AudioPlayer::play("/battery-critically-low.mp3");
          break;
        case 0: {
          console.log("battery low");
          AudioPlayer::play("/battery-critically-low.mp3");
          JSON response;
          response["mac"] = MAC::getMac();
          response["online"] = 0;
          response["env"] = Logs::getEnv();
          response["battery"] = battery.getPercentage(true);
          response["reason"] = "battery-low";
          IntervalReference refVal = wifiMQTT.emit("shutdown-v2", response.toString(), true, LogDataType::LOG_OBJECT);
          setTimeout([]() {
            AudioPlayer::play("/turn-off.mp3");
          }, 2000);
          wifiMQTT.on(String(refVal), [](String _) {
            setTimeout([]() {
              Configuration::Device::powerOff();
            }, SECONDS(2.5));
          });
          setTimeout([]() {
            wifi.indicator->turnOff();
            battery.batteryIndicator->turnOff();
          }, SECONDS(2.5));
          setTimeout([]() {
            Configuration::Device::powerOff();
          }, SECONDS(12.5));
        } return;
      }
      if (current.toInt() <= 25) {
        tracker = setTimeout([]() {
          AudioPlayer::play("/charging.mp3");
        }, SECONDS(3));
      }
    }, SECONDS(10));
  });
  battery.on("shutdown", [](String _) {
    console.log("unregistering battery events");
    battery.unsubscribe("alert");
    interCom.unsubscribe("battery");
    battery.batteryIndicator->turnOff();
  });
}

void initializeOTAEvents() {
  OTA::whileProgramming([](int percentage) {
    battery.batteryIndicator->setColor(random(0x000000, 0xFFFFFF));
    wifi.indicator->setColor(random(0x000000, 0xFFFFFF));
  });
  OTA::onFinished([]() {
    console.log("ota finished");
  });
}
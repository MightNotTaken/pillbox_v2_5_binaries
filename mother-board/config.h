#ifndef CONFIG_H__
#define CONFIG_H__
#include <JSON.h>
#include <mac.h>
#include <output-gpio.h>
#include <gpio-config.h>
#include "wifi.h"
#include <my-web-server.h>
#include "power-source.h"
#include "viles.h"
#include "medicines.h"
#include "log.h"
#include <serial-interpretter.h>
#include <esp_sleep.h>
#include "audio-player.h"

#define PRODUCT_NAME               String("PILLBOX")
#define FIRMWARE_VERSION           String("6.2.3")
#define DEVICE_TYPE                String("pillbox")

#define MOSFET_GPIO                2 

namespace Configuration {
  JSON mqttCredentials() {
    JSON creds;
    creds["server"]   = "3.213.192.148";
    creds["port"]     = 1883;
    creds["username"] = "pillbox_health";
    creds["password"] = "Pill_Ka_Box";
    creds["id"]       = MAC::getMac();
    return creds;
  }
  
  namespace Device {
    IntervalReference connectionTracker;
    String env = "prod";
    OutputGPIO* mosfetSwitch;
    InputGPIO* powerButton;
    InputGPIO* chargeSense;
    String toString(bool onlyNecessary = false) {
      JSON json;
      json["mac"] = MAC::getMac();
      json["version"] = FIRMWARE_VERSION;
      return json.toString();
    }

    void reset() {
      console.log("Device reset");
      Medicines::reset();
      Viles::setStatus("000000");
      wifi.resetCredentials();
      AudioPlayer::play("/device-removed.mp3");
      JSON data;
      data["mac"] = MAC::getMac();
      
      IntervalReference refVal = wifiMQTT.emit("reset-done", data.toString(), true, LogDataType::LOG_OBJECT);
      wifiMQTT.on(String(refVal), [](String data) {
        setTimeout([]() {
          ESP.restart();
        }, 2000);
      });
      setTimeout([]() {
        ESP.restart();
      }, 12000);
    }

    void listenToChanges() {
      wifiMQTT.listen(MAC::getMac() + "/env", [](String env) {
        Device::env = env;
        Logs::_env = env;
        console.log("env received", env);
        Database::writeFile("/env.cnf", env);
      });
      wifiMQTT.listen(MAC::getMac() + "/reset", [](String env) {
        Device::reset();
      });
    }

    void beginRoutes() {
      webServer.get("/reset", [](Request* reques) {
        Medicines::reset();
        Viles::setStatus("000000");
        wifi.resetCredentials();
        AudioPlayer::play("/device-removed.mp3");
        JSON data;
        data["status"] = "okay";
        setTimeout([]() {
          ESP.restart();
        }, 2500);
        return std::make_pair(200, data.toString());
      });
    }

    void powerOn() {
      pinMode(MOSFET_GPIO, OUTPUT);
      digitalWrite(MOSFET_GPIO, HIGH);
    }

    void powerOff() {
      digitalWrite(MOSFET_GPIO, LOW);
    }

    void begin() {
      powerButton = new InputGPIO(15, INPUT_PULLUP);
      chargeSense = new InputGPIO(23, INPUT_PULLDOWN);
      powerButton->onStateLow([]() {
        setTimeout([]() {
          if (powerButton->getCurrentState() == LOW) {
            if (digitalRead(PROVISIONING_PIN) == LOW) {
              Configuration::Device::reset();
              return;
            }
            AudioPlayer::play("/turn-off.mp3");
            JSON response;
            response["mac"] = MAC::getMac();
            response["online"] = 0;
            response["env"] = Logs::getEnv();
            response["battery"] = battery.getPercentage();
            response["reason"] = "manual-turn-off";
            AudioPlayer::disabled = true;
            battery.call("shutdown");
            wifiMQTT.call("shutdown");
            wifi.call("shutdown");
            interCom.call("shutdown");
            IntervalReference refVal = wifiMQTT.emit("shutdown-v2", response.toString(), true, LogDataType::LOG_OBJECT);
            wifiMQTT.on(String(refVal), [](String _) {
              setTimeout([]() {
                Device::powerOff();
              }, SECONDS(2.5));
            });
            setTimeout([]() {
              wifi.indicator->turnOff();
              battery.batteryIndicator->turnOff();
              if (!wifi.isConnected()) {
                Device::powerOff();
              }
            }, SECONDS(2.5));
            setTimeout([]() {
              Device::powerOff();
            }, SECONDS(12.5));
          }
        }, SECONDS(2));
      });
      chargeSense->onStateChange([](bool pinState) {
        static TimeoutReference tr;
        if (pinState == HIGH) { // HIGH FOR PILOT LOT
              battery.startCharging();
            } else {
              battery.stopCharging();
            }
        if (wifi.isConnected()) {
          clearTimeout(tr);
          tr = setTimeout([pinState]() {
            JSON response;
            response["mac"] = MAC::getMac();
            if (pinState == HIGH) { // HIGH FOR PILOT LOT
              response["status"] = 1;
            } else {
              response["status"] = 0;
            }
            console.log("uplinking charging data", response);
            wifiMQTT.emit("charging", response.toString(), true, LogDataType::LOG_OBJECT);
          }, 3000);
        }
      });
      chargeSense->setForceState(LOW); // LOW FOR PILOT LOT
      GPIOConfig::registerInput(powerButton);
      GPIOConfig::registerInput(chargeSense);
      wifiMQTT.setCredentials(Configuration::mqttCredentials());
      if (!Database::hasFile("/env.cnf")) {
        Database::writeFile("/env.cnf", "prod");
      }
      if (Database::readFile("/env.cnf")) {
        Device::env = Database::payload();
        if (!Device::env.length()) {
          Device::env = "prod";
        }
        Logs::_env = Device::env;
      }
    }
  };

  void begin() {
    MAC::load();
    OutputGPIO::begin();
    TabCount::begin();
    Alarms::begin();
    Device::begin();

    Viles::begin([]() {
      console.log("waking by vile");
      // Device::wakeup();  // incase of emergency wake up device
    });
    GPIOConfig::beforeListen([]() {
      Viles::listen();
    });
    GPIOConfig::afterListen([]() {
      // Viles::turnOff();
    });
    
    Medicines::begin([](String data) {
      JSON parameters(data);
      console.log(parameters);
      TimeReference sleep = parameters["sleep"].toInt();
      TimeReference wakeAfter = parameters["wakeAfter"].toInt();
      // Configuration::Device::rescheduleSleep(sleep, wakeAfter);
    });
  }

};
#endif
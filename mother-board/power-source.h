#ifndef POWER_SOURCE_H__
#define POWER_SOURCE_H__

#include <circular-buffer.h>
#include <Wire.h>
#include <definitions.h>
#include <serial-interpretter.h>
#include "audio-player.h"
#include <event-handler.h>
#include "mqtt.h"
#include <battery.h>
#include <alert.h>

enum Colors {
  GREEN_COLOR  = 0x00FF00,
  RED_COLOR    = 0xFF0000,
  YELLOW_COLOR = 0xFFFF00,
  ORANGE_COLOR = 0xFFA500,
  BLUE_COLOR   = 0x0000FF
};


#define BATTERY_INDICATOR_GPIO              0
#define BATTERY_INDICATOR_GPIOType          GPIOType::WS_2811

#define mv_PER_ADC        11.95606F

class PowerSource_T: public EventHandler {
  IntervalReference tracker;
  CircularBuffer* buffer;
  CircularBuffer* ltsBuffer;
  int lastPercentage;
  int lastCalculated;
  Alert* fullCharging;
  Alert* alert95;
  Alert* alert50;
  Alert* alert25;
  Alert* alert20;
  Alert* alert15;
  Alert* alertCritical;
  Alert* alertTurnOff;
  bool charging;
public:

  IntervalReference chargingBlinker;
  IntervalReference startTracker;
  OutputGPIO* batteryIndicator;
  PowerSource_T() {
    charging = false;
    buffer = new CircularBuffer(30);
    ltsBuffer = new CircularBuffer(30);
    lastPercentage = -1;
    lastCalculated = -1;
    fullCharging = new Alert(100, 2, 102, 
    Direction_T::UPWARD);
    alert95 = new Alert(95, 5, 93, Direction_T::DOWNWARD);
    alert50 = new Alert(50, 5, 48, Direction_T::DOWNWARD);
    alert25 = new Alert(25, 5, 23, Direction_T::DOWNWARD);
    alert20 = new Alert(20, 5, 18, Direction_T::DOWNWARD);
    alert15 = new Alert(15, 5, 13, Direction_T::DOWNWARD);
    alertCritical = new Alert(10, 5, 5, Direction_T::DOWNWARD);
    alertTurnOff = new Alert(5, 5, 0, Direction_T::DOWNWARD);
    fullCharging->on("trigger", [this](String event) {
      this->batteryIndicator->stopBlink();
      this->batteryIndicator->turnOn();
    });
    alert95->on("trigger", [this](String event) {
      if (this->isCharging()) {
        this->startCharging();
      }
    });
    alert50->on("trigger", [this](String event) {
      this->call("alert", String(50));
    });
    alert25->on("trigger", [this](String event) {
      this->call("alert", String(25));
    });
    alert20->on("trigger", [this](String event) {
      this->call("alert", String(20));
    });
    alert15->on("trigger", [this](String event) {
      this->call("alert", String(15));
    });
    alertCritical->on("trigger", [this](String event) {
      this->call("alert", String(10));
    });
    alertTurnOff->on("trigger", [this](String event) {
      this->call("alert", String(0));
    });
  }

  void listen() {
    interCom.on("battery", [this](String adc) {
      // console.log("millis", millis());
      int voltage = this->getVoltage(adc.toInt());
      int battery = this->getPercentageFromMV(voltage);
      if (battery > 95 && this->isCharging()) {
        this->batteryIndicator->stopBlink();
        this->batteryIndicator->turnOn();
      }
      // console.log("adc:", adc);
      // console.log("voltage:", voltage);
      this->buffer->push(battery);
      this->ltsBuffer->push(this->buffer->evaluate());
      battery = this->ltsBuffer->evaluate();
      this->updateIndicator(battery);
      alert50->update(battery);
      alert25->update(battery);
      alert20->update(battery);
      alert15->update(battery);
      alert95->update(battery);
      fullCharging->update(battery);
      alertCritical->update(battery);
      alertTurnOff->update(battery);
      Database::writeFile("/battery.conf", String(battery));
    });
    this->loop();
  }

  void startCharging() {
    if (MAC::getMac() == "A0A3B36D6F14" || MAC::getMac() == "48E729905420") {
      return;
    }
    this->charging = true;
    console.log("charging started");
    this->batteryIndicator->blink(SECONDS(1), 0xFFFFF);
  }
  
  void stopCharging() {
    this->charging = false;
    console.log("charging end");
    this->batteryIndicator->stopBlink();
    this->batteryIndicator->turnOn();
  }

  bool isCharging() {
    return this->charging;
  }

  void updatePercentage() {
    if (wifiMQTT.connected()) {
      JSON data;
      data["mac"] = MAC::getMac();
      data["battery"] = this->getPercentage(true);
      data["time"] = getTimeStamp();
      data["online"] = true;
      console.log(data);
      wifiMQTT.emit("battery", data.toString());
    }
  }

  void updateIndicator(int battery) {
    this->batteryIndicator->enable();
    if (battery > 75) {
      this->batteryIndicator->setColor(Colors::GREEN_COLOR);
    } else if (battery > 50) {
      this->batteryIndicator->setColor(Colors::YELLOW_COLOR);
    } else if (battery > 25) {
      this->batteryIndicator->setColor(Colors::ORANGE_COLOR);
    } else {
      this->batteryIndicator->setColor(Colors::RED_COLOR);
    }
    this->batteryIndicator->setIntensity(10);
  }

  void begin() {
    this->batteryIndicator = new OutputGPIO(BATTERY_INDICATOR_GPIO, BATTERY_INDICATOR_GPIOType);
  }

  bool inRange(int input, int start, int end) {
    return input >= start && input <= end;
  }


  int getVoltage(int adc) {
    return adc * mv_PER_ADC;
  }

  int getPercentageFromMV(int voltage_in_mv) {
    return Battery::getPercentage(voltage_in_mv);
  }

  int getPercentage(bool forced = false) {
    float percentage = this->buffer->evaluate();
    if (forced) {
      if (!Database::hasFile("/battery.conf")) {
        Database::writeFile("/battery.conf", String(51));
      }
      Database::readFile("/battery.conf");
      percentage = Database::payload().toInt();
    }
    return percentage;
  }

  void loop() {
    clearInterval(tracker);
    tracker = setInterval([this]() {
      interCom.emit("battery");
    }, SECONDS(5));
  }
} battery;
#endif
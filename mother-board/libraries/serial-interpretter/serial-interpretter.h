#ifndef SERIAL_INTREPRETTER_H__
#define SERIAL_INTREPRETTER_H__
#include <functional>
#include <event-handler.h>
#include <async-core.h>
#include <console.h>
#include <output-gpio.h>
// #define SERIAL_DEBUG_MODE_ACTIVE

#define bridge (&Serial2)
#define RTL_ENABLE      4

using namespace AsyncCore;
class SerialInterpretter: public EventHandler {
    String data;
    IntervalReference connectionTracker;
    OutputGPIO* rtlEnable;
    bool listening;
    bool mqttMessage;
    std::vector<std::function<void(char)>> junkDataCallbacks;
    std::function<void(char)> dataCallback;
    bool paused;
public:
    SerialInterpretter() {
        listening = false;
        mqttMessage = false;
        paused = false;
    }
    
    void begin() {
        paused = false;
        bridge->begin(9600, SERIAL_8N1, 16, 17);
        this->resetDataCallback();
    }

    void setBaudRate(uint32_t baud) {
      console.log("changing baudrate to", baud);
      emit("baud", String(baud));
      delay(100);
      bridge->begin(baud, SERIAL_8N1, 16, 17);
    }

    void flush() {
        data = "";
    }

    void emit(const String& event, const  String& message="") {
        bridge->write('~');
        bridge->print(event);
        bridge->write(SEPERATOR);
        bridge->print(message);
        bridge->write('|');
    }

    void emit(const String& event, int intValue) {
        emit(event, String(intValue));
    }

    void raw(const String& data) {
        bridge->print(data);
    }

    void reset() {
        rtlEnable->turnOff();
        setTimeout([this]() {
            this->rtlEnable->turnOn();
        }, 1000);
    }

    void onJunkData(std::function<void (char)> callback) {
        this->junkDataCallbacks.push_back(callback);
    }

    void onData(std::function<void(char)> callback) {
      this->dataCallback = callback;
    }

    void resetDataCallback() {
      this->dataCallback = [this](char ch) {
        if (ch == '~') {
            this->flush();
            listening = true;
            return;
        }
        if (ch == '|') {
            int index = this->data.indexOf(SEPERATOR);
            if (index > -1) {
                String event = this->data.substring(0, index);
                call(event, this->data.substring(index + 1));
            }
            this->flush();
            listening = false;
            return;
        }
        if (listening) {
            data += ch;
        } else {
            for (auto cb: junkDataCallbacks) {
                invoke(cb, ch);
            }
        }
      };
    }

    void pause() {
      paused = true;
    }

    void loop() {
        while (bridge->available()) {
            char ch = bridge->read();
            if (!paused) {
              this->dataCallback(ch);
            }
        }
    }
} interCom;
#endif
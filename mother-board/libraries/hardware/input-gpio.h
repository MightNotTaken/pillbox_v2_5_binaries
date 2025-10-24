#ifndef INPUT_H__
#define INPUT_H__
#include <functional>
#include <definitions.h>
#include <Arduino.h>
class InputGPIO {
  uint8_t GPIO;
  std::function<void()> stateHighCallback;
  std::function<void()> stateLowCallback;
  std::function<void(bool)> stateChangeCallback;
  bool lastState;
  public:
    InputGPIO(uint8_t GPIO, uint8_t mode = INPUT): GPIO(GPIO) {
      pinMode(GPIO, mode);
      this->lastState = digitalRead(GPIO);
      this->GPIO = GPIO;
    }

    void onStateHigh(std::function<void()> callback) {
      this->stateHighCallback = callback;
    }

    void onStateLow(std::function<void()> callback) {
      this->stateLowCallback = callback;
    }

    void onStateChange(std::function<void(bool)> callback) {
      this->stateChangeCallback = callback;
    }
    
    void setForceState(bool state) {
      this->lastState = state;
    }

    bool getCurrentState() {
      return digitalRead(this->GPIO);
    }

    void listen() {
      if (digitalRead(this->GPIO) != this->lastState) {
        this->lastState = digitalRead(this->GPIO);
        invoke(this->stateChangeCallback, this->lastState);
        switch(digitalRead(this->GPIO)) {
          case HIGH: invoke(this->stateHighCallback);
            break;
          case LOW: invoke(this->stateLowCallback);
            break;
        }
      }
    }
    ~InputGPIO() {
      // nothing needed
    }
};
#endif
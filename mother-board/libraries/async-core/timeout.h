#ifndef TIMEOUT_H__
#define TIMEOUT_H__
#include "interval.h"
#include <functional>

typedef uint32_t TimeoutReference;

class Timeout : public Interval {
public:
  bool executed;
  Timeout(std::function<void()> callback, uint32_t duration)
    : Interval(callback, duration) {
    this->executed = false;
  }
  ~Timeout() {
    // Serial.printf("timeout deleted\n");
  }

  bool execute() override {
    if (this->isExpired()) {
      return true;
    }
    if (millis() - this->start > this->duration) {
      this->markExpired();
      this->executionTime = micros();
      invoke(callback);
      this->executionTime = micros() - this->executionTime;
      return true;
    }
    return false;
  }

};
#endif

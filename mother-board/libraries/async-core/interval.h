#ifndef INTERVAL_H__
#define INTERVAL_H__

#include <functional>
#include <Arduino.h>
typedef uint32_t IntervalReference;
typedef uint32_t IntervalDuration;

#define NULL_REFERENCE       0

class Interval {
public:
  IntervalDuration duration;
  uint32_t executionTime;
  uint32_t start;
  static uint32_t counter;
  IntervalReference id;
  std::function<void()> callback;
  uint32_t reps;
  Interval() {}
  Interval(std::function<void()> callback, IntervalDuration duration, uint32_t reps = 0xFFFFFFFF)
    : duration(duration), callback(callback), reps(reps) {
    this->id = ++Interval::counter;
    this->expired = false;
    this->start = millis(); // You need to define millis() function somewhere
  }

  static IntervalReference nextID() {
    return counter + 1;
  }

  virtual ~Interval() {
    // Serial.printf("%d interval removed\n", this->getID());
  }

  uint32_t getExecutionTime() {
    return this->executionTime;
  }

  IntervalDuration getDuration() {
    return this->duration;
  }

  void markExpired() {
    this->expired = true;
  }

  bool isExpired() {
    return this->expired;
  }

  IntervalReference& getID() {
    return this->id;
  }

  virtual bool execute() {
    if (!this->callback) {
      return false;
    }
    if (millis() - this->start > this->duration) {
      this->start = millis();
      this->executionTime = micros();
      invoke(callback);
      this->executionTime = micros() - this->executionTime;
      this->reps --;
      if (!this->reps) {
        this->markExpired();
      }
    }
    return true;
  }

private:
  bool expired;
};

uint32_t Interval::counter = 0;

#endif

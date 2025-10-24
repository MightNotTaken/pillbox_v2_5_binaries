#ifndef ALERT_H__
#define ALERT_H__
#include <event-handler.h>
enum Direction_T {
  UPWARD = 0,
  DOWNWARD = 1
};
class Alert: public EventHandler {
  int threshold;
  int currentValue;
  int recovery;
  int limit;
  bool triggered;
  uint8_t direction;
public:
  Alert(
    int threshold,
    int recovery,
    int limit,
    uint8_t direction
  ):
  threshold(threshold),
  recovery(recovery),
  direction(direction),
  limit(limit),
  triggered(false) {}

  void update(int newValue) {
    if (this->direction == Direction_T::UPWARD) {
      if (newValue > threshold && newValue <= limit) {
        if (!triggered) {
          triggered = true;
          EventHandler::call("trigger");
        }
      } else {
        if (triggered) {
          if (newValue < (threshold - recovery)) {
            triggered = false;
            EventHandler::call("recover");
          }
        }
      }
    } else {
      if (newValue < threshold && newValue >= limit) {
        if (!triggered) {
          triggered = true;
          EventHandler::call("trigger");
        }
      } else {
        if (triggered) {
          if (newValue > (threshold + recovery)) {
            triggered = false;
            EventHandler::call("recover");
          }
        }
      }
    }
  }
};
#endif
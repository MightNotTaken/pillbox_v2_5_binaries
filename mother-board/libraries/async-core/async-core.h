#ifndef CORE_H__
#define CORE_H__
#include "definitions.h"
#include <map>
#include "interval.h"
#include "timeout.h"
#include <vector>
#include <functional>

namespace AsyncCore {
  std::vector<Interval*> intervals;
  std::vector<Timeout*> timeouts;
  std::function<void()> highPriorityCallback;


  IntervalReference setInterval(std::function<void()> callback, IntervalDuration duration, uint32_t maxReps = 0xFFFFFFFF) {
    Interval *interval = new Interval(callback, duration, maxReps);
    intervals.push_back(interval);
    return interval->getID();
  }

  IntervalReference setImmediate(std::function<void()> callback, IntervalDuration duration, uint32_t maxReps = 0xFFFFFFFF) {
    callback();
    return setInterval(callback, duration, maxReps);
  }

  TimeoutReference setTimeout(std::function<void()> callback, IntervalDuration duration) {
    Timeout *timeout = new Timeout(callback, duration);
    timeouts.push_back(timeout);
    return timeout->getID();
  }

  void clearInterval(IntervalReference &ref) {
    if (ref == 1) {
      return;
    }
    for (size_t i = 0; i < intervals.size(); ++i) {
      if (intervals[i]->getID() == ref) {
        intervals[i]->markExpired();
        ref = NULL_REFERENCE;
        break;
      }
    }
  }

  void setHighPriority(std::function<void()> callback) {
    AsyncCore::highPriorityCallback = callback;
  }

  void clearTimeout(TimeoutReference &ref) {
    for (size_t i = 0; i < timeouts.size(); ++i) {
      if (timeouts[i]->getID() == ref) {
        timeouts[i]->markExpired();
        ref = NULL_REFERENCE;
        break;
      }
    }
  }

  void execute(void (*executable)()) {
    invoke(executable);
  }

  void clearImmediate(IntervalReference &ref) {
    clearInterval(ref);
  }

  void loop() {
    // Remove expired intervals
    for (size_t i = 0; i < intervals.size(); ++i) {
      if (intervals[i]->isExpired()) {
        delete intervals[i];
        intervals.erase(intervals.begin() + i);
      } else {
        intervals[i]->execute();
        invoke(highPriorityCallback);
      }
    }

    // Remove expired timeouts
    for (size_t i = 0; i < timeouts.size(); ++i) {
      if (timeouts[i]->isExpired() || timeouts[i]->execute()) {
        delete timeouts[i];
        timeouts.erase(timeouts.begin() + i);
      }
    }
  }
};

#endif

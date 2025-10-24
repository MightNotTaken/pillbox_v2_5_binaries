#ifndef ALARM_H__
#define ALARM_H__
#include <functional>
#include <interval.h>
#include <timeout.h>
#include <definitions.h>
#include <async-core.h>
#include <debugger.h>
#include <map>

typedef uint32_t AlarmReference;
typedef uint16_t AlarmRepeats;
using namespace AsyncCore;
enum {
  ALARM_NO_REPEAT = 1,
  ALARM_ONCE = 1,
  ALARM_REPEAT_UNITIL_STOP
};
class Alarm {
public:
  std::function<void()> activationCallback;
  std::function<void()> triggerCallback;
  std::function<void()> snoozeCallback;
  std::function<void()> expireCallback;
  std::function<void()> beforeTriggerCallback;

  TimeReference start;
  TimeReference marginStart;
  TimeReference marginEnd;
  AlarmRepeats repeat;
  TimeReference snoozeAfter;
  TimeReference repeatAfter;

  TimeoutReference waitRef;
  TimeoutReference expireRef;
  TimeoutReference snoozeRef;
  TimeoutReference marginRef;
  IntervalReference mainTracker;

  static AlarmReference count;
  static AlarmReference idCount;


  AlarmReference id;
  String time;
  bool active;
  bool complete;
  void show() {
    showX(this->waitRef);
    showX(this->marginRef);
    showX(this->snoozeRef);
    showX(this->mainTracker);
    showX(this->expireRef);
    showX(this->marginStart);
  }

  void unschedule() {
    clearTimeout(this->waitRef);
    clearTimeout(this->marginRef);
    clearTimeout(this->snoozeRef);
    clearTimeout(this->expireRef);
    clearInterval(this->mainTracker);
    // this->show();
  }

  Alarm(
    String time,
    int repeat = 1,
    TimeReference repeatAfter = MINUTES(3),
    TimeReference snoozeAfter = MINUTES(1),
    TimeReference marginStart = SECONDS(20),
    TimeReference marginEnd = SECONDS(61)
  ): time(time),
    repeat(repeat),
    repeatAfter(repeatAfter),
    snoozeAfter(snoozeAfter),
    marginStart(marginStart),
    marginEnd(marginEnd),
    complete(false) {
    this->id = ++Alarm::count;
    this->waitRef = 0;
    this->marginRef = 0;
    this->snoozeRef = 0;
    this->expireRef = 0;
    this->mainTracker = 0;
  }

  ~Alarm() {
    console.log("deleting alarm");
    this->unschedule();
  }

  void schedule() {
    this->active = false;
    this->unschedule();
    // this->show();
    TimeReference wait = getTimeStampDifference(this->time);
    if (wait < this->marginStart) {
      this->active = true;
      invoke(this->activationCallback);
    } else  {
      this->marginRef = setTimeout([this]() {
        this->active = true;
        invoke(this->activationCallback);
      }, wait - this->marginStart);
    }

    this->expireRef = setTimeout([this]() {
      this->active = false;
      this->complete = true;
      console.log("calling expire callback");
      invoke(this->expireCallback);
    }, wait + this->repeatAfter * (this->repeat - 1) + this->marginEnd);
    this->waitRef = setTimeout([this]() {
      this->mainTracker = setImmediate([this]() {
        if (!this->repeat) {
          this->complete = true;
          return;
        }
        invoke(this->beforeTriggerCallback);
        invoke(this->triggerCallback);
        
        this->snoozeRef = setTimeout([this]() {
          invoke(this->snoozeCallback);
        }, this->snoozeAfter);
        
        this->repeat --;
      }, this->repeatAfter);
    }, wait);
  }


  void onActivation(std::function<void()> callback) {
    this->activationCallback = callback;
  }

  void onTrigger(std::function<void()> callback) {
    this->triggerCallback = callback;
  }

  void onSnooze(std::function<void()> callback) {
    this->snoozeCallback = callback;
  }

  void onExpire(std::function<void()> callback) {
    this->expireCallback = callback;
  }

  void beforeTrigger(std::function<void()> callback) {
    this->beforeTriggerCallback = callback;
  }
};
AlarmReference Alarm::count = 0;


namespace Alarms {
  std::map<uint32_t, Alarm*> alarms;
  void log();
  void begin();
  Alarm* add(
    uint32_t id,
    String time,
    int repeat = 1,
    TimeReference repeatAfter = MINUTES(3),
    TimeReference marginStart = MINUTES(10),
    TimeReference marginEnd = SECONDS(61),
    TimeReference snoozeAfter = MINUTES(1)
  ) {
    console.log(id, time, repeat, repeatAfter/MINUTES(1), marginStart/MINUTES(1), marginEnd/MINUTES(1));
    console.log("Setting alarm at : ", time);
    Alarm* alarm = new Alarm(time, repeat, repeatAfter, snoozeAfter, marginStart, marginEnd);
    alarm->onActivation([]() {
      console.log("alarm activated");
    });
    alarm->onTrigger([]() {
      console.log("alarm triggered");
    });
    alarm->onSnooze([]() {
      console.log("alarm snoozed");
    });
    alarm->onExpire([]() {
      console.log("alarm expired");
    });
    alarm->beforeTrigger([]() {
      console.log("alarm going to trigger");
    });
    alarm->schedule();
    alarms[alarm->id] = alarm;
    return alarm;
  }
  void remove(Alarm* alarm) {
    auto it = Alarms::alarms.find(alarm->id);
    if (it != Alarms::alarms.end()) {
      delete it->second;
      Alarms::alarms.erase(it);
    }
  }
  
  void log() {
    if (Alarms::alarms.empty()) {
      return;
    }
    Debugger::table_t table;
    int index = 1;
    for (auto [id, alarm]: Alarms::alarms) {
      
      Debugger::table_row_t row;
      row.push_back(String(index++));
      row.push_back(String(id));
      row.push_back(alarm->time);
      row.push_back(formatMillis(getTimeStampDifference(alarm->time)));
      row.push_back(String(alarm->marginStart));
      row.push_back(String(alarm->repeat));
      row.push_back(String(alarm->repeatAfter));
      row.push_back(String(alarm->snoozeAfter));
      row.push_back(String(alarm->active ? "Yes" : "No"));
      table.push_back(row);
    }
    Debugger::displayTable(
      "Alarms",
      {"S.No.", "ID", "Trigger Time", "Remaining Time", "Margin", "Repeatitions", "Repeat Time", "Snooze Time", "Active"},
      table
    );
    
  }
  
  void begin() {
    setInterval([]() {
      // Alarms::log();
    }, SECONDS(3));
  }

}
#endif

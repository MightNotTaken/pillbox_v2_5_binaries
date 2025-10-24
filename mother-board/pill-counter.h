#ifndef PILL_COUNTERS_H__
#define PILL_COUNTERS_H__
#include "viles.h"
#include <definitions.h>
#include "tab-count.h"
#include <debugger.h>
#include <vector>
#include <serial-interpretter.h>

namespace PillCounters {
  IntervalReference tracker = NULL_REFERENCE;
  class PillCounter {
    int count;
    Viles::Vile* vile;
    uint32_t id;
    bool active;
    TimeoutReference timeout;
    IntervalReference repeater;
    TimeReference start;
    bool expired;
    bool muted;
    public:
      PillCounter(uint32_t id, int count, Viles::Vile* vile, bool muted)
        : id(id), count(count), vile(vile), active(false), expired(false), start(0), muted(muted) {
      }

      void display() {
        if (!start) {
          start = millis();
        }
        this->active = true;
        this->show();
        this->repeater = setInterval([this]() {
          console.log("playing audio");
          this->vile->blink(SECONDS(1), 4);
          if (!muted) {
            AudioPlayer::play("/alarm.mp3", AudioPlayMode::PLAY_MODE_IMMEDIATE);
          }
          TabCount::setCount(this->count);
        }, SECONDS(4.4));
      }

      TimeReference ellapsedTime() {
        if (!start) {
          return 0;
        }
        return millis() - start;
      }
  
      bool isExpired() {
        return expired;
      }

      void expire() {
        expired = true;
      }

      bool isActive() {
        return active;
      }

      uint32_t getId() {
        return this->id;
      }

      void reset() {
        TabCount::setCount(0);
        this->vile->stopBlink();
        clearInterval(this->repeater);
        clearTimeout(this->timeout);
        AudioPlayer::stop();
      }

      Viles::Vile* getVile() {
        return this->vile;
      }

      int getCount() {
        return this->count;
      }

      void show() {
        console.log("id: ", id);
        console.log("vile: ", vile->getID());
        console.log("count: ", count);
        console.log("active: ", active ? "true" : "false");
      }

      ~PillCounter() {
        this->reset();
        console.log("pill counter removed");
      }
  };

  std::vector<PillCounter*> list;
  size_t currentCounterIndex = 0;

  void loop();

  void log() {
    if (PillCounters::list.empty()) {
      return;
    }
    Debugger::table_t table;
    int index = 1;
    for (auto item : list) {
      Debugger::table_row_t row;
      console.log(item->getId());
      row.push_back(String(index++));
      row.push_back(String(item->getId()));
      row.push_back(String(item->getVile()->getID()));
      row.push_back(String(item->getCount()));
      row.push_back(String(item->isActive() ? "yes" : "no"));
      row.push_back(formatMillis(item->ellapsedTime()));
      table.push_back(row);
    }
    Debugger::displayTable(
      "Pill Counters",
      {"S.No.", "ID", "Vile", "Pills", "Active", "Time Ellapsed"},
      table
    );
  }

  void next() {
    if (PillCounters::list.empty()) {
      return;
    }
    PillCounter* front = PillCounters::list.front();
    if (front) {
      front->display();
    }
  }

  void add(uint32_t id, int count, int vile, bool muted) {
    PillCounter* pc = new PillCounter(id, count, Viles::viles[vile], muted);
    PillCounters::list.push_back(pc);
    if (PillCounters::list.size() == 1) {
      PillCounters::next();
    }
  }

  void remove(uint32_t id) {
    console.log("removing", id);
    for (auto it = PillCounters::list.begin(); it != PillCounters::list.end(); ++it) {
      if ((*it)->getId() == id) {
        (*it)->expire();
        break;
      }
    }
  }


  void loop() {
    setInterval([]() {
      for (auto it = PillCounters::list.begin(); it != PillCounters::list.end(); ++it) {
        if ((*it)->isExpired() || (*it)->ellapsedTime() > MINUTES(1)) {
          delete *it;
          PillCounters::list.erase(it);
          next();
          break;
        }
      }
      // PillCounters::log();
    }, 500);
  }
};
#endif
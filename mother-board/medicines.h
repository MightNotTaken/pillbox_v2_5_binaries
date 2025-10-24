#ifndef MEDICINE_H__
#define MEDICINE_H__
#include <alarm.h>
#include <definitions.h>
#include <debugger.h>
#include <JSON.h>
#include "mqtt.h"
#include "log.h"
#include "pill-counter.h"
#include "viles.h"
#include <async-core.h>
#include "audio-player.h"

using namespace AsyncCore;
namespace Medicines {
  IntervalReference dataRefresher;
  TimeoutReference sleepTracker;
  void remove(uint32_t);
  void log();
  struct MedicineData {
    uint32_t id;
    int vile;
    char time[12];
    int pills;
    int repeat;
    bool parsed;
    int windowSize;
    int muted;


    void show() {
      showX(id);
      showX(vile);
      showX(time);
      showX(pills);
      showX(repeat);
      showX(windowSize);
      showX(muted);
    }

    bool parse(String& raw) {
      raw.trim();
      if (!raw.length()) {
        return false;
      }
      if (!raw.startsWith("m-")) {
        return false;
      }
      sscanf(raw.c_str(), "m-%d-%ld,%d,%d_%s", &this->repeat, &this->id, &this->vile, &this->pills, &this->time);
      sscanf(raw.c_str(), "m-%d", &this->repeat);
      this->vile -= 1;
      this->vile %= 6;
      
      raw += '\n';
      raw = raw.substring(raw.indexOf('\n') + 1, raw.length());
      return true;
    }
  } medicineDataParser;
  class Medicine {
    public:
    std::function<void()>onPickCallback;
    uint32_t id;
    Viles::Vile* vile;
    byte pills;
    String time;
    int repeatDuration;
    bool consumed;
    bool missed;
    bool active;
    bool muted;
    int windowSize;
    TimeoutReference metaInfoTracker;
    Alarm* alarm;
      Medicine(
        byte vileIndex,
        uint32_t id,
        byte pills,
        String time,
        int repeatDuration
      )
      : id(id), pills(pills), time(time), repeatDuration(repeatDuration) {
        this->windowSize = 20;
        this->muted = false;
        this->vile = Viles::viles[vileIndex];
        this->active = false;
        this->missed = false;
        this->consumed = false;
        metaInfoTracker = setTimeout([this]() {
          console.log("emitting med-meta");
          JSON data;
          data["mac"] = MAC::getMac();
          data["id"] = this->id;
          wifiMQTT.emit("med-meta", data.toString(), true, LogDataType::LOG_OBJECT);
        }, 5000);
        wifiMQTT.listen(String(MAC::getMac()) + "/meta-" + this->id, [this](String data) {
          sscanf(data.c_str(), "%d_%d", &this->muted, &this->windowSize);
          console.log(this->muted, this->windowSize);
          clearTimeout(metaInfoTracker);
          wifiMQTT.stopListen(String(MAC::getMac()) + "/meta-" + this->id);
          this->schedule();
          Medicines::log();
        });

      }
      
      void schedule() {

        this->alarm = Alarms::add(
          this->id,
          time,
          2,
          MINUTES(this->repeatDuration),
          MINUTES(max(windowSize / 2, 10)),
          MINUTES(max(windowSize / 2 - this->repeatDuration, 1))
        );

        this->onPickCallback = [this]() {
          setTimeout([this]() {
            this->consumed = true;
            PillCounters::remove(this->id);
          }, 1200);
          AudioPlayer::stop();
          Logs::setVilePosition(Viles::updateVilePosition());
          Logs::add(this->vile->getID(), this->id);
          console.log("vile picked from medicine");
        };

        this->alarm->onActivation([this]() {
          this->active = true;
          this->consumed = false;
          this->missed = false;
          console.log("Medicine activated");
          if (this->vile->inPlace()) {
            this->vile->onPick([this]() {
              this->onPickCallback();
              AudioPlayer::play("/buffer-pick.mp3");
            });
          }
        });

        this->alarm->onTrigger([this]() {
          if (this->consumed) {
            console.log("Medicine already consumed");
          } else {
            if (this->vile->inPlace()) {
              this->vile->onPick([this]() {
                this->onPickCallback();
                AudioPlayer::play("/buffer-pick.mp3");
              });
              PillCounters::add(this->id, this->pills, this->vile->getID(), this->muted);
            } else {
              console.log("vile not placed");
              this->missed = true;
            }
          }
        });
        
        this->alarm->onSnooze([this]() {
          // console.log("snoozed");
          // PillCounters::remove(this->id);
          // console.log("pill couter removed");
        });
        
        this->alarm->onExpire([this]() {
          this->missed = true;
          this->active = false;
          PillCounters::remove(this->id);
          console.log("alarm expired");
        });
        this->alarm->schedule();
        
      }
        
      bool isMissed() {
        return this->missed;
      }

      bool isActive() {
        return this->active;
      }

      bool isConsumed() {
        return this->consumed;
      }

      uint32_t getId() {
        return this->id;
      }

      Medicine(MedicineData& med) {
        Medicine(med.vile, med.id, med.pills, med.time, med.repeat);
      }

      ~Medicine() {
        this->vile->release();
        PillCounters::remove(this->id);
        Alarms::remove(this->alarm);
      }
  };

  std::map<uint32_t, Medicine*> medicines;
  std::function<void(String)> sleepCallback;
  void loop();

  void onNoMedicineReceived(std::function<void(String)> callback) {
    Medicines::sleepCallback = callback;
  }

  void remove(uint32_t id) {
    auto it = medicines.find(id);
    if (it != medicines.end()) {
      delete it->second;
      medicines.erase(it);
    }
  }

  void reset() {
    for (auto [reference, medicine]: medicines) {
      delete medicine;
    }
    medicines.clear();
    console.log("reset done");
  }

  Medicine* getMedicine(uint32_t id) {
    auto it = medicines.find(id);
    if (it != medicines.end()) {
      return it->second;
    }
    return nullptr;
  }

  void add(MedicineData medData) {
    auto it = medicines.find(medData.id);
    if (it == medicines.end()) {
      Medicine* med = new Medicine(medData.vile, medData.id, medData.pills, medData.time, medData.repeat);
      Medicines::medicines[medData.id] = med;
    }
  }

  void log() {
    if (Medicines::medicines.empty()) {
      return;
    }
    Debugger::table_t table;
    int index = 1;
    for (auto [_, medicine]: Medicines::medicines) {
      Debugger::table_row_t row;
      row.push_back(String(index++));
      row.push_back(String(medicine->getId()));
      row.push_back(String(medicine->vile->getID()));
      row.push_back(String(medicine->pills));
      row.push_back(String(medicine->windowSize));
      row.push_back(String(medicine->muted ? "Yes" : "No"));
      row.push_back(formatMillis(MINUTES(medicine->repeatDuration)));
      row.push_back(medicine->time);
      table.push_back(row);
    }
    Debugger::displayTable(
      "Medicines",
      {"S.No.", "Medicine ID", "Vile", "Pills", "Window size", "Muted", "Repeat After", "Trigger Time"},
      table
    );
  }


  void refreshDataListener() {
    console.log("listening to medicine changes");
    wifiMQTT.listen(MAC::getMac() + "/sleep", [](String message) {
      invoke(Medicines::sleepCallback, message);
    });
    wifiMQTT.listen(MAC::getMac() + "/medicine", [](String medicines) {
      console.log("medicines", medicines);
      while (medicineDataParser.parse(medicines)) {
        medicineDataParser.show();
        Medicines::add(medicineDataParser);
      }
      console.log("going to log now");
      
    });
    
    wifiMQTT.listen(MAC::getMac() + "/remove", [](String medicineIDs) {
      medicineIDs.trim();
      medicineIDs += ',';
      console.log("remove event");
      while (medicineIDs.length()) {
        uint32_t id;
        sscanf(medicineIDs.c_str(), "%ld", &id);
        medicineIDs = medicineIDs.substring(medicineIDs.indexOf(',') + 1, medicineIDs.length());
        Medicine* med = Medicines::getMedicine(id);
        if (med) {
          Medicines::remove(id);
        }
      }
      Medicines::log();
    });
  }

  String getIDs() {
    String data = "[";
    for(auto [id, medicine]: Medicines::medicines) {
      data += medicine->getId();
      data += ",";
    }
    data += "]";
    data.replace(",]", "]");
    return data;
  }

  void begin(std::function<void(String)> sleepCallback = nullptr) {
    Medicines::sleepCallback = sleepCallback;
    setInterval([]() {
      // Medicines::log();
    }, SECONDS(3));
    setInterval([]() {
      if (Medicines::medicines.empty()) {
        return;
      }
      for(auto [id, medicine]: Medicines::medicines) {
        if (medicine->isConsumed() || medicine->isMissed()) {
          setTimeout([id]() {
            console.log("removig medicine", id);
            Medicines::remove(id);
          }, 0);
        }
      }
    }, SECONDS(.5));
  }
};
#endif
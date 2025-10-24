#ifndef VILES_H__
#define VILES_H__
#include <vector>
#include <gpio-config.h>
#include <async-core.h>
#include <functional>
#include <database.h>
#include "mqtt.h"
#include "log.h"
#include "audio-player.h"
#include <my-web-server.h>

#define IR_ENABLE       13

#define IR_1            35
#define IR_2            32
#define IR_3            33
#define IR_4            34
#define IR_5            39
#define IR_6            36

#define LOG_DELAY       SECONDS(2)

using namespace GPIOConfig;

namespace Viles {
  OutputGPIO* IRMasterControl;
  String statusString = "000000";
  TimeoutReference positionUpdateTracker;
  std::vector<InputOutputPair*> VileGPIOs = {
    new InputOutputPair(IR_1, 0, GPIOType::HC_595),
    new InputOutputPair(IR_2, 1, GPIOType::HC_595),
    new InputOutputPair(IR_3, 2, GPIOType::HC_595),
    new InputOutputPair(IR_4, 5, GPIOType::HC_595),
    new InputOutputPair(IR_5, 4, GPIOType::HC_595),
    new InputOutputPair(IR_6, 3, GPIOType::HC_595)
  };
  String updateVilePosition(bool force = false);
  class Vile {
    InputGPIO* irSensor;
    OutputGPIO* led;
    std::function<void()> onPickCallback;
    std::function<void()> onPlaceCallback;
    std::function<void()> onPickFallbackCallback;
    std::function<void()> onPlaceFallbackCallback;
    std::function<void()> onBlinkStopCallback;
    std::function<void()> emergencyCallback;
    int id;
    static int count;
    bool activated;
    int blinkCount;
    IntervalReference blinkTracker;
    TimeoutReference blinkStopper;
    TimeoutReference vilePlaceBackTracker;
    IntervalReference vilePlaceBackAudioTracker;
    TimeoutReference vilePlaceBackAudioTimeoutTracker;
    TimeoutReference vilePlaceBackLogTimeoutTracker;
    TimeReference lastLogTime;
    bool testing;
    TimeReference activationTime;
  public:
    TimeoutReference logEmitter;
    bool audioLocked;
    IntervalReference vilePlaceTracker;
    void lockAudio() {
      audioLocked = true;
    }
    void unlockAudio() {
      this->audioLocked = true;
    }
    Vile(InputOutputPair* gpioPair) {
      this->audioLocked = false;
      this->vilePlaceBackAudioTracker = false;
      this->testing = false;
      this->id = this->count++;
      this->activated = true;
      this->lastLogTime = 0;
      this->led = new OutputGPIO(gpioPair->output->pin, gpioPair->output->type);
      this->irSensor = new InputGPIO(gpioPair->inputPin);
      GPIOConfig::registerInput(this->irSensor);
      this->irSensor->onStateHigh([this]() {
        console.log("high:", this->id);
        if (this->testing) {
          this->blink(500);
        }
        setTimeout([this]() {
          if (this->irSensor->getCurrentState() != HIGH) {
            // console.log("ignored");
            return;
          }
          console.log("considered");
          if (!this->activated) {
            // console.log("vile", this->id, "not activated");
            return;
          }
          console.log("vile", this->id, "picked");
          invoke(this->onPickCallback);
          invoke(this->emergencyCallback);
          
          clearTimeout(this->vilePlaceBackTracker);
          this->vilePlaceBackTracker = setTimeout([this]() {
            this->vilePlaceBackAudioTracker = setImmediate([this]() {
              AudioPlayer::play(String("/vile-put-") + (this->id + 1) + ".mp3");
              this->stopBlink();
              this->blink(SECONDS(.20), 90);
            }, SECONDS(20));
            this->vilePlaceBackAudioTimeoutTracker = setTimeout([this]() {
              clearImmediate(this->vilePlaceBackAudioTracker);
              this->stopBlink();
            }, SECONDS(79));

            this->vilePlaceBackLogTimeoutTracker = setTimeout([this]() {
              JSON data;
              data["mac"] = MAC::getMac();
              data["env"] = Logs::getEnv();
              data["vile"] = this->id + 1;
              wifiMQTT.emit("vile-not-placed", data.toString());
            }, MINUTES(6.5));
          }, MINUTES(7));
        }, 1000);
      });
      this->irSensor->onStateLow([this]() {
        if (this->testing) {
          this->blink(500);
        }
        if (!this->activated) {
          // console.log("vile", this->id, "not activated");
          return;
        }
        if (this->vilePlaceBackAudioTracker) {
          setTimeout([this]() {
            if (audioLocked) {
              audioLocked = false;
            } else {
              AudioPlayer::play("/vile-in-place.mp3");
            }
          },  1000);
        }   
        this->flushPlaceBack();
        this->stopBlink();
        AudioPlayer::stop();
        invoke(this->onPlaceCallback);
      });
      setTimeout([this]() {
        this->irSensor->setForceState(LOW);
        this->activationTime = millis();
      }, 2500 * this->id);
    }

    TimeReference getActivationTime() {
      return activationTime;
    }

    void flushPlaceBack() {
      clearTimeout(this->vilePlaceBackTracker);
      clearImmediate(this->vilePlaceBackAudioTracker);
      clearTimeout(this->vilePlaceBackAudioTimeoutTracker);
      clearTimeout(this->vilePlaceBackLogTimeoutTracker);
      apCtrl.unqueue(String("/vile-put-") + (this->id + 1) + ".mp3");
    }

    void setTesting(bool status) {
      this->testing = status;
    }

    bool isTesting() {
      return this->testing;
    }

    bool previousLogTooOld() {
      if (!lastLogTime) {
        lastLogTime = millis();
        return true;
      }
      if (millis() - lastLogTime > SECONDS(2)) {
        lastLogTime = millis();
        return true;
      }
      return false;
    }

    bool inPlace(bool forced = false) {
      return (forced || this->activated) && this->inCompartment();
    }

    bool inCompartment() {
      return this->irSensor->getCurrentState() == LOW;
    }

    int getID() {
      return this->id;
    }

    void onPick(std::function<void()> callback) {
      this->onPickCallback = callback;
      if (!this->onPickFallbackCallback) {
        this->onPickFallbackCallback = callback;
      }
    }

    void onEmergency(std::function<void()> callback) {
      this->emergencyCallback = callback;
    }

    void onPlace(std::function<void()> callback) {
      this->onPlaceCallback = callback;
      if (!this->onPlaceFallbackCallback) {
        this->onPlaceFallbackCallback = callback;
      }
    }

    void release() {
      this->onPickCallback = this->onPickFallbackCallback;
      this->onPlaceCallback = this->onPlaceFallbackCallback;
    }

    void activate() {
      this->activated = true;
      statusString[this->id] = '1';
    }

    bool isActivated() {
      return this->activated;
    }
  
    void deactivate() {
      this->activated = false;
      statusString[this->id] = '0';
      this->flushPlaceBack();
    }

    void stopBlink() {
      this->led->stopBlink();
      invoke(this->onBlinkStopCallback);
    }

    void onBlinkStop(std::function<void()> callback) {
      this->onBlinkStopCallback = callback;
    }

    void blink(uint32_t duration, int count = 1) {
      this->led->enable();
      this->led->blink(duration, count);
    }
  };

  int Vile::count = 0;

  std::vector<Vile*> viles;

  void listen() {
    IRMasterControl->turnOff();
  }

  void turnOff() {
    IRMasterControl->turnOn();
  }

  void loadStatus() {
    String fileName = "/vile/status";
    if (!Database::hasFile(fileName)) {
      Database::writeFile(fileName, "000000");
    }
    Database::readFile(fileName);
    console.log("payload", Database::payload());
    statusString = Database::payload();
    for (int i = 0; i < statusString.length(); i++) {
      if (i >= 6) {
        break;
      }
      statusString[i] == '1' ? viles[i]->activate() : viles[i]->deactivate();
      // console.log("vile", i, viles[i]->isActivated() ? "activated" : "not activated");
    }
  }

  void setStatus(String status) {
    if (status.length() == 6) {
      Database::writeFile("/vile/status", status);
      for (int i = 0; i < status.length(); i++) {
        status[i] == '1' ? viles[i]->activate() : viles[i]->deactivate();
      }
    }
  }

  void blinkAll() {
    for (int i = 0; i < 6; i++) {
      viles[i]->blink(SECONDS(.4));
    }
  }

  String getStatus() {
    return statusString;
  }

  void listenStatusChange() {
    console.log("listening to vile activation deactivation", MAC::getMac());
    
    wifiMQTT.listen(MAC::getMac() + "/med-alert", [](String _data) {
      JSON data(_data);
      int i;
      sscanf(data["message"].toString().c_str(), "med:%d", &i);
      console.log("i", i);
      if (!Viles::viles[i]->inPlace(true)) {
        Viles::viles[i]->onPlace([i]() {
          clearInterval(Viles::viles[i]->vilePlaceTracker);
          AudioPlayer::play(String("/compartment-") + (i + 1) + ".mp3");
          // Viles::viles[i]->lockAudio();
          // setTimeout([i]() {
          //   Viles::viles[i]->unlockAudio();
          // }, 3000);
          Viles::viles[i]->stopBlink();
          Viles::viles[i]->release();
          setTimeout([]() {
            Viles::updateVilePosition(true);
          }, 5000);
        });
        console.log("clearing interval", Viles::viles[i]->vilePlaceTracker);
        clearInterval(Viles::viles[i]->vilePlaceTracker);
        Viles::viles[i]->vilePlaceTracker = setImmediate([i]() {
          AudioPlayer::play(String("/vile-put-") + (i + 1) + ".mp3");
          Viles::viles[i]->stopBlink();
          Viles::viles[i]->blink(SECONDS(.20), 90);
        }, SECONDS(20));
        setTimeout([i]() {
          clearInterval(Viles::viles[i]->vilePlaceTracker);
          AudioPlayer::stop();
          Viles::viles[i]->release();
          Viles::viles[i]->stopBlink();
        }, SECONDS(80));
      } else {
        AudioPlayer::play(String("/compartment-") + (i + 1) + ".mp3");
        Viles::updateVilePosition(true);
      }
    });

    wifiMQTT.listen(MAC::getMac() + "/viles", [](String vileStatus) {
      console.log("vileStatus", vileStatus);
      Logs::vileStatus = vileStatus;
      Viles::setStatus(vileStatus);
    });

    wifiMQTT.listen(MAC::getMac() + "/testing", [](String status) {
      Database::writeFile("/testing.conf", status);
      for (int i=0; i<6; i++) {
        Viles::viles[i]->setTesting(status.toInt());
      }
    });
  }

  String updateVilePosition(bool force) {
    clearTimeout(positionUpdateTracker);
    JSON data;
    String position = "";
    console.log("positionUpdateTracker");
    for (auto vile : Viles::viles) {
      position += vile->inCompartment() ? '1' : '0';
    }
    data["mac"] = MAC::getMac();
    data["vilePosition"] = position;
    if (force) {
      console.log(data);
      wifiMQTT.emit("vile-position", data.toString(), true, LogDataType::LOG_OBJECT);
    }
    return position;
  }

  void begin(std::function<void()> emergencyCallback) {
    IRMasterControl = new OutputGPIO(IR_ENABLE, GPIOType::DIGITAL_PIN);
    console.log("Enabling viles");
    bool testing = false;

    if (!Database::hasFile("/testing.conf")) {
      Database::writeFile("/testing.conf", "0");
    }

    if (Database::readFile("/testing.conf")) {
      testing = Database::payload().toInt();
    }

    for (auto gpio : VileGPIOs) {
      Vile* vile = new Vile(gpio);
      vile->setTesting(testing);
      vile->onPick([vile]() {
        console.log("Vile picked", vile->getID());
        vile->stopBlink();
        if (millis() - vile->getActivationTime() > 1500) {
          vile->blink(SECONDS(.2), 5);
          AudioPlayer::play("/unschedule.mp3");
        }
        Logs::setVilePosition(updateVilePosition());
        clearTimeout(vile->logEmitter);
        vile->logEmitter = setTimeout([vile]() {
          Logs::add(vile->getID(), 0);
        }, LOG_DELAY);
      });
      vile->onPlace([vile]() {
        console.log("Vile placed", vile->getID());
        vile->stopBlink();
        clearTimeout(vile->logEmitter);
        AudioPlayer::stop();
        updateVilePosition(true);
      });
      vile->onEmergency(emergencyCallback);
      viles.push_back(vile);
    }
    Viles::loadStatus();
    Viles::listen();
  }

  void beginRoutes() {
    
    webServer.get("/api/viles/activate", [](Request* request) {
      Viles::setStatus("111111");
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });

    
    webServer.get("/api/viles/set-status", [](Request* request) {
      int vile = request->getParam("vile")->value().toInt();
      int status = request->getParam("status")->value().toInt();
      if (vile < 6) {
        status ? Viles::viles[vile]->activate() : Viles::viles[vile]->deactivate(); 
      }
      Viles::setStatus(Viles::getStatus());
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });

    webServer.get("/api/viles/get-status", [](Request* request) {
      JSON response;
      response["status"] = Viles::getStatus();
      int code = 200;
      return std::make_pair(code, response.toString());
    });

    webServer.get("/api/viles/testing/start", [](Request* request) {
      Database::writeFile("/testing.conf", "1");
      for (int i=0; i<6; i++) {
        Viles::viles[i]->setTesting(1);
      }
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });

    webServer.get("/api/viles/testing/end", [](Request* request) {
      Database::writeFile("/testing.conf", "0");
      for (int i=0; i<6; i++) {
        Viles::viles[i]->setTesting(0);
      }
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });

    webServer.get("/api/viles/deactivate", [](Request* request) {
      Viles::setStatus("000000");
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });
  }
};
#endif

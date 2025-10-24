#ifndef AUDIO_H__
#define AUDIO_H__
#include "Arduino.h"
#include "mqtt.h"
#include <mac.h>
#include <JSON.h>
#include <http.h>
#include <console.h>
#include <database.h>
#include <my-web-server.h>
#include <async-core.h>
#include "audio-player-controller.h"

#define I2S_DOUT      12
#define I2S_BCLK      5
#define I2S_LRC       18
long arduino_map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
extern Audio audio;
namespace AudioPlayer {
  bool disabled = false;
  int getVolume();
  void play(String, AudioPlayMode = AudioPlayMode::PUT_IN_QUEUE);
  void initialize() {
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(getVolume());
    audio.forceMono(true);
  }

  int getVolume() {
    if (!Database::hasFile("/volume.conf")) {
      Database::writeFile("/volume.conf", String(100));
    }
    Database::readFile("/volume.conf");
    int volume = Database::payload().toInt();
    if (volume < 30) {
      volume = 30;
    }
    if (volume > 100) {
      volume = 100;
    }
    return arduino_map(volume, 0, 100, 0, audio.maxVolume());
  }

  void setVolume(int volume) {
    if (volume < 30) {
      volume = 30;
    }
    if (volume > 100) {
      volume = 100;
    }
    Database::writeFile("/volume.conf", String(volume));
    audio.setVolume(getVolume());
    AudioPlayer::play("/unschedule.mp3", AudioPlayMode::PLAY_MODE_IMMEDIATE);
  }
  void begin(String welcomeNote) {
    AudioPlayer::initialize();
    Serial.printf("volume: %d\n", getVolume());
    AudioPlayer::play(welcomeNote);
    wifiMQTT.on(MAC::getMac() + "/audio-download", [](String data) {
      JSON params(data);
      console.log(params);
      console.log("memory left", SPIFFS.totalBytes() - SPIFFS.usedBytes());
      String host = params["host"].toString();
      int port = params["port"].toInt();
      String file = params["file"].toString();
      file.trim();
      httpClient.on("data", [file](String bytesToRead) {
        console.log("bytes to receive", bytesToRead);
        uint32_t remaining = bytesToRead.toInt();
        uint32_t start = millis();
        String fileName = String("/") + file;
        Serial.println(fileName);
        Database::removeFile(fileName);
        File file = Database::fs.open(fileName.c_str(), FILE_WRITE);
        if (!file) {
          console.log("Unable to open file");
          return;
        }
        while (remaining > 0) {
          while (bridge->available()) {
            start = millis();
            file.write(bridge->read());
            remaining --;
            if (remaining % 1000 == 0) {
              console.log("file written", 100 * (bytesToRead.toInt() - remaining) / bytesToRead.toInt(), "%");
            }
          }
          if (millis() - start > SECONDS(5)) {
            break;
          }
        }
        console.log("file written 100 %");
        file.close();
        console.log("memory left", SPIFFS.totalBytes() - SPIFFS.usedBytes());
      });
      httpClient.on("size", [](String bytes) {
        console.log("going to receive", bytes, "bytes");
      });
      httpClient.on("complete", [file](String status) {
        console.log("Request completed", status.toInt() ? "successfully" : "with error");
        AudioPlayer::play(String("/") + file);
      });
      httpClient.get(host, port, String("/api/v1/audio/download?file=") + file);
    });
  }
  
  void registerAudioRoute() {
    webServer.get("/audio-play", [](Request* request) {
      String audio = request->getParam("file")->value();
      AudioPlayer::play(audio);
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });
    webServer.get("/audio-remove", [](Request* request) {
      String audio = request->getParam("file")->value();
      Database::removeFile(audio);
      JSON response;
      response["status"] = "okay";
      response["removed"] = audio;
      int code = 200;
      return std::make_pair(code, response.toString());
    });
    webServer.on("/audio-download", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    // Handle preflight OPTIONS request
      AsyncWebServerResponse *response = request->beginResponse(204);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      request->send(response);
    });
    webServer.on("/audio-download", HTTP_POST, [](AsyncWebServerRequest *request) {
        JSON json;
        json["status"] = "okay";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.toString());
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        if (!SPIFFS.open("/" + filename, FILE_WRITE)) {
          Serial.println("Failed to open file for writing");
          return request->send(500, "text/plain", "Failed to open file for writing");
        }
      }
      File file = SPIFFS.open("/" + filename, FILE_APPEND);
      if (file) {
        if (file.write(data, len) != len) {
          Serial.println("Failed to write file");
          return request->send(500, "text/plain", "Failed to write file");
        }
        file.close();
      } else {
        Serial.println("Failed to open file for appending");
        return request->send(500, "text/plain", "Failed to open file for appending");
      }
      if (final) {
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        JSON json;
        json["status"] = "okay";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json.toString());
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        request->send(response);
      }
    });
  }

  void stop() {
    audio.stopSong();
  }

  void play(String file, AudioPlayMode mode) {
    apCtrl.play(file, mode);
  }
}
#endif


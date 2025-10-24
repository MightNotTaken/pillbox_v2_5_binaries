#ifndef CUSTOMER_CARE_H__
#define CUSTOMER_CARE_H__
#include "wifi_c.h"
#include "mqtt.h"
#include "power-source.h"
#include <mac.h>
#include <JSON.h>
#include "viles.h"
#include "tab-count.h"
#include "audio-player.h"
#include <my-web-server.h>

namespace Reference {
  enum Led {
    BATTERY_LED = 0,
    WIFI_LED,
    COMPARTMENT_1,
    COMPARTMENT_2,
    COMPARTMENT_3,
    COMPARTMENT_4,
    COMPARTMENT_5,
    COMPARTMENT_6
  };

  enum LedColor {
    NO_COLOR_CHANGE = 0,
    LED_GREEN = 1,
    LED_RED = 2,
    LED_BLUE = 3,
    LED_YELLOW = 4
  };

  enum Sound {
    PUT_BACK_VILE = 0,
    COMPARTMENT_SUCCESS = 1
  };
};

class CustomerCareService {
public:
  void begin();
  void beginWebServerRoutes();
} customerCare;


void CustomerCareService::beginWebServerRoutes() {

  webServer.get("/blink-led", [](Request* request) {
    int led;
    int color;

    JSON response;
    response["status"] = "okay";
    int code = 200;
    String led_color = request->getParam("led_color")->value();
    sscanf(led_color.c_str(), "%d_%d", &led, &color);
    if (led > Reference::Led::COMPARTMENT_6) {
      response["status"] = "error";
      return std::make_pair(400, response.toString());
    }
    if (led >= Reference::Led::COMPARTMENT_1) {
      Viles::viles[led - Reference::Led::COMPARTMENT_1]->blink(SECONDS(.5), 5);
      return std::make_pair(200, response.toString());
    }
    OutputGPIO* ledPin;
    switch (led) {
      case Reference::Led::BATTERY_LED:
        ledPin = battery.batteryIndicator;
        break;
      case Reference::Led::WIFI_LED:
        ledPin = wifi.indicator;
        break;
      default:       response["status"] = "error";
      return std::make_pair(400, response.toString());

    }

    switch (color) {
      case Reference::LedColor::NO_COLOR_CHANGE:
        ledPin->blink(SECONDS(.5), 5);
        break;
      case Reference::LedColor::LED_BLUE:
        ledPin->setColor(Colors::BLUE_COLOR);
        break;
      case Reference::LedColor::LED_GREEN:
        ledPin->setColor(Colors::GREEN_COLOR);
        break;
      case Reference::LedColor::LED_RED:
        ledPin->setColor(Colors::RED_COLOR);
        break;
      case Reference::LedColor::LED_YELLOW:
        ledPin->setColor(Colors::YELLOW_COLOR);
        break;
    }

    return std::make_pair(code, response.toString());
  });
}

void CustomerCareService::begin() {
  wifiMQTT.listen(MAC::getMac() + "/c-get-battery", [](String _) {
    battery.updatePercentage();
  });
  wifiMQTT.listen(MAC::getMac() + "/c-wifi-get", [](String index) {
    JSON response;
    response["mac"] = MAC::getMac();
    if (index.toInt() < wifi.size()) {
      response["cred"] = wifi[index.toInt()];
    } else {
      response["cred"] = "";
    }
    wifiMQTT.emit("c-wifi-get", response.toString());
  });
  wifiMQTT.listen(MAC::getMac() + "/c-blink-led", [](String led_color) {
    int led;
    int color;
    sscanf(led_color.c_str(), "%d_%d", &led, &color);
    if (led > Reference::Led::COMPARTMENT_6) {
      return;
    }
    if (led >= Reference::Led::COMPARTMENT_1) {
      Viles::viles[led - Reference::Led::COMPARTMENT_1]->blink(SECONDS(.5), 5);
      return;
    }
    OutputGPIO* ledPin;
    switch (led) {
      case Reference::Led::BATTERY_LED:
        ledPin = battery.batteryIndicator;
        break;
      case Reference::Led::WIFI_LED:
        ledPin = wifi.indicator;
        break;
      default: return;
    }

    switch (color) {
      case Reference::LedColor::NO_COLOR_CHANGE:
        ledPin->blink(SECONDS(.5), 5);
        break;
      case Reference::LedColor::LED_BLUE:
        ledPin->setColor(Colors::BLUE_COLOR);
        break;
      case Reference::LedColor::LED_GREEN:
        ledPin->setColor(Colors::GREEN_COLOR);
        break;
      case Reference::LedColor::LED_RED:
        ledPin->setColor(Colors::RED_COLOR);
        break;
      case Reference::LedColor::LED_YELLOW:
        ledPin->setColor(Colors::YELLOW_COLOR);
        break;
    }
  });
  wifiMQTT.listen(MAC::getMac() + "/c-pill-count", [](String count) {
    TabCount::setCount(count.toInt());
    setTimeout([]() {
      TabCount::setCount(0);
    },
               5000);
  });
  wifiMQTT.listen(MAC::getMac() + "/c-compart-sound", [](String vile_type) {
    int vile;
    int type;
    sscanf(vile_type.c_str(), "%d_%d", &vile, &type);
    switch (type) {
      case Reference::Sound::PUT_BACK_VILE:
        AudioPlayer::play(String("/vile-put-") + vile + ".mp3");
        break;
      case Reference::Sound::COMPARTMENT_SUCCESS:
        AudioPlayer::play(String("/compartment-") + vile + ".mp3");
        break;
    }
  });
  wifiMQTT.listen(MAC::getMac() + "/play-audio", [](String audio) {
    AudioPlayer::play(String("/") + audio + ".mp3");
  });
}
#endif
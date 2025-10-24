#ifndef TAB_COUNT_H__
#define TAB_COUNT_H__
#include <output-gpio.h>
#include <my-web-server.h>
namespace TabCount {
  std::vector<OutputGPIO*> leds;
  void test() {
    for (auto led: leds) {
      led->turnOn();
      delay(500);
      led->turnOff();
    }
  }
  
  void blinkAll() {
    for (int i=0; i<5; i++) {
      TabCount::leds[i]->blink(SECONDS(.4));
    }
  }

  void setCount(int count) {
    for (int i=0; i<5; i++) {
      i < count ? TabCount::leds[i]->turnOn() : TabCount::leds[i]->turnOff();
    }
  }

  void begin() {
    TabCount::leds.push_back(new OutputGPIO(9, GPIOType::HC_595));
    TabCount::leds.push_back(new OutputGPIO(10, GPIOType::HC_595));
    TabCount::leds.push_back(new OutputGPIO(11, GPIOType::HC_595));
    TabCount::leds.push_back(new OutputGPIO(12, GPIOType::HC_595));
    TabCount::leds.push_back(new OutputGPIO(13, GPIOType::HC_595));
    // TabCount::blinkAll();
    

  }

  void beginRoutes() {
    
    webServer.get("/api/tab-count/set", [](Request* request) {
      int count = request->getParam("count")->value().toInt();
      TabCount::setCount(count);
      JSON response;
      response["status"] = "okay";
      int code = 200;
      return std::make_pair(code, response.toString());
    });
  }

};
#endif
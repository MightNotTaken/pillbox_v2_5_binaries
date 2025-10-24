#ifndef RTL_H__
#define RTL_H__
#include <serial-interpretter.h>
#include <async-core.h>

using namespace AsyncCore;
namespace RTL {
  int resetPin;
  void listenToReset(int reset) {
    RTL::resetPin = reset;
    pinMode(reset, OUTPUT);
    digitalWrite(reset, HIGH);
  }

  void reset() {
    digitalWrite(resetPin, LOW);
    setTimeout([]() {
      digitalWrite(resetPin, HIGH);
    }, 500);
  }
};
#endif
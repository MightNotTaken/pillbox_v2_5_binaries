#ifndef GPIO_H__
#define GPIO_H__
#include <vector>
#include <input-gpio.h>
#include <output-gpio.h>
#include <async-core.h>
#include <definitions.h>

using namespace AsyncCore;

namespace GPIOConfig {
std::function<void()> beforeListenCallback;
std::function<void()> afterListenCallback;
uint32_t listeningIntervalSize = 50;
struct Output {
  int pin;
  int type;
  Output(int pin, int type)
    : pin(pin), type(type) {}
};
struct InputOutputPair {
  int inputPin;
  Output* output;
  InputOutputPair(int inputPin, int outputPin, int outputType)
    : inputPin(inputPin) {
    this->output = new GPIOConfig::Output(outputPin, outputType);
  }
  ~InputOutputPair() {
    delete this->output;
  }
};

std::vector<InputGPIO*> inputs;

void registerInput(InputGPIO* input) {
  GPIOConfig::inputs.push_back(input);
}

void unregisterInput(InputGPIO* input) {
  auto it = std::find(inputs.begin(), inputs.end(), input);
  if (it != inputs.end()) {
    inputs.erase(it);
  }
}

void beforeListen(std::function<void()> callback) {
  GPIOConfig::beforeListenCallback = callback;
}

void afterListen(std::function<void()> callback) {
  GPIOConfig::afterListenCallback = callback;
}

void listen() {
  invoke(GPIOConfig::beforeListenCallback);
  for (auto& input : GPIOConfig::inputs) {
    input->listen();
  }
  invoke(GPIOConfig::afterListenCallback);
}

void begin() {
  OutputGPIO::begin();
  auto ref = setInterval([]() {
    GPIOConfig::listen();
  }, listeningIntervalSize);
  console.log("ref", ref);
}
};
#endif
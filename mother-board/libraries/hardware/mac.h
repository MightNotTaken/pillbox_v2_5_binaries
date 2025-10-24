#ifndef MAC_H__
#define MAC_H__
#include <WiFi.h>
namespace MAC {
  String mac;
  bool loaded = false;
  void load() {
    char __address[20];
    WiFi.macAddress().toCharArray(__address, 18);
    mac = String(__address);
    mac.replace(":", "");
    loaded = true;
  }

  String getMac() {
    if (!loaded) {
      load();
    }
    return mac;
  }
};
#endif
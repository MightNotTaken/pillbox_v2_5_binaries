#ifndef HTTP_H__
#define HTTP_H__
#include <serial-interpretter.h>
#include <console.h>
#include <vector>
#include <definitions.h>
#include <event-handler.h>

class HTTP: public EventHandler {
  uint32_t bytesToRead;
public:
  void get(String host = "192.168.103.145", int port = 3000, String path = "/file?file=1.mp3") {
    interCom.emit("http-set-host", host);
    interCom.emit("http-set-port", String(port));
    interCom.emit("http-get", path);
    interCom.on("http-response", [this](String bytes) {
      call("data", bytes);
    });
    interCom.on("http-end", [](String _) {
    });
  }
} httpClient;
#endif
#ifndef EVENT_HANDLER_H__
#define EVENT_HANDLER_H__
#include <functional>
#include <Arduino.h>
#include <vector>
#include <console.h>

typedef std::function<void(String)> EventCallback;

struct Event {
  String name;
  EventCallback callback;
  Event(String name, EventCallback callback)
    : name(name), callback(callback) {}

  void setCallback(EventCallback cb) {
    this->callback = cb;
  }
};

class EventHandler {
  std::vector<Event*> events;
public:
  void on(String event, EventCallback callback) {
    for (uint8_t i = 0; i < events.size(); i++) {
      if (events[i]->name == event) {
        delete events[i];
        events.erase(events.begin() + i);
        break;
      }
    }
    Event* e = new Event(event, callback);
    events.push_back(e); // Add new event
  }


  void call(String event, String data = "") {
    for (uint8_t i = 0; i < events.size(); i++) {
      if (events[i]->name == event) {
        invoke(events[i]->callback, data);
        return;
      }
    }
  }

  void unsubscribe(String event) {
    // Find and remove the event from the list
    for (uint8_t i = 0; i < events.size(); i++) {
      if (events[i]->name == event) {
        delete events[i]; // Free memory
        events.erase(events.begin() + i); // Remove from vector
        return; // Exit after removing
      }
    }
  }
};
#endif
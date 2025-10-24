#include <JSON.h>
#include <Arduino.h>
typedef enum {
  INCREASING = 0,
  DECREASING = 1,
  NEUTRAL = 2
} Direction;
struct CircularBuffer {
  int* buffer;
  int cycle;
  int index;
  int length;
  CircularBuffer(int length) {
    this->length = length;
    this->buffer = new int[length];
    this->cycle = 0;
    this->index = 0;
  }

  void push(int newValue) {
    if (this->empty()) {
      fill(newValue);
    }
    if (this->index == this->length) {
      this->cycle ++;
      this->index = 0;
    }
    this->buffer[this->index++] = newValue;
  }

  bool empty() {
    return !this->index && !this->cycle;
  }

  int evaluate() {
    long total = 0;
    for (int i=0; i<length; i++) {
      total += this->buffer[i];
    }
    return total / length;
  }

  void fill(int newValue) {
    for (int i=0; i<length; i++) {
      this->buffer[i] = newValue;
    }
  }

  int getDirection() {
    int tentativeDirection = 0;
    for (int i=0; i<length-1; i++) {
      if (buffer[i+1] < buffer[i]) {
        tentativeDirection -= 1;
      } else if (buffer[i+1] > buffer[i]) {
        tentativeDirection += 1;
      }
    }
    if (tentativeDirection > 0) {
      return Direction::INCREASING;
    } else if (tentativeDirection == 0) {
      return Direction::NEUTRAL;
    } else {
      return Direction::DECREASING;
    }
  }

  String toString() {
    String response;
    for (int i=0; i<length; i++) {
      response += String(buffer[i]);
      if (i != length -1) {
        response += ", ";
      }
    }
    return response;
  }
};
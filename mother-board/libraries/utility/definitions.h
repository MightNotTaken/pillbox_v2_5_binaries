#ifndef DEFINITIONS_H__
#define DEFINITIONS_H__
#include <TimeLib.h>
#include <Arduino.h>
#define SEPERATOR      ''
#define invoke(func, ...)  do {\
  if (func) {\
    func(__VA_ARGS__);\
  }\
} while (0)


#define var(x)         #x
#define showX(x)       Serial.println(String(var(x)) + " : " + String(x))

#define _min_(x,y)     (x>y?y:x)
long mapped(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

typedef uint32_t TimeReference;

#define SECONDS(x)    (x * 1000L)
#define MINUTES(x)    (x * SECONDS(60))
#define HOURS(x)      (x * MINUTES(60))
#define DAYS(x)       (X * HOURS(24))

String formatDays(int days, uint8_t hours) {
  return String(days) + "d " + hours + "h"; 
}

String formatHours(uint16_t hours, uint8_t minutes, uint8_t seconds) {
  if (hours < 24) {
    return String(hours) + ":" + minutes + ":" + seconds;
  }
  return formatDays(hours / 24.0, hours % 24);
}

String formatMinutes(uint16_t minutes, uint8_t seconds) {
  if (minutes < 60) {
    return String(minutes) + "m " + seconds + "s";
  }
  return formatHours(minutes / 60.0, minutes % 60, seconds);
}

String formatSeconds(uint32_t seconds, uint16_t ms) {
  if (seconds < 60) {
    return String(seconds) + "s " + ms + "ms";
  }
  return formatMinutes(seconds / 60.0, seconds % 60);
}

String formatMillis(uint32_t ms) {
  if (ms < 1000) {
    return String(ms) + "ms";
  }
  return formatSeconds(ms / 1000.0, ms % 1000);
}

String formatMicros(uint32_t us) {
  if (us < 1E3) {
    return String(us) + "us";
  }
  return formatMillis(us / 1000.0);
}

String getTimeStampFromMS(uint32_t input) {
  input /= 1000;
  uint8_t s = input % 60;
  input /= 60;
  uint8_t m = input % 60;
  input /= 60;
  char timeStamp[10];
  sprintf(timeStamp, "%02d:%02d:%02d", input % 24, m, s);
  return timeStamp;
}

uint32_t getCurrentTimeMS() {
  tmElements_t timeInfo;
  breakTime(now(), timeInfo);  
  return HOURS(timeInfo.Hour) + MINUTES(timeInfo.Minute) + SECONDS(timeInfo.Second);
}

uint32_t getMSFromTimeStamp(String timestamp) {
  uint8_t h;
  uint8_t m;
  uint8_t s;
  sscanf(timestamp.c_str(), "%d:%d:%d", &h, &m, &s);
  return HOURS(h) + MINUTES(m) + SECONDS(s);
}

uint32_t getTimeStampDifference(String to, String from = "") {
  uint32_t fromMS;
  if (!from.length()) {
    fromMS = getCurrentTimeMS();
  } else {
    fromMS = getMSFromTimeStamp(from);
  }
  uint32_t toMS = getMSFromTimeStamp(to);
  if (fromMS > toMS) {
    toMS += HOURS(24);
  }
  return toMS - fromMS;
}


String getTimeStamp() {
  char stamp[10];
  tmElements_t timeInfo;
  breakTime(now(), timeInfo);
  sprintf(stamp, "%02d:%02d:%02d", timeInfo.Hour, timeInfo.Minute, timeInfo.Second);
  return stamp;
}

String getDateStamp() {
  char stamp[12];
  tmElements_t timeInfo;
  breakTime(now(), timeInfo);
  sprintf(stamp, "%04d-%02d-%02d", timeInfo.Year + 1970, timeInfo.Month, timeInfo.Day);
  return stamp;
}

String getTimeDateStamp() {
  return getTimeStamp() + " " + getDateStamp();
}

String toLowerCase(String str) {
  str.toLowerCase();
  return str;
}

float matchStrings(String str1, String str2) {
    str1.toLowerCase();
    str2.toLowerCase();

    str1.replace(" ", "");
    str1.replace("-", "");
    str1.replace("_", "");
    str2.replace(" ", "");
    str2.replace("-", "");
    str2.replace("_", "");

    int len1 = str1.length();
    int len2 = str2.length();

    int charCount = 0;
    for (int i = 0; i < len1; i++) {
        if (str2.indexOf(str1[i]) != -1) {
            charCount++;
        }
    }
    return (float)charCount / len1 * 100;
}

String getDateTimeStamp() {
  return getDateStamp() + " " + getTimeStamp();
}

String nowToStamp(uint32_t _now) {
  tmElements_t timeInfo;
  char date[14];
  char time[14];
  breakTime(_now, timeInfo);
  sprintf(date, "%04d-%02d-%02d", timeInfo.Year + 1970, timeInfo.Month, timeInfo.Day);
  sprintf(time, "%02d:%02d:%02d", timeInfo.Hour, timeInfo.Minute, timeInfo.Second);
  return String(date) + " " + time;
}


uint32_t stampToNow(String stamp) {
  int year, month, day, hour, minute, second;
  sscanf(stamp.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

  year -= 1970;

  tmElements_t timeinfo;
  timeinfo.Year = year;
  timeinfo.Month = month;
  timeinfo.Day = day;
  timeinfo.Hour = hour;
  timeinfo.Minute = minute;
  timeinfo.Second = second;

  uint32_t _now = makeTime(timeinfo);

  return _now;
}

int binarySearchNearest(const int arr[], int size, int target) {
  int left = 0;
  int right = size - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    // Check if the target is at mid
    if (arr[mid] == target) {
      return mid;
    }

    // If the target is greater, ignore the left half
    if (arr[mid] < target) {
      left = mid + 1;
    }
    // If the target is smaller, ignore the right half
    else {
      right = mid - 1;
    }
  }

  // If we exit the loop, left and right have crossed over
  // Determine the closest index
  if (right < 0) {
    return left; // target is less than the smallest element
  }
  if (left >= size) {
    return right; // target is greater than the largest element
  }
  // Find the nearest index between left and right
  if ((arr[left] - target) < (target - arr[right])) {
    return left;
  }
  return right;
}
#endif
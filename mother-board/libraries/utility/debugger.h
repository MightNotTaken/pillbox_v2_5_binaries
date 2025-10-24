#ifndef DEBUGGER_H__
#define DEBUGGER_H__
#include <vector>
#include <initializer_list>

namespace Debugger {
  int width = 50;
  typedef std::vector<String> table_row_t;
  typedef std::vector<table_row_t> table_t;
  template <typename T>
  String forS(T input, int width, char fill = ' ') {
    String inp(input);
    if (inp.length() > width) {
      inp = inp.substring(0, width - 2) + "..";
      return inp.c_str();
    }
    if (inp.length() == width) {
      return inp.c_str();
    }  
    int margin = (width - inp.length()) / 2;
    for (int i=0; i<margin; i++) {
      inp += fill;
    }
    while (inp.length() < width) {
      inp = String(fill) + inp;
    }
    return inp;
  }

  void setScreenWidth(int width) {
    Debugger::width = width;
  }

  void line(char fill = '-', int width = -1) {
    if (width < 0) {
      width = Debugger::width;
    }
    Serial.println(Debugger::forS('-', width, '-'));
  }

  std::vector<int> cellWidths;

  
  void display(std::vector<String> values) {
    uint8_t i = 0;
    Serial.print('|');
    for (const auto & value: values) {
      Serial.print(Debugger::forS(value, Debugger::cellWidths[i++]));
      Serial.print('|');
    }
    Serial.println();
  }

  template <typename T>
  void display(std::initializer_list<T> values)  {
    std::vector<String> stringList;
    for (auto& item: values) {
      stringList.push_back(item);
    }
    Debugger::display(stringList);
  }

  void resetCellWidths(uint8_t newCells) {
    Debugger::cellWidths.clear();
    for (int i=0; i<newCells; i++) {
      Debugger::cellWidths.push_back(0);
    }
    Debugger::width = 0;
  }
  void calculateCellWidths(std::vector<String> list) {
    uint8_t i = 0;
    uint16_t fullWidth = 1;
    bool changed = false;
    for (auto& item: list) {
      if (Debugger::cellWidths[i] < String(item).length()) {
        Debugger::cellWidths[i] = String(item).length() + 4;
      }
      fullWidth += Debugger::cellWidths[i++] + 1;
    }
    if (fullWidth > Debugger::width) {
      Debugger::width = fullWidth;
    }
  }

  template<typename T>
  void calculateCellWidths(std::initializer_list<T> list) {
    std::vector<String> stringList;
    for (auto& item: list) {
      stringList.push_back(item);
    }
    Debugger::calculateCellWidths(stringList);
  }

  void display(String singleItem) {
    Debugger::line('-');
    Serial.print('|');
    Serial.print(Debugger::forS(singleItem, Debugger::width - 2));
    Serial.println('|');
    Debugger::line('-');
  }
  
  template <typename T>
  void displayTable(
    String heading,
    std::initializer_list<T> columnHeadings,
    table_t table
  ) {
    Debugger::resetCellWidths(columnHeadings.size());
    Debugger::calculateCellWidths(columnHeadings);
    for (auto& row: table) {
      Debugger::calculateCellWidths(row);
    }
    Serial.println();
    Debugger::display(heading);
    Debugger::display(columnHeadings);
    Debugger::line('-');
    for (const auto& row: table) {
      Debugger::display(row);
    }
    Debugger::line('-');
    Serial.println();
  }
  
};
#endif
#ifndef INCLUDE_CSV_PRINT_HPP_
#define INCLUDE_CSV_PRINT_HPP_

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

#include <csv/constants.hpp>
#include <csv/error.hpp>
#include <csv/match.hpp>

namespace csv {

  class Linescan_Printer { // LCOV_EXCL_START
  public:
    virtual void print(const Linescan& sc_result) const = 0;
    virtual ~Linescan_Printer(){};
  }; // LCOV_EXCL_STOP

  class Linescan_Line_Printer : public Linescan_Printer { // LCOV_EXCL_START
  public:
    void print(const Linescan& sc_result) const override;
  }; // LCOV_EXCL_STOP

  class Linescan_Field_Printer : public Linescan_Printer { // LCOV_EXCL_START
  private:
    const std::vector<size_t> _fields;
    const char _delimiter;

  public:
    void print(const Linescan& sc_result) const override;
    Linescan_Field_Printer(std::vector<size_t> fields, char delimiter) :
      _fields {fields}, _delimiter {delimiter} {};   
  }; // LCOV_EXCL_STOP

}

#endif

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

  // LCOV_EXCL_START
  inline void print(const char* buf, size_t length) {
    fwrite(buf, sizeof(char), length, stdout);
  }
  // LCOV_EXCL_END
  

  // LCOV_EXCL_START
  inline void print_field(const char* buf,
			  const std::vector<size_t>& offsets,
			  size_t field_idx){
    size_t next_field_idx = field_idx + 1;
    assert(offsets.size() > next_field_idx);
    size_t offset = offsets[field_idx];
    size_t length = offsets[next_field_idx] - offset - 1;
    csv::print(buf + offset, length);
  }
  // LCOV_EXCL_END

  class Field_Printer {
  private:
    const std::vector<size_t> _fields;
    const char _delimiter;
    const bool _crnl;
    const bool _nl;
    const bool _cont;
    
  public:
    void print(const char* buf,
	       const std::vector<size_t>& offsets) const;
    void print(const std::vector<std::string>& fields) const;
    Field_Printer(std::vector<size_t> fields,
		  char delimiter,
		  bool crnl,
		  bool nl,
		  bool cont) :
      _fields {fields}, _delimiter {delimiter}, _crnl {crnl},
      _nl {nl}, _cont {cont} {};
  };

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
    const std::unique_ptr<csv::Field_Printer> _printer;

  public:
    void print(const Linescan& sc_result) const override;
    Linescan_Field_Printer(std::unique_ptr<csv::Field_Printer> printer) :
      _printer {std::move(printer)} {};   
  }; // LCOV_EXCL_STOP

}

#endif

#include <csv/print.hpp>

using namespace std;
using namespace csv;


// LCOV_EXCL_START
void csv::Field_Printer::print(const char* buf,
			       const std::vector<size_t>& offsets) const {
  size_t fields_n = _fields.size() - 1;
  for(size_t i=0;i<fields_n;i++){
    csv::print_field(buf, offsets, _fields[i]);
    putc(_delimiter,stdout);
  }
  csv::print_field(buf, offsets, _fields[fields_n]);
  if(_crnl){
    putc('\r',stdout);
  }
  putc('\n',stdout);
}
// LCOV_EXCL_END

void csv::Linescan_Line_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  csv::print(sc_result.begin(), sc_result.length()-1);
  putc('\n',stdout);
}
// LCOV_EXCL_STOP

void csv::Linescan_Field_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  _printer->print(sc_result.begin(), sc_result.offsets());
}
// LCOV_EXCL_STOP

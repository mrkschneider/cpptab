#include <csv/print.hpp>

using namespace std;
using namespace csv;

void csv::Linescan_Line_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  fwrite(sc_result.begin(), sizeof(char), sc_result.length()-1, stdout);
  putc('\n',stdout);
}
// LCOV_EXCL_STOP

void csv::Linescan_Field_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  size_t fields_n = _fields.size();
  for(size_t i=0;i<fields_n;i++){
    const size_t& field = _fields[i];
    fwrite(sc_result.field(field), sizeof(char), sc_result.field_size(field), stdout);	
    if(i<fields_n-1)
      putc(_delimiter,stdout);
  }
  if(sc_result.crnl()){
    putc('\r',stdout);
  }
  putc('\n',stdout);
}
// LCOV_EXCL_STOP

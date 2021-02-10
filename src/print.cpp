#include <csv/print.hpp>

using namespace std;
using namespace st;
using namespace csv;


// LCOV_EXCL_START
void csv::Field_Printer::print(const char* buf,
			       const std::vector<size_t>& offsets) const {
  if(_fields.size() > 0) {
    if(_cont) putc(_delimiter,stdout);
    size_t fields_n = _fields.size() - 1;
    for(size_t i=0;i<fields_n;i++){
      size_t field = _fields[i];
      csv::print_field(buf, offsets, field);
      putc(_delimiter,stdout);
    }
    csv::print_field(buf, offsets, _fields[fields_n]);
  }
  if(_crnl){
    putc('\r',stdout);
  }
  if(_nl){
    putc('\n',stdout);
  }
}
// LCOV_EXCL_END

void csv::Field_Printer::print(const std::vector<std::string>& fields) const {
  auto print = [this, &fields](size_t field_idx)
	       {
		 if(field_idx < fields.size()) {
		   const string& field = fields[field_idx];
		   csv::print(field.c_str(),field.length());
		 } else if(_allow_out_of_bounds) {
		   // Do nothing
		 }
		 else {
		   throw std::runtime_error("Field print out of bounds");
		 }
	       };
  
  if(_fields.size() > 0) {
    if(_cont) putc(_delimiter,stdout);
    size_t fields_n = _fields.size() - 1;
    for(size_t i=0;i<fields_n;i++){
      size_t field_idx = _fields[i];
      print(field_idx);
      putc(_delimiter,stdout);
    }
    size_t field_idx = _fields[fields_n];
    print(field_idx);
  }

  if(_crnl){
    putc('\r',stdout);
  }
  if(_nl){
    putc('\n',stdout);
  }
}

void csv::Linescan_Line_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  csv::print(sc_result.begin(), sc_result.length()-1);
  putc('\n',stdout);
}
// LCOV_EXCL_STOP

void csv::Linescan_Field_Printer::print(const Linescan& sc_result) const { // LCOV_EXCL_START
  _printer->print(sc_result.begin(), sc_result.offsets());
}
// LCOV_EXCL_STOP

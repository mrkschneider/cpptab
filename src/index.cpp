#include <iostream>
#include <fstream>

#include <csv/index.hpp>
#include <csv/st.hpp>

using namespace std;
using namespace st;
using namespace csv;

unique_ptr<Index> Index::create(const std::string& csv_path, char delimiter,
				size_t read_size, size_t buffer_size,
				size_t offsets_size){
  Circbuf cbuf(csv_path, read_size, buffer_size);
  FILE* fd = fopen(csv_path.c_str(),"r");
  unique_ptr<Index> r = make_unique<Index>(fd, read_size);
  Linescan lscan(delimiter, offsets_size);
  lscan.do_scan_header(cbuf.head(), read_size);
  cbuf.advance_head(lscan.length());
  
  for(size_t i=0;i<lscan.n_fields();i++){
    string column = string(lscan.field(i),lscan.field_size(i));
    r->_column_keys[column] = i;    
  }

  size_t len = lscan.length();
  size_t len_sum = len;
  r->_line_offsets.push_back(len_sum);

  while(!cbuf.at_eof()){
    lscan.do_scan(cbuf.head(), read_size);
    r->_offsetss.push_back(lscan.offsets());
    len = lscan.length();
    len_sum += len;
    r->_line_offsets.push_back(len_sum);
    cbuf.advance_head(len);
  }
  return r;
}


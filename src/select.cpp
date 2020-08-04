#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iterator>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <typeinfo>
#include <select.hpp>
#include "../include/st.hpp"

using namespace std;
using namespace st;
using namespace csv;

string csv::Line_Scan::str(){
  ostringstream s;
  s << "Begin: " << str_ptr(_begin)					
    << "\nLength: " << _length						
    << "\nMatch field: " << _match_field
    << "\nField count: " << _n_fields
    << "\nDelimiter offsets: " << str_vector(_delimiter_offsets);
  return s.str();
}

void csv::scan(const char* b, uint n,
	       char target,
	       char delimiter,
	       Line_Scan& r){
  r.reset();
  
  char* nl_left = simple_scan_left(b,n,'\n');
  if(nl_left==nullptr) throw runtime_error("Could not find left newline");
  r._begin = nl_left;

  //fprintf(stderr,"%p\n",nl_left);
  
  char* nl_right = simple_scan_right(b,n,'\n');

  //fprintf(stderr,"%p\n",nl_right);
  if(nl_right==nullptr) throw runtime_error("Could not find right newline");
  r._length = nl_right - nl_left;
  {
    const char* dl = b;
    int nd = dl-nl_left;
    while(true){
      dl = simple_scan_left(dl,nd,delimiter);
      if(dl > nl_left) {
	nd = dl-nl_left;
	r._delimiter_offsets.push_back(nd);
      }
      else
	break;
    }
    r._match_field = r._delimiter_offsets.size();
    r._delimiter_offsets.push_back(0);
    reverse(r._delimiter_offsets.begin(), r._delimiter_offsets.end());
  }

  {
    const char* dr = b;
    r._delimiter_offsets.push_back(dr-nl_left);
    int nd = nl_right - dr;
    while(true){
      dr = simple_scan_right(dr+1,nd,delimiter);
      if(dr==NULL) break;
      else if(dr < nl_right) {
	nd = nl_right - dr;
	r._delimiter_offsets.push_back(dr - nl_left);
      }
      else
	break;
    }
    r._delimiter_offsets.push_back(r._length);
  }
  r._n_fields = r._delimiter_offsets.size()-2;
  return;
}

void csv::print_line(const Line_Scan& sc_result){
  fwrite(sc_result.begin()+1, sizeof(char), sc_result.length(), stdout);
}

void csv::print_fields(const Line_Scan& sc_result, const vector<size_t>& print_fields){
    size_t fields_n = print_fields.size();
    for(size_t i=0;i<fields_n;i++){
      size_t field = print_fields[i];
      char* b = sc_result.begin();
      size_t begin_offset = sc_result.delimiter_offsets()[field] + 1;
      size_t end_offset = sc_result.delimiter_offsets()[field+1];
      size_t n = end_offset - begin_offset;
      fwrite(b+begin_offset, sizeof(char), n, stdout);	
      if(i<fields_n-1)
	putc(',',stdout);
    }
    putc('\n',stdout);
}

void csv::print_header(circbuf* c, Line_Scan& sc_result){
  size_t read_size = c->read_size;
  char* head = simple_scan_right(circbuf_head(c),read_size,',');
  if(head==nullptr) throw runtime_error("Could not identify header");
  scan(head,read_size,'\n',',',sc_result);

  csv::print_line(sc_result);
  return;
}

bool csv::match_print_line(circbuf* c, 
			   const boost::regex& pattern,
			   boost::cmatch& match_result,
			   size_t pattern_field,
			   Line_Scan& sc_result){
  const char* head = circbuf_head(c);
  size_t read_size = c->read_size;
  bool match = boost::regex_search(head,head+read_size,match_result,pattern);
  
  if(match){
    long pos = match_result.position();
    long size = match_result.length();
    long new_pos = pos+size;

    head = circbuf_head_forward(c,new_pos);
  
    scan(head,read_size,'\n',',',sc_result);

    bool print = sc_result.match_field() == pattern_field;
    //fprintf(stderr,"%s",str_vector(sc_result.delimiter_offsets()).c_str());
    if(print) {  
      csv::print_line(sc_result);
    }
  } else if(!c->finished){
    char* del_left = simple_scan_left(head+read_size,read_size,',');
    head = circbuf_head_forward(c,del_left-head);
  } else {
    head = circbuf_head_forward(c,read_size);
  }
  
  return match;
}



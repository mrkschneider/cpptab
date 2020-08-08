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

bool csv::Regex_Matcher::search(const char* begin, size_t n){
  bool match = boost::regex_search(begin,begin+n,_match_result,_pattern,
				   csv::REGEX_MATCH_FLAGS);
  return match;
}

bool csv::Boyer_Moore_Matcher::search(const char* begin, size_t n){
  const char* end = begin + n;
  boost::algorithm::boyer_moore<const char*> matcher = *_matcher;
  pair<const char*,const char*> r = matcher(begin,end);
  _position = r.first - begin;
  return r.first != r.second;
}

string csv::Line_Scan::str() const {
  ostringstream s;
  s << "Begin: " << str_ptr(_begin)					
    << "\nLength: " << _length						
    << "\nMatch field: " << _match_field
    << "\nField count: " << _n_fields
    << "\nDelimiter offsets: " << str_vector(_field_offsets);
  return s.str();
}

const char* csv::Line_Scan::field(size_t idx) const {
  return _begin + _field_offsets[idx];
}

size_t csv::Line_Scan::field_size(size_t idx) const {
  return _field_offsets[idx+1] - _field_offsets[idx] - 1; 
}

void csv::scan(const char* b, uint n,
	       char target,
	       char delimiter,
	       linescan* lscan,
	       Line_Scan& r
	       ){
  r.reset();

  uint64_t cmask = linescan_create_mask(delimiter);

  const char* nl_left;
  {
    int rc = linescan_rfind(b-n,cmask,n,lscan);
    
    if(rc < 1) throw runtime_error("Could not find left newline. Maybe --read-size is too small.");
    size_t offset = lscan->offsets[lscan->offsets_n-1];
  
    nl_left = lscan->begin + offset;
    for(int i=lscan->offsets_n-1;i>=1;i--){
      r._field_offsets.push_back(lscan->offsets[i]-offset);    
    }

    r._begin = nl_left + 1;
    r._match_field = r._field_offsets.size() - 1;
  }

  const char* nl_right;
  {
    int rc = linescan_find(b,cmask,n,lscan); 

    if(rc > 0) {
        nl_right = lscan->begin + lscan->size - 1;
    } else if(rc == 0) {
      char* nl = simple_scan_right(b,n,EOF);
      if(nl==nullptr) throw runtime_error("Could not find right newline. Maybe --read-size is too small");
      nl[0] = NL;
      nl_right = nl;
      lscan->offsets[lscan->offsets_n] = nl_right - b;
      lscan->offsets_n++;
    } else {
      throw runtime_error("Could not find right newline. Maybe --read-size is too small");
    } 
    r._length = nl_right - nl_left;

    size_t offset = b - nl_left;
    for(size_t i=1;i<lscan->offsets_n-1;i++){
      r._field_offsets.push_back(lscan->offsets[i]+offset);    
    }
  }

  r._n_fields = r._field_offsets.size()-2;

  return;
}

void csv::print_line(const Line_Scan& sc_result){
  fwrite(sc_result.begin(), sizeof(char), sc_result.length(), stdout);
}

void csv::print_fields(const Line_Scan& sc_result, char delimiter,
		       const vector<size_t>& print_fields){
    size_t fields_n = print_fields.size();
    for(size_t i=0;i<fields_n;i++){
      size_t field = print_fields[i];
      fwrite(sc_result.field(field), sizeof(char), sc_result.field_size(field), stdout);	
      if(i<fields_n-1)
	putc(delimiter,stdout);
    }
    putc('\n',stdout);
}

void csv::scan_header(circbuf* c, char delimiter, linescan* l_r, Line_Scan& sc_result){
  size_t read_size = c->read_size;
  char* head = simple_scan_right(circbuf_head(c),read_size,delimiter);
  if(head==nullptr) throw runtime_error("Could not find delimiter in header. Maybe --read-size is too small");
  scan(head,read_size,NL,delimiter,l_r,sc_result);
  return;
}

void csv::scan_match_print_line(circbuf* c, 
				char delimiter,
				Matcher& matcher,
				size_t pattern_field,
				bool complete_match,
				linescan* l_r,
				Line_Scan& sc_result,
				const Line_Scan_Printer& printer){
  const char* head = circbuf_head(c);
  size_t read_size = c->read_size;

  bool match = matcher.search(head,read_size);
  
  if(match){
    /* Regex matches in the buffer. 
       Now we need to find out whether the match is located in the correct column. */
    long match_end = matcher.position() + matcher.size();
    // Move to end of regex match
    head = circbuf_head_forward(c,match_end);
    // Find delimiters and surrounding newlines
    scan(head,read_size,NL,delimiter,l_r,sc_result);
    size_t match_field = sc_result.match_field();

    bool is_complete = (!complete_match) ||
      (delimiter == head[0] &&
       delimiter == head[-matcher.size()-1]);

    if(match_field == pattern_field){
      // Match is in expected column.
      if(is_complete)
	printer.print(sc_result);
      // Print and move to next newline.
      circbuf_head_forward(c,sc_result.begin() + sc_result.length() - head);
      return;
    } else if(match_field < pattern_field){
      // Match is to the left of expected column. Cannot move further for now.
      return;
    } else {
      // Match is to the right of expected column. Moving to next newline is safe. 
      circbuf_head_forward(c,sc_result.begin() + sc_result.length() - head);
      return;
    }
  } else if(!c->finished) {
    /* No match in buffer. It is possible that there is an 
       incomplete match at the end. To avoid missing a potential match,
       we skip only to the last delimiter in the buffer. This is only
       relevant as long as the circbuffer is not finished.
    */  
    char* del_left = simple_scan_left(head+read_size,read_size,delimiter);
    circbuf_head_forward(c,del_left - head);
    return;
  } else {
    /* Partial matches at the end of the buffer are impossible once 
       circbuffer is finished. This branch makes sure that the circbuffer head
       eventually reaches EOF.
     */
    circbuf_head_forward(c,read_size);
    return;
  }
}

void csv::Line_Printer::print(const Line_Scan& sc_result) const {
  print_line(sc_result);
}

void csv::Field_Printer::print(const Line_Scan& sc_result) const {
  print_fields(sc_result,_delimiter,_fields);
}

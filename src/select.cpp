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


const char* csv::Linescan::field(size_t idx) const {
  #ifdef CSV_DEBUG
  if(idx > _n_fields - 1)
    throw runtime_error("Array index too large");
  #endif
  return _begin + _offsets[idx];  
}

size_t csv::Linescan::field_size(size_t idx) const {
  #ifdef CSV_DEBUG
  if(idx > _n_fields - 1)
    throw runtime_error("Array index too large");
  #endif
  return _offsets[idx+1] - _offsets[idx] - 1;
}

string csv::Linescan::str() const {
  ostringstream s;
  s << "Begin: " << str_ptr(_begin)					
    << "\nLength: " << _length
    << "\nMatch field: " << _match_field
    << "\nField count: " << _n_fields
    << "\nDelimiter offsets: " << str_vector(_offsets);
  return s.str();
}

void csv::scan(const char* b, uint n,
	       char delimiter,
	       Linescan& r){
  r.reset();
  linescan* lscan = r._lscan;

  uint64_t cmask = linescan_create_mask(delimiter);

  const char* nl_left;
  {
    int rc = linescan_rfind(b-n,cmask,n,lscan);
    
    if(rc < 1) throw runtime_error("Could not find left newline. Maybe --read-size is too small.");
    size_t offset = lscan->offsets[lscan->offsets_n-1];
  
    nl_left = lscan->buf + offset;
    for(int i=lscan->offsets_n-1;i>=1;i--){
      r._offsets.push_back(lscan->offsets[i]-offset);    
    }

    r._begin = nl_left + 1;
    r._match_field = r._offsets.size() - 1;
  }

  const char* nl_right;
  {
    int rc = linescan_find(b,cmask,n,lscan); 

    if(rc > 0) {
        nl_right = lscan->buf + lscan->size - 1;
    } else if(rc == 0) {
      char* nl = simple_scan_right(b,n,'\0');
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
    for(size_t i=1;i<lscan->offsets_n;i++){
      r._offsets.push_back(lscan->offsets[i]+offset);    
    }
  }

  r._n_fields = r._offsets.size() - 1;

  return;
}

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

void csv::print_line(const Linescan& sc_result){
  fwrite(sc_result.begin(), sizeof(char), sc_result.length()-1, stdout);
  putc('\n',stdout);
}

void csv::print_fields(const Linescan& sc_result, char delimiter,
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

void csv::scan_header(circbuf* c, char delimiter, Linescan& lscan){
  size_t read_size = c->read_size;
  char* head = simple_scan_right(circbuf_head(c),read_size,delimiter);
  if(head==nullptr) throw runtime_error("Could not find delimiter in header. Maybe --read-size is too small");
  scan(head,read_size,delimiter,lscan);
  return;
}

bool csv::Multiline_BMatcher::search(circbuf* c, Matcher& matcher, Linescan& result){
  const char* head = circbuf_head_forward(c,_advance_next);
  if(head[0] == '\0') return false;
  size_t read_size = c->read_size;

  bool match = matcher.search(head,read_size);
  
  if(match){
    /* Regex matches in the buffer. 
       Now we need to find out whether the match is located in the correct column. */
    size_t match_end_offset = matcher.position() + matcher.size();
        
    // Move to end of regex match
    head = circbuf_head_forward(c,match_end_offset);

    char* match_delimiter = simple_scan_left(head, matcher.size(),
					      _delimiter);
    if(match_delimiter != NULL)
      throw runtime_error("Malformed input pattern: Matches delimiter");
    // Find delimiters and surrounding newlines
    scan(head,read_size,_delimiter,result);

    char end = head[0];
    char start = head[-matcher.size()-1];
    bool is_complete = (!_complete_match) ||
      ((_delimiter == start || NL == start)  &&
       (_delimiter == end || NL == end));
    size_t match_field = result.match_field();
    
    if(match_field == _pattern_field){
      // Match is in expected column.
      // Move to next newline.
      _advance_next = result.begin() + result.length() - head;
      return is_complete;
    } else if(match_field < _pattern_field){
      // Match is too far left. Move to begin of expected column.
      const char* new_pos = result.field(_pattern_field);
      /* new_pos should always be bigger than head, so this is just a safety to catch
	 pathological cases. */
      assert(new_pos > head);
      _advance_next = new_pos - head;
      return false;
    } else {
      // Match is to the right of expected column. Moving to next newline is safe. 
      _advance_next = result.begin() + result.length() - head;
      return false;
    }
  } else if(!c->finished) {
    /* No match in buffer. It is possible that there is an 
       incomplete match at the end. To avoid missing a potential match,
       we skip only to the last delimiter in the buffer. This is only
       relevant as long as the circbuffer is not finished.
    */  
    char* del_left = simple_scan_left(head+read_size,read_size,_delimiter);
    _advance_next = del_left - head;
    return false;
  } else {
    /* Partial matches at the end of the buffer are impossible once 
       circbuffer is finished. This branch makes sure that the circbuffer head
       eventually reaches the end.
     */
    _advance_next = read_size;
    return false;
  }

  throw runtime_error("Unreachable code");
}

bool csv::Singleline_BMatcher::search(circbuf* c, Matcher& matcher,
				      Linescan& result){
  size_t read_size = c->read_size;
  const char* head = circbuf_head_forward(c,_advance_next);
  if(head[0] == '\0') return false;
  scan(head,read_size,_delimiter,result);

  const char* match_field = result.field(_pattern_field);
  size_t match_field_size = result.field_size(_pattern_field);

  bool match = matcher.search(match_field, match_field_size);
  bool is_complete = !(_complete_match) || match_field_size == matcher.size();

  _advance_next = result.length();
  return match && is_complete;  
}

void csv::Line_Printer::print(const Linescan& sc_result) const {
  print_line(sc_result);
}

void csv::Field_Printer::print(const Linescan& sc_result) const {
  print_fields(sc_result,_delimiter,_fields);
}

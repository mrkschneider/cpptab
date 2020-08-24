#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iterator>
#include <iostream>
#include <typeinfo>

#include <csv/st.hpp>
#include <csv/match.hpp>

using namespace std;
using namespace st;
using namespace csv;

const char* csv::Linescan::field(size_t idx) const {
  if(idx >= _n_fields) return nullptr;
  return _begin + _offsets[idx];  
}

size_t csv::Linescan::field_size(size_t idx) const {
  if(idx >= _n_fields) return 0;
  return _offsets[idx+1] - _offsets[idx] - 1;
}

string csv::Linescan::str() const {
  ostringstream s;
  s << "Begin: " << str_ptr(_begin) // lcov bug; LCOV_EXCL_LINE 				
    << "\nLength: " << _length
    << "\nMatch field: " << _match_field
    << "\nField count: " << _n_fields
    << "\nDelimiter offsets: " << str_vector(_offsets)
    << "\nCRNL mode: " << _crnl;
  return s.str();
}

void csv::Linescan::do_scan(const char* b, size_t n){
  this->reset();

  const char* nl_left;
  {
    int rc = linescan_rfind(b-n,_delimiter_mask,n,_lscan);
    
    if(rc < 1) throw runtime_error("Could not find left newline. Maybe --read-size is too small.");
    size_t offset = _lscan->offsets[_lscan->offsets_n-1];
  
    nl_left = _lscan->buf + offset;
    for(int i=_lscan->offsets_n-1;i>=1;i--){
      _offsets.push_back(_lscan->offsets[i]-offset);    
    }

    _begin = nl_left + 1;
    _match_field = _offsets.size() - 1;
  }

  const char* nl_right;
  {
    int rc = linescan_find(b,_delimiter_mask,n,_lscan); 

    if(rc > 0) {
      nl_right = _lscan->buf + _lscan->size - 1;
    } else if(rc == 0) {
      char* nl = simple_scan_right(b,n,'\0');
      if(nl==nullptr) throw runtime_error("Could not find right newline. Maybe --read-size is too small");
      nl[0] = NL;
      nl_right = nl;
      _lscan->offsets[_lscan->offsets_n] = nl_right - b;
      _lscan->offsets_n++;
    } else { // LCOV_EXCL_START
      // rc < 0 is reserved for general errors
      throw runtime_error("Linescan: Unknown error."); 
    } // LCOV_EXCL_STOP
    _length = nl_right - nl_left;

    size_t offset = b - nl_left;
    for(size_t i=1;i<_lscan->offsets_n;i++){
      _offsets.push_back(_lscan->offsets[i]+offset);    
    }

    if(rc > 0 && _crnl) this->adjust_for_crnl();
  }

  assert(_lscan->offsets_n <= _offsets_size);

  _n_fields = _offsets.size() - 1;
  return;
}

void csv::Linescan::do_scan_header(const char* buf, size_t n){
  this->reset();
  
  if(buf[-1] != '\n') throw runtime_error("No left newline found in header");

  this->do_scan(buf,n);

  if((this->begin() + this->length()-2)[0] == '\r'){
    this->set_crnl(true);
    this->adjust_for_crnl();
  }
  return;
}

bool csv::Regex_Matcher::do_search(const char* begin, size_t n){
  bool match = boost::regex_search(begin,begin+n,_match_result,_pattern,
				   csv::REGEX_MATCH_FLAGS);
  return match;
}

bool csv::Onig_Regex_Matcher::_initialized = false;

bool csv::Onig_Regex_Matcher::do_search(const char* begin, size_t n){
  int match = onig_search_with_param(_pattern,
				     (const OnigUChar*) begin,
				     (const OnigUChar*) begin+n,
				     (const OnigUChar*) begin,
				     (const OnigUChar*) begin+n,
				     _match_result,
				     CSV_ONIG_MATCH_FLAGS,
				     _match_param
				     );
  if(match == ONIG_MISMATCH) {
    return false;
  } else if(match < 0) { // Oniguruma error. Don't know how to test this. LCOV_EXCL_START
    OnigErrorInfo einfo;
    char s[ONIG_MAX_ERROR_MESSAGE_LEN];
    onig_error_code_to_str((UChar*)s, match, &einfo);
    throw runtime_error(s);
  } // LCOV_EXCL_STOP
  return true;
}

bool csv::Boyer_Moore_Matcher::do_search(const char* begin, size_t n){
  const char* end = begin + n;
  boost::algorithm::boyer_moore<const char*>& matcher = *_matcher;
  pair<const char*,const char*> r = matcher(begin,end);
  _position = r.first - begin;
  return r.first != r.second;
}

bool csv::Multiline_BMatcher::do_search(Circbuf& c, Linescan& result){
  const char* head = c.advance_head(_advance_next);
  size_t read_size = c.read_size();

  bool match = _matcher->do_search(head,read_size);

  if(match){
    /* Regex matches in the buffer. 
       Now we need to find out whether the match is located in the correct column. */
    size_t match_end_offset = _matcher->position() + _matcher->size();
        
    // Move to end of regex match
    head = c.advance_head(match_end_offset);

    char* match_delimiter = simple_scan_left(head, _matcher->size(),
					      _delimiter);
    if(match_delimiter != NULL)
      throw runtime_error("Malformed input pattern: Matches delimiter");
    // Find delimiters and surrounding newlines
    result.do_scan(head,read_size);

    size_t match_field = result.match_field();
    
    if(match_field == _pattern_field){
      // Match is in expected column.
      // Move to next newline.
      _advance_next = result.begin() + result.length() - head;
      bool is_complete = (!(_complete_match)) ||
	_matcher->size() == result.field_size(match_field); 
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
  }
  // No match
  else if(c.finished() && (head + read_size - 1)[0] == '\0') {
    /* Partial matches at the end of the buffer are impossible now. 
       This branch makes sure that the circbuffer head eventually reaches the end.
     */
    _advance_next = read_size;
    return false;
  } else {
    /* No match in buffer. It is possible that there is an 
       incomplete match at the end. To avoid missing a potential match,
       we skip only to the last delimiter in the buffer. This is only
       relevant as long as the circbuffer is not finished.
    */  
    char* del_left = simple_scan_left(head+read_size,read_size,_delimiter);
    _advance_next = del_left - head;
    return false;
  }

  throw runtime_error("Unreachable code");
}

bool csv::Singleline_BMatcher::match(const Linescan& lscan){
  const char* match_field = lscan.field(_pattern_field);
  size_t match_field_size = lscan.field_size(_pattern_field);

  bool match = _matcher->do_search(match_field, match_field_size);
  bool is_complete = (!(_complete_match)) || match_field_size == _matcher->size();
  return match && is_complete;
}

bool csv::Singleline_BMatcher::do_search(Circbuf& c, Linescan& result){
  size_t read_size = c.read_size();
  const char* head = c.advance_head(_advance_next);
  if(c.at_eof()) return false;
  result.do_scan(head,read_size);

  bool match = this->match(result);
  _advance_next = result.length();
  return match;  
}

bool csv::contains_special_chars(const string& regex){
  bool match = false;
  for(const char& c: ".[]{}()\\*+?|^$") {
    match = regex.find(c) != string::npos;
    if(match) return true;
  }
  return false;
}

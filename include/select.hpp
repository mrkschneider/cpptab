#ifndef INCLUDE_CSV_SELECT_HPP_
#define INCLUDE_CSV_SELECT_HPP_

#include <string>
#include <vector>
#include <map>
#include <any>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <boost/regex.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/algorithm/searching/boyer_moore.hpp>
#include <circbuf.h>
#include <linescan.h>
#include <oniguruma.h>
#include "../include/constants.hpp"

namespace csv {
  
  class Context {
  private:
    uint _read_size;
    uint _buffer_size;
    boost::scoped_array<char> _bytes;
    FILE* _fd;
    circbuf* _cbuf;
    linescan* _lscan;

    void initialize(FILE* fd, uint read_size, uint buffer_size){
      _read_size = read_size;
      _buffer_size = buffer_size;
      _bytes.reset(new char[buffer_size]());
      _fd = fd;
      _cbuf = circbuf_create(_bytes.get(),buffer_size,read_size,_fd);
      _cbuf->bytes[read_size-1] = NL;
      _lscan = linescan_create(read_size);
    }
  public:
    Context(FILE* fd, uint read_size, uint buffer_size){
      initialize(fd, read_size, buffer_size);
    }
    
    Context(std::string csv_path, uint read_size, uint buffer_size){ 
      FILE* fd = fopen(csv_path.c_str(),"r");
      initialize(fd, read_size, buffer_size);
    }

    ~Context(){
      linescan_free(_lscan);
      circbuf_free(_cbuf);
      fclose(_fd);
    }
    Context(const Context& o) = delete; 
    Context& operator=(const Context& o) = delete;
    
    inline circbuf* cbuf() const {return _cbuf;};
    inline linescan* lscan() const {return _lscan;};
  };

  class Matcher {
  public:
    virtual bool search(const char* begin, size_t n) = 0;
    virtual size_t position() const = 0;
    virtual size_t size() const = 0;

    virtual ~Matcher(){};
  };

  class Regex_Matcher : public Matcher {
  private:
    boost::regex _pattern;
    boost::cmatch _match_result;
    char _delimiter;
    
  public:
    Regex_Matcher(boost::regex pattern, boost::cmatch match_result,
		  char delimiter) :
      _pattern { pattern }, _match_result { match_result }, _delimiter {delimiter} {};
    
    virtual bool search(const char* begin, size_t n);
    inline virtual size_t position() const { return _match_result.position();};
    inline virtual size_t size() const {return _match_result.length();};
  };

  class Onig_Regex_Matcher : public Matcher {
  private:
    regex_t* _pattern;
    OnigRegion* _match_result;
    char _delimiter;

  public:
    Onig_Regex_Matcher(std::string pattern, char delimiter){
      OnigEncoding use_encs[1];
      use_encs[0] = ONIG_ENCODING_ASCII;
      onig_initialize(use_encs, sizeof(use_encs)/sizeof(use_encs[0]));

      OnigErrorInfo einfo;
      auto pattern_c = boost::scoped_array<UChar>(new UChar[pattern.size()]);
      strncpy((char*)(pattern_c.get()),pattern.c_str(),pattern.size());
      int rc = onig_new(&_pattern, pattern_c.get(), pattern_c.get() + pattern.size(),
			CSV_ONIG_SYNTAX_OPTIONS, ONIG_ENCODING_ASCII, ONIG_SYNTAX_PERL, &einfo);
      if(rc != ONIG_NORMAL){
	char s[ONIG_MAX_ERROR_MESSAGE_LEN];
	onig_error_code_to_str((UChar*)s, rc, &einfo);
	throw std::runtime_error(s);
      }
      _match_result = onig_region_new();
      _delimiter = delimiter;
    }

    ~Onig_Regex_Matcher(){
      onig_region_free(_match_result, 1);
      onig_free(_pattern);
      onig_end();
    }
    
    virtual bool search(const char* begin, size_t n);
    inline virtual size_t position() const { return _match_result->beg[0];};
    inline virtual size_t size() const {return _match_result->end[0] - _match_result->beg[0];};
  };

  class Boyer_Moore_Matcher : public Matcher {
  private:
    std::string _pattern;
    char _delimiter;
    boost::scoped_ptr<boost::algorithm::boyer_moore<const char*>> _matcher;
    size_t _position;
    size_t _size;

  public:
    Boyer_Moore_Matcher(std::string pattern, char delimiter) {
      _pattern = pattern;
      _size = pattern.size();
      _position = 0;
      _delimiter = delimiter;
      const char* begin = _pattern.c_str();
      const char* end = begin + _size;
      _matcher.reset(new boost::algorithm::boyer_moore<const char*>(begin,end));
    };

    virtual bool search(const char* begin, size_t n);
    inline virtual size_t position() const { return _position;};
    inline virtual size_t size() const {return _size;};

    Boyer_Moore_Matcher(const Boyer_Moore_Matcher& o) = delete; 
    Boyer_Moore_Matcher& operator=(const Boyer_Moore_Matcher& o) = delete;
  };

  class Linescan {
  private:
    const size_t _offsets_size = 4096;
    
    linescan* _lscan;
    const char* _begin;
    size_t _length;
    std::vector<size_t> _offsets;
    size_t _match_field;
    size_t _n_fields;
  public:
    const char* begin() const {return _begin;};
    size_t length() const {return _length;};
    size_t match_field() const {return _match_field;};
    size_t n_fields() const {return _n_fields;};
    const std::vector<size_t>& field_offsets() const {return _offsets;};

    const char* field(size_t idx) const;
    size_t field_size(size_t idx) const;
    std::string str() const;

    Linescan(){
      _lscan = linescan_create(_offsets_size);
      _offsets = std::vector<size_t>();
      reset();
    }

    ~Linescan(){
      linescan_free(_lscan);
    }
    
    void reset(){
      _begin = nullptr;
      _length = 0;
      _match_field = 0;
      _offsets.clear();
    }

    friend void scan(const char* buf, uint n, char delimiter, Linescan&);
  };

 

  class Buffer_Matcher {
  public:
    virtual bool search(circbuf* c,
			Matcher& matcher,
			Linescan& result) = 0;
    virtual ~Buffer_Matcher(){};
  };
  
  class Multiline_BMatcher : public Buffer_Matcher {
  private:
    char _delimiter;
    size_t _pattern_field;
    bool _complete_match;
    size_t _advance_next;
    
  public:
    Multiline_BMatcher(char delimiter,
		       size_t pattern_field,
		       bool complete_match){
      _delimiter = delimiter;
      _pattern_field = pattern_field;
      _complete_match = complete_match;
      _advance_next = 0;
    }
    
    virtual bool search(circbuf* c, Matcher& matcher, Linescan& result);
  };

  class Singleline_BMatcher : public Buffer_Matcher {
  private:
    char _delimiter;
    size_t _pattern_field;
    bool _complete_match;
    size_t _advance_next;

  public:
    Singleline_BMatcher(char delimiter,
			size_t pattern_field,
			bool complete_match){
      _delimiter = delimiter;
      _pattern_field = pattern_field;
      _complete_match = complete_match;
      _advance_next = 0;
    }
    virtual bool search(circbuf* c, Matcher& matcher, Linescan& result);
  };

  class Line_Scan_Printer {
  public:
    virtual void print(const Linescan& sc_result) const = 0;
    virtual ~Line_Scan_Printer(){};
  };

  class Line_Printer : public Line_Scan_Printer {
  public:
    virtual void print(const Linescan& sc_result) const;
  };

  class Field_Printer : public Line_Scan_Printer {
  private:
    std::vector<size_t> _fields;
    char _delimiter;

  public:
    virtual void print(const Linescan& sc_result) const;
    Field_Printer(std::vector<size_t> fields, char delimiter) :
      _fields {fields}, _delimiter {delimiter} {};   
  };
      

  inline char* simple_scan_left(const char* buf, uint n, char target){
    return (char*)memrchr(buf-n,target,n);
  }
  inline char* simple_scan_right(const char* buf, uint n, char target){
    return (char*)memchr(buf,target,n);
  }

  void print_line(const Linescan& sc_result);
  void print_fields(const Linescan& sc_result, char delimiter,
                    const std::vector<size_t>& print_fields);
  void scan_header(circbuf* c, char delimiter, Linescan& sc_result);
  void scan(const char* buf, uint n, char delimiter, Linescan& r);
}

#endif

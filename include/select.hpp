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

  };
  
  class Line_Scan {
  private:
    const char* _begin;
    size_t _length;
    std::vector<size_t> _field_offsets;
    size_t _match_field;
    size_t _n_fields;

  public:
    Line_Scan() {
      reset();
    };
    void reset() {
      _begin = nullptr;
      _length = 0;
      _match_field = 0;
      _n_fields = 0;
      _field_offsets.clear();
    };
    inline const char* begin() const {return _begin;};
    inline size_t length() const {return _length;};
    inline size_t match_field() const {return _match_field;};
    inline size_t n_fields() const {return _n_fields;};
    inline const std::vector<size_t>& field_offsets() const {return _field_offsets;};

    const char* field(size_t idx) const;
    size_t field_size(size_t idx) const;
    std::string str() const;
    
    friend void scan(const char* buf, uint n, char target, char delimiter,
		     linescan* l_r, Line_Scan&);
  };

  class Line_Scan_Printer {
  public:
    virtual void print(const Line_Scan& sc_result) const = 0;
  };

  class Line_Printer : public Line_Scan_Printer {
  public:
    virtual void print(const Line_Scan& sc_result) const;
  };

  class Field_Printer : public Line_Scan_Printer {
  private:
    std::vector<size_t> _fields;
    char _delimiter;

  public:
    virtual void print(const Line_Scan& sc_result) const;
    Field_Printer(std::vector<size_t> fields, char delimiter) :
      _fields {fields}, _delimiter {delimiter} {};   
  };
      

  inline char* simple_scan_left(const char* buf, uint n, char target){
    return (char*)memrchr(buf-n,target,n);
  }
  inline char* simple_scan_right(const char* buf, uint n, char target){
    return (char*)memchr(buf,target,n);
  }

  void scan(const char* buf, uint n, char target, char delimiter, linescan* l_r,
	    Line_Scan&);
  void print_line(const Line_Scan& sc_result);
  void print_fields(const Line_Scan& sc_result, char delimiter,
                    const std::vector<size_t>& print_fields);
  void scan_header(circbuf* c, char delimiter, linescan* l_r, Line_Scan& sc_result);
  void scan_match_print_line(circbuf* c, char delimiter,
			     Matcher& matcher,
			     size_t pattern_field,
			     bool complete_match,
			     linescan* l_r,
			     Line_Scan& sc_result,
			     const Line_Scan_Printer& printer
			     );
}

#endif

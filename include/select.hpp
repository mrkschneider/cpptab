#ifndef INCLUDE_CSV_HPP_
#define INCLUDE_CSV_HPP_

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
#include <circbuf.h>

namespace csv {
  
  class Context {
  private:
    uint _read_size;
    uint _buffer_size;
    boost::scoped_array<char> _bytes;
    FILE* _fd;
    circbuf* _cbuf;
  public:
    Context(std::string csv_path, uint read_size, uint buffer_size){
      _read_size = read_size;
      _buffer_size = buffer_size;
      _bytes.reset(new char[buffer_size]);
      _fd = fopen(csv_path.c_str(),"r");
      _cbuf = circbuf_create(_bytes.get(),buffer_size,read_size,_fd);
      _cbuf->bytes[read_size-1] = '\n';
    }
    ~Context(){
      circbuf_free(_cbuf);
      fclose(_fd);
    }
    Context(const Context& o) = delete; 
    Context& operator=(const Context& o) = delete;
    
    circbuf* cbuf(){return _cbuf;};
  };
  
  
  class Line_Scan {
  private:
    char* _begin;
    size_t _length;
    std::vector<size_t> _delimiter_offsets;
    size_t _match_field;
    size_t _n_fields;

  public:
    Line_Scan() {
      _delimiter_offsets = std::vector<size_t>();
      reset();
    };
    void reset() {
      _begin = nullptr;
      _length = 0;
      _match_field = 0;
      _n_fields = 0;
      _delimiter_offsets.clear();
    };
    char* begin() const {return _begin;};
    size_t length() const {return _length;};
    size_t match_field() const {return _match_field;};
    size_t n_fields() const {return _n_fields;};
    const std::vector<size_t>& delimiter_offsets() const {return _delimiter_offsets;};

    std::string str();
    
    friend void scan(const char* buf, uint n, char target, char delimiter, Line_Scan&);
  };

  inline char* simple_scan_left(const char* buf, uint n, char target){
    return (char*)memrchr(buf-n,target,n);
  }
  inline char* simple_scan_right(const char* buf, uint n, char target){
    return (char*)memchr(buf,target,n);
  }

  void scan(const char* buf, uint n, char target, char delimiter, Line_Scan&);
  void print_line(const Line_Scan& sc_result);
  void print_fields(const Line_Scan& sc_result, const std::vector<size_t>& print_fields);
  void print_header(circbuf* c, Line_Scan& sc_result);
  bool match_print_line(circbuf* c, const boost::regex& pattern,
			boost::cmatch& match_result,
			size_t pattern_field,
			Line_Scan& sc_result);
}

#endif /* INCLUDE_CSV_HPP_ */

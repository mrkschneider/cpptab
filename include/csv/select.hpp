#ifndef INCLUDE_CSV_SELECT_HPP_
#define INCLUDE_CSV_SELECT_HPP_

#include <stdio.h>
#include <string.h>

#include <oniguruma.h>

#include <circbuf.h>
#include <linescan.h>

#include <string>
#include <vector>
#include <map>
#include <any>
#include <iostream>
#include <sstream>
#include <boost/regex.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/algorithm/searching/boyer_moore.hpp>

#include <csv/constants.hpp>
#include <csv/error.hpp>

namespace csv {

  class Circbuf {
  private:
    size_t _read_size;
    size_t _buffer_size;
    boost::scoped_array<char> _bytes;
    FILE* _fd;
    circbuf* _cbuf;

    void initialize(FILE* fd, size_t read_size, size_t buffer_size){
      _read_size = read_size;
      _buffer_size = buffer_size;
      _bytes.reset(new char[buffer_size]());
      _fd = fd;
      _cbuf = circbuf_create(_bytes.get(),buffer_size,read_size,_fd);
      _cbuf->bytes[read_size-1] = NL;
    }
  public:
    Circbuf(FILE* fd, size_t read_size, size_t buffer_size){
      initialize(fd, read_size, buffer_size);
    }
    
    Circbuf(const std::string& csv_path, size_t read_size, size_t buffer_size){ 
      FILE* fd = fopen(csv_path.c_str(),"r");
      initialize(fd, read_size, buffer_size);
    }

    ~Circbuf(){
      circbuf_free(_cbuf);
      fclose(_fd);
    }

    char* head() const {return circbuf_head(_cbuf);};
    bool finished() const {return _cbuf->finished;};
    size_t read_size() const {return _read_size;};
    bool at_eof() const {return head()[0] == '\0';};

    char* advance_head(size_t n) {return circbuf_head_forward(_cbuf,n);};
    
    Circbuf(const Circbuf& o) = delete; 
    Circbuf& operator=(const Circbuf& o) = delete;
    
  };

  class Matcher {
  public:
    virtual bool do_search(const char* begin, size_t n) = 0;
    virtual size_t position() const = 0;
    virtual size_t size() const = 0;

    virtual ~Matcher(){};
  };

  class Regex_Matcher : public Matcher {
  private:
    boost::regex _pattern;
    boost::cmatch _match_result;
    
  public:
    Regex_Matcher(boost::regex pattern, boost::cmatch match_result) :
      _pattern { pattern }, _match_result { match_result } {};
    
    bool do_search(const char* begin, size_t n) override;
    size_t position() const override { return _match_result.position();};
    size_t size() const override {return _match_result.length();};
  };

  class Onig_Regex_Matcher : public Matcher {
  private:
    regex_t* _pattern;
    OnigRegion* _match_result;
    OnigMatchParam* _match_param;
    static bool _initialized;

  public:
    Onig_Regex_Matcher(std::string pattern) 
    {
      if(!_initialized){ // LCOV_EXCL_START
	/* No exception thrown from constructor - potential memory
	   leak as destructor would never be called. */
	csv::exit_error("Onig_Regex_Matcher not initialized before first constructor call.");
      } // LCOV_EXCL_STOP
      OnigErrorInfo einfo;
      auto pattern_c = boost::scoped_array<UChar>(new UChar[pattern.size()]);
      strncpy((char*)(pattern_c.get()),pattern.c_str(),pattern.size());
      int rc = onig_new(&_pattern, pattern_c.get(), pattern_c.get() + pattern.size(),
			CSV_ONIG_SYNTAX_OPTIONS, ONIG_ENCODING_ASCII, ONIG_SYNTAX_PERL, &einfo);
      if(rc != ONIG_NORMAL){ // LCOV_EXCL_START
	char s[ONIG_MAX_ERROR_MESSAGE_LEN];
	onig_error_code_to_str((UChar*)s, rc, &einfo);
	csv::exit_error(s);
      } // LCOV_EXCL_STOP
      _match_param = onig_new_match_param();
      _match_result = onig_region_new();
    }

    ~Onig_Regex_Matcher(){
      onig_region_free(_match_result, 1);
      onig_free_match_param(_match_param);
      onig_free(_pattern);
    }

    static void initialize(){
      OnigEncoding use_encs[1];
      use_encs[0] = ONIG_ENCODING_ASCII;
      onig_initialize(use_encs, sizeof(use_encs)/sizeof(use_encs[0]));
      _initialized = true;
    }

    static void finalize(){
      onig_end();
      _initialized = false;
    }
    
    bool do_search(const char* begin, size_t n) override;
    size_t position() const override { return _match_result->beg[0];};
    size_t size() const override {return _match_result->end[0] - _match_result->beg[0];}; 

    Onig_Regex_Matcher(const Onig_Regex_Matcher& o) = delete; 
    Onig_Regex_Matcher& operator=(const Onig_Regex_Matcher& o) = delete;
  };

  class Boyer_Moore_Matcher : public Matcher {
  private:
    const std::string _pattern;
    const size_t _size;
    size_t _position;
    boost::scoped_ptr<boost::algorithm::boyer_moore<const char*>> _matcher;

  public:
    Boyer_Moore_Matcher(std::string pattern) :
      _pattern {pattern},
      _size {pattern.size()}
    {
      _position = 0;
      const char* begin = _pattern.c_str();
      const char* end = begin + _size;
      _matcher.reset(new boost::algorithm::boyer_moore<const char*>(begin,end));
    };

    bool do_search(const char* begin, size_t n) override;
    size_t position() const override { return _position;};
    size_t size() const override {return _size;};

    Boyer_Moore_Matcher(const Boyer_Moore_Matcher& o) = delete; 
    Boyer_Moore_Matcher& operator=(const Boyer_Moore_Matcher& o) = delete;
  };

  class Linescan {
  private:
    const char _delimiter;
    const uint64_t _delimiter_mask;
    const size_t _offsets_size;
    linescan* const _lscan;
    const char* _begin;
    std::vector<size_t> _offsets;
    size_t _length;
    size_t _match_field;
    size_t _n_fields;
    bool _crnl;
  public:
    const char* begin() const {return _begin;};
    size_t length() const {return _length;};
    size_t match_field() const {return _match_field;};
    size_t n_fields() const {return _n_fields;};
    const std::vector<size_t>& offsets() const {return _offsets;};
    bool crnl() const {return _crnl;};

    const char* field(size_t idx) const;
    size_t field_size(size_t idx) const;
    std::string str() const;

    void do_scan(const char* buf, size_t n);
    void do_scan_header(const char* buf, size_t n);

    void set_crnl(bool crnl) { _crnl = crnl; };
    void adjust_for_crnl() { _offsets[_offsets.size()-1]--;};

    Linescan(char delimiter, size_t offsets_size) :
      _delimiter {delimiter},
      _delimiter_mask {linescan_create_mask(delimiter)},
      _offsets_size {offsets_size},
      _lscan {linescan_create(offsets_size)}
    {
      _offsets = std::vector<size_t>();
      _offsets.reserve(_offsets_size);
      _crnl = false;
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

    Linescan(const Linescan& o) = delete; 
    Linescan& operator=(const Linescan& o) = delete;
  };

  class Buffer_Matcher {
  public:
    virtual bool do_search(Circbuf& c,
			   Matcher& matcher,
			   Linescan& result) = 0;
    virtual ~Buffer_Matcher(){};
  };
  
  class Multiline_BMatcher : public Buffer_Matcher {
  private:
    const char _delimiter;
    const size_t _pattern_field;
    const bool _complete_match;
    size_t _advance_next;
    
  public:
    Multiline_BMatcher(char delimiter,
		       size_t pattern_field,
		       bool complete_match) :
      _delimiter {delimiter},
      _pattern_field {pattern_field},
      _complete_match {complete_match},
      _advance_next {0} {};
    bool do_search(Circbuf& c, Matcher& matcher, Linescan& result) override;
  };

  class Singleline_BMatcher : public Buffer_Matcher {
  private:
    const char _delimiter;
    const size_t _pattern_field;
    const bool _complete_match;
    size_t _advance_next;

  public:
    Singleline_BMatcher(char delimiter,
			size_t pattern_field,
			bool complete_match) :
      _delimiter {delimiter},
      _pattern_field {pattern_field},
      _complete_match {complete_match},
      _advance_next {0} {};
    bool do_search(Circbuf& c, Matcher& matcher, Linescan& result) override;
  };

  class Linescan_Printer { // LCOV_EXCL_START
  public:
    virtual void print(const Linescan& sc_result) const = 0;
    virtual ~Linescan_Printer(){};
  }; // LCOV_EXCL_STOP

  class Line_Printer : public Linescan_Printer { // LCOV_EXCL_START
  public:
    void print(const Linescan& sc_result) const override;
  }; // LCOV_EXCL_STOP

  class Field_Printer : public Linescan_Printer { // LCOV_EXCL_START
  private:
    const std::vector<size_t> _fields;
    const char _delimiter;

  public:
    void print(const Linescan& sc_result) const override;
    Field_Printer(std::vector<size_t> fields, char delimiter) :
      _fields {fields}, _delimiter {delimiter} {};   
  }; // LCOV_EXCL_STOP
      

  inline char* simple_scan_left(const char* buf, uint n, char target){
    return (char*)memrchr(buf-n,target,n);
  }
  inline char* simple_scan_right(const char* buf, uint n, char target){
    return (char*)memchr(buf,target,n);
  }

}

#endif

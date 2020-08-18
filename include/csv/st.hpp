#ifndef INCLUDE_CSV_ST_HPP_
#define INCLUDE_CSV_ST_HPP_

#include <string>
#include <sstream>
#include <vector>

namespace st {
  std::string str_ptr(const void* ptr);

  template<class T>
  std::string str_vector(const std::vector<T>& xs){
    std::ostringstream s;
    s << "[";
    for(const T& x:xs){
      s << " " << x;
    }
    s << " ]";
    return s.str();
  }

  inline char str2char(const std::string& s){
    if(s.size()!=1) throw std::runtime_error("Cannot convert string to char: " + s);
    return s[0];
  }

  std::vector<std::string> split(const std::string& s, char delimiter);
}

#endif

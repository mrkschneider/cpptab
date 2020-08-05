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
}

#endif

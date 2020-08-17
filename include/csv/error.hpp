#ifndef INCLUDE_CSV_ERROR_HPP_
#define INCLUDE_CSV_ERROR_HPP_

#include <iostream>
namespace csv {
  
  template <typename T>
  void exit_error(const T& msg){ // LCOV_EXCL_START
    std::cerr << "ERROR: " << msg << std::endl;  
    exit(1); 
  } // LCOV_EXCL_STOP
  
}

#endif

#include <../include/st.hpp>

std::string st::str_ptr(const void* ptr){
  std::ostringstream s;
  if(ptr == nullptr){
    s << "null";
  } else {
    s << ptr;
  }
  return s.str();
}

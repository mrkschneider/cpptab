#include <csv/st.hpp>

#include <iostream>

std::string st::str_ptr(const void* ptr){
  std::ostringstream s;
  if(ptr == nullptr){
    s << "null";
  } else {
    s << ptr;
  }
  return s.str();
}

std::vector<std::string> st::split(const std::string& s, char delimiter){
  size_t i = 0;
  size_t end = 0;
  std::vector<std::string> v;
  if(s.empty()) return v;
  while(end != std::string::npos){
    end = s.find(delimiter,i);
    v.push_back(s.substr(i,end-i));
    i = end + 1;
  }
  return v;
}

std::vector<size_t> st::numbers(size_t start, size_t end) {
    std::vector<size_t> r;
    for(size_t i=start;i<end;i++)
      r.push_back(i);
    return r;
}

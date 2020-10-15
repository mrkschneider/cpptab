#ifndef INCLUDE_CSV_ST_HPP_
#define INCLUDE_CSV_ST_HPP_

#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <iterator>
#include <algorithm>
#include <iostream>

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

  template<class T>
  std::string str_vectors(const std::vector<T>& xs){
    std::ostringstream s;
    s << "[";
    for(const T& x:xs){
      s << " " << str_vector(x);
    }
    s << " ]";
    return s.str();
  }

  template<class T>
  std::string str_set(const std::set<T>& xs){
    std::ostringstream s;
    s << "{";
    for(const T& x:xs){
      s << " " << x;
    }
    s << " }";
    return s.str();
  }

  inline char str2char(const std::string& s){
    if(s.size()!=1) throw std::runtime_error("Cannot convert string to char: " + s);
    return s[0];
  }

  template<class Map>
  std::set<class Map::key_type> keys(const Map& m){
    std::set<class Map::key_type> r;
    for(const auto& kv:m){
      r.insert(kv.first);
    }
    return r;
  }

  template<class Map>
  class Map::value_type get(const Map& m, const class Map::key_type& key){
    class Map::const_iterator it = m.find(key);
    if(it == m.end()) throw std::runtime_error("Key not found");
    return *it;
  }

  template<class Map>
  class Map::value_type getOr(const Map& m, const class Map::key_type& key,
			      const class Map::value_type& default_value){
    class Map::const_iterator it = m.find(key);
    if(it == m.end()) return default_value;
    return *it;
  }

  template<class Map>
  bool contains(const Map& m, const class Map::key_type& key){
    return m.find(key) != m.end();
  }

  template<class T>
  size_t find_idx(const std::vector<T>& v, T ele){
    size_t size = v.size();
    for(size_t i=0;i<size;i++){
      const T& x = v[i];
      int rc = x.compare(ele);
      if(rc == 0) return i;
    }
    return size;
  }

  template<class T>
  std::set<T> intersection_set(const std::set<T>& a, const std::set<T>& b){
    std::set<T> r;
    std::set_intersection(a.begin(),a.end(),
			  b.begin(),b.end(),
			  std::inserter(r, std::end(r)));
    return r;
  }

  template<class T>
  std::set<T> union_set(const std::set<T>& a, const std::set<T>& b){
    std::set<T> r;
    std::set_union(a.begin(),a.end(),
		   b.begin(),b.end(),
		   std::inserter(r, std::end(r)));
    return r;
  }

  template<class T>
  std::set<T> directional_difference_set(const std::set<T>& a, const std::set<T>& b){
    std::set<T> r;
    std::set_difference(a.begin(),a.end(),
			b.begin(),b.end(),
			std::inserter(r, std::end(r)));
    return r;
  }

  template<class T>
  void remove_from_vec(std::vector<T>& xs, const std::set<T>& es){
    for(int i=xs.size()-1;i>=0;i--){
      const T& x = xs[i];
      if(es.find(x) != es.end()){
	xs.erase(xs.begin()+i);
      }
    }
  }

  std::vector<size_t> numbers(size_t start, size_t end);  
  std::vector<std::string> split(const std::string& s, char delimiter);
}

#endif

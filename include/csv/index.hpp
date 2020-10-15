#ifndef INCLUDE_CSV_INDEX_HPP_
#define INCLUDE_CSV_INDEX_HPP_

#include <stdio.h>

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>

#include <csv/match.hpp>

namespace csv {

  class Index {
  private:
    FILE* _fd;
    std::vector<std::vector<size_t>> _offsetss; 
    std::vector<size_t> _line_offsets;
    std::unordered_map<std::string,size_t> _column_keys;
    size_t _read_size; 

  public:
    
    Index(FILE* fd, size_t read_size) :
      _fd { fd } , _read_size {read_size} {};
    ~Index() {
      fclose(_fd);
    }
    
    static std::unique_ptr<Index> create(const std::string& csv_path, char delimiter,
					 size_t read_size, size_t buffer_size,
					 size_t offsets_size);

    const std::vector<std::vector<size_t>>& offsetss() const { return _offsetss; };
    const std::vector<size_t>& line_offsets() const { return _line_offsets; };
    const char* field(size_t line_idx, size_t field_idx);
    size_t field_size(size_t line_idx, size_t field_idx); 
    
    Index(const Index& o) = delete;
    Index& operator=(const Index& o) = delete;
  };


}


#endif

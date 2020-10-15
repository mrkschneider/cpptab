#include <cxxtest/TestSuite.h>

#include <string>
#include <map>
#include <stdexcept>

#include <stdio.h>

#include <csv/index.hpp>
#include <csv/st.hpp>

typedef std::vector<size_t> Vec_size_t;
typedef std::vector<std::vector<size_t>> Vecs_size_t;
typedef std::vector<std::string> Vec_string;

class Index_Test : public CxxTest::TestSuite {
private:
  size_t read_size = 100;
  size_t buffer_size = 100 * 100;
  size_t offsets_size = 100;
  std::string in_path = "./test_resources/simple.csv";
  Vecs_size_t ref_offsetss = {{0,3,6,8},{0,2,4,6,7},
		       {0,1}, {0,3,5,7},
		       {0,3,7,11},{0,3,6,10,11},
		       {0,3,6,9,10},{0,3,7,10}
  };
  Vec_size_t ref_line_offsets = {6, 14, 21, 22, 29, 40, 51, 61, 71};

  char delimiter = ',';
  std::unique_ptr<csv::Index> idx;

public:
  void setUp(){
    idx = csv::Index::create(in_path, delimiter, read_size, buffer_size, offsets_size);
  }

  void tearDown(){
  }

  void test_create(){
    Vecs_size_t idxss = idx->offsetss();

    TS_ASSERT_EQUALS(ref_offsetss.size(),idxss.size());
    TS_ASSERT_EQUALS(ref_offsetss,idxss);

    Vec_size_t line_offsets = idx->line_offsets();

    TS_ASSERT_EQUALS(ref_line_offsets.size(),line_offsets.size());
    TS_ASSERT_EQUALS(ref_line_offsets,line_offsets);
  }

};


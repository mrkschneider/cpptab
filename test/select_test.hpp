#include <cxxtest/TestSuite.h>

#include <string>
#include <map>
#include <stdexcept>

#include <stdio.h>

#include <csv/select.hpp>

typedef std::vector<size_t> Vec_size_t;
typedef std::vector<std::string> Vec_string;

class CircbufTest : public CxxTest::TestSuite {
private:
  size_t read_size = 4;
  size_t buffer_size = 16;
  std::string in_path = "./test_resources/bytes.txt";
  csv::Circbuf* cbuf;

public:
  void setUp(){
    cbuf = new csv::Circbuf(in_path, read_size, buffer_size);    
  }

  void tearDown(){
    delete cbuf;
  }

  void test_advance_head(){
    char* head = cbuf->head();
    TS_ASSERT_EQUALS('a',head[0]);
    TS_ASSERT_EQUALS(false,cbuf->finished());
    TS_ASSERT_EQUALS(read_size,cbuf->read_size());
    TS_ASSERT_EQUALS(false,cbuf->at_eof());

    head = cbuf->advance_head(4);
    TS_ASSERT_EQUALS('e',head[0]);

    head = cbuf->advance_head(4);
    head = cbuf->advance_head(4);
    head = cbuf->advance_head(4);
    head = cbuf->advance_head(4);
    head = cbuf->advance_head(4);
    // Close to the end now    
    TS_ASSERT_EQUALS('\n',head[2]);
    TS_ASSERT_EQUALS('\0',head[3]);
    TS_ASSERT_EQUALS(true,cbuf->finished()); // Reached EOF
    TS_ASSERT_EQUALS(false,cbuf->at_eof()); // Head not yet at EOF

    head = cbuf->advance_head(4); // After EOF, every byte should be \0
    TS_ASSERT_EQUALS(true,cbuf->at_eof());
    for(size_t i=0;i<4;i++)
      TS_ASSERT_EQUALS('\0',head[i]);
  }

  void test_constructor_fd(){
    FILE* fd = fopen(in_path.c_str(),"r");
    csv::Circbuf c = csv::Circbuf(fd,read_size,buffer_size);
    char* head = cbuf->advance_head(4);
    TS_ASSERT_EQUALS('e',head[0]);
  } 

};




class LinescanTest : public CxxTest::TestSuite {
private:
  char* b;
  size_t size = 16;
  csv::Linescan* lscan;
  char delimiter = 'b';

public:

  void setUp(){
    b = new char[size];
    for(size_t i=0;i<size/4;i++)
      for(size_t j=0;j<4;j++)
    	b[i*4 + j] = (char)(j + 97); // chars a to d

    b[0] = '\n';
    b[size-1] = '\n';
    lscan = new csv::Linescan(delimiter,size);
  }

  void tearDown(){
    delete[] b;
    delete lscan;
  }

  void test_do_scan(){

    auto offsets = Vec_size_t{0,1,5,9,13,15};

    { // Normal operation
      lscan->reset();
      lscan->do_scan(b+12,size);
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      TS_ASSERT_EQUALS(b+1,lscan->begin());
      TS_ASSERT_EQUALS(size-1,lscan->length());
      TS_ASSERT_EQUALS(3,lscan->match_field());
      TS_ASSERT_EQUALS(5,lscan->n_fields());
      TS_ASSERT_EQUALS(false,lscan->crnl());
      for(size_t i=0;i<lscan->n_fields();i++)
	TS_ASSERT_EQUALS(b+1+offsets[i],lscan->field(i));
      auto field_sizes = Vec_size_t{0,3,3,3,1};
      for(size_t i=0;i<lscan->n_fields();i++){
	TS_ASSERT_EQUALS(field_sizes[i],lscan->field_size(i));
      }
      auto fields = Vec_string{"","cda","cda","cda","c"};
      for(size_t i=0;i<lscan->n_fields();i++){
	TS_ASSERT_EQUALS(fields[i],std::string(lscan->field(i),lscan->field_size(i)));
      }
#ifdef CSV_DEBUG
      TS_ASSERT_THROWS_ANYTHING(lscan->field(lscan->n_fields()));
      TS_ASSERT_THROWS_ANYTHING(lscan->field_size(lscan->n_fields()));
#endif
      
    }

    { // Start at delimiter
      lscan->reset();
      char* start = b+5;
      lscan->do_scan(start,size);
      TS_ASSERT_EQUALS('b',start[0]);
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      TS_ASSERT_EQUALS(b+1,lscan->begin());
      TS_ASSERT_EQUALS(size-1,lscan->length());
      TS_ASSERT_EQUALS(1,lscan->match_field());
      TS_ASSERT_EQUALS(5,lscan->n_fields());
    }

    { // Start at beginning newline
      lscan->reset();
      TS_ASSERT_THROWS_ANYTHING(lscan->do_scan(b,size)); // No left newline, only to the right
      lscan->reset();
      TS_ASSERT_THROWS_NOTHING(lscan->do_scan(b+1,size-1));
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      TS_ASSERT_EQUALS(b+1,lscan->begin());
    }

    { // Start at final newline
      lscan->reset();
      TS_ASSERT_THROWS_NOTHING(lscan->do_scan(b+size-1,size));
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      TS_ASSERT_EQUALS(b+1,lscan->begin());
    }
    
  }

  void test_do_scan_2(){
    { // No right newline
      lscan->reset();
      TS_ASSERT_THROWS_NOTHING(lscan->do_scan(b+1,size-1));
      b[size-1] = 'x';
      lscan->reset();
      TS_ASSERT_THROWS_ANYTHING(lscan->do_scan(b+1,size-1));
      lscan->reset();
      b[size-1] = '\0'; // Null terminator works as alternative newline
      TS_ASSERT_THROWS_NOTHING(lscan->do_scan(b+1,size-1));
    }
  }

  void test_do_scan_header(){

    auto offsets = Vec_size_t{0,1,5,9,13,15};

    { // Normal operation
      lscan->reset();
      lscan->do_scan_header(b+1,size-1);
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      TS_ASSERT_EQUALS(b+1,lscan->begin());
      TS_ASSERT_EQUALS(size-1,lscan->length());
      TS_ASSERT_EQUALS(0,lscan->match_field());
      TS_ASSERT_EQUALS(5,lscan->n_fields());
      TS_ASSERT_EQUALS(false,lscan->crnl());
      for(size_t i=0;i<lscan->n_fields();i++)
	TS_ASSERT_EQUALS(b+1+offsets[i],lscan->field(i));
      auto field_sizes = Vec_size_t{0,3,3,3,1};
      for(size_t i=0;i<lscan->n_fields();i++){
	TS_ASSERT_EQUALS(field_sizes[i],lscan->field_size(i));
      }
      auto fields = Vec_string{"","cda","cda","cda","c"};
      for(size_t i=0;i<lscan->n_fields();i++){
	TS_ASSERT_EQUALS(fields[i],std::string(lscan->field(i),lscan->field_size(i)));
      }
    }
  }

  void test_do_scan_header_2(){
    { // No left newline
      lscan->reset();
      TS_ASSERT_THROWS_NOTHING(lscan->do_scan_header(b+1,size));
      lscan->reset();
      b[0] = 'x';
      TS_ASSERT_THROWS_ANYTHING(lscan->do_scan_header(b+1,size));
    }
  }

  void test_do_scan_header_3(){
    auto offsets = Vec_size_t{0,1,5,9,13,15};
    auto offsets_2 = Vec_size_t{0,1,5,9,13,14};

    { // DOS/Windows style (CRNL) newline
      lscan->reset();
      lscan->do_scan_header(b+1,size);
      TS_ASSERT_EQUALS(false,lscan->crnl());
      TS_ASSERT_EQUALS(offsets,lscan->offsets());
      
      lscan->reset();
      b[size-2] = '\r';
      lscan->do_scan_header(b+1,size);
      TS_ASSERT_EQUALS(true,lscan->crnl());
      TS_ASSERT_EQUALS(offsets_2,lscan->offsets());
    }
  }

  void test_adjust_for_crnl(){
    auto offsets = Vec_size_t{0,1,5,9,13,15};
    auto offsets_2 = Vec_size_t{0,1,5,9,13,14};
    { 
      lscan->reset();
      lscan->do_scan_header(b+1,size);

      TS_ASSERT_EQUALS(offsets,lscan->offsets());

      lscan->adjust_for_crnl();

      TS_ASSERT_EQUALS(offsets_2,lscan->offsets());
    }
  }

  void test_str(){
    std::string s = lscan->str();
    TS_ASSERT(!s.empty());

    lscan->do_scan_header(b+1,size);
    s = lscan->str();
    TS_ASSERT(!s.empty());
  }



  




  



};

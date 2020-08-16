#include <cxxtest/TestSuite.h>

#include <string>
#include <map>
#include <stdexcept>

#include <stdio.h>

#include <csv/select.hpp>

class LinescanTest : public CxxTest::TestSuite {
private:
  char* b;
  size_t size = 64;

public:

  void setUp(){
    b = new char[size];
    for(size_t i=0;i<64;i++)
      for(size_t j=0;j<4;j++)
	b[i*4 + j] = (char)(i + 97); // chars a to d
  }

  void tearDown(){
    delete[] b;
  }





  



};

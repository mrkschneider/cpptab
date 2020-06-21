#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iterator>

#include <ctype.h>
#include <stdio.h>
#include <iostream>
#include <typeinfo>
//#include <regex>
#include "../include/st.hpp"
#include <boost/regex.hpp>
#include <csv.hpp>
#include <circbuf.h>
#include <bufscan.h>

using namespace std;
using namespace st;


//typedef match_results<char*> bmatch;

int main(int argc, const char* argv[]){

  uint size = 16;
  char* bytes = new char[size]();
  uint read_size = 4;
  FILE* fd = fopen("bytes.txt","r");

  circbuf* c = circbuf_create(bytes,size,read_size,fd);
  circbuf_print(c);
  boost::regex pattern("bc");

  const char* head = circbuf_head(c);
  boost::cmatch match_result;
  bool match = boost::regex_search(head,head+3,match_result,pattern);

  printf("%d\n",match);
  printf("%ld\n",match_result.position());
  //printf("%s\n",head+match_result.position());
  printf("%s\n",match_result.str().c_str());
  
  circbuf_head_forward(c,4);
  circbuf_head_forward(c,4);
  circbuf_head_forward(c,4);
  printf("%s\n",match_result.str().c_str());
  circbuf_print(c);

  int rc = 0;
  char* b = circbuf_head(c);
  char fp[] = "fx";
  int n = 2;
  
  rc = bufscan_match_fingerprint(b,fp,fp,read_size,n,n);
  printf("%d\n",rc);

  circbuf_free(c);
  delete[] bytes;
  fclose(fd);
    
  return 0;
}

#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <iterator>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <typeinfo>
//#include <regex>
#include "../include/st.hpp"
#include <boost/regex.hpp>
#include <csv.hpp>
#include <circbuf.h>

using namespace std;
using namespace st;

typedef struct scan_result {
  char* target;
  bool match_delimiter;
  bool match_nl;
} scan_result;

void reset_scan_result(scan_result& r){
  r.target = NULL;
  r.match_delimiter = false;
  r.match_nl = false;
}

void scan_newline_left(const char* b, int n, char delimiter, int delimiter_n,
			scan_result& r){
  reset_scan_result(r);

  char* nl = (char*)memrchr(b-n,'\n',n);
  r.match_nl = nl != NULL;
  
  const char* dl = b;

  for(int i=0;i<delimiter_n;i++) {
    int n = dl-nl-1;
    dl = (char*)memrchr(dl-n,delimiter,n);
    //printf("%p\n",dl);
  }
  r.match_delimiter = dl > nl;
  r.target = nl;

  //printf("%d %d %p\n",r.match_nl,r.match_delimiter,r.target);
}

void print_header(circbuf* c, char* out){
  char* head = circbuf_head(c);
  char* nl = (char*)memchr(head,'\n',c->read_size);
  size_t n = nl - head;
  fwrite(head, sizeof(char), n, stdout);
  return;
}


bool match_print_line(circbuf* c, 
			boost::regex& pattern,
			boost::cmatch& match_result,
			scan_result& sc_result,
			char* out
			){
  const char* head = circbuf_head(c);
  int read_size = c->read_size;
  uint delimiter_count = 2;
  bool match = boost::regex_search(head,head+read_size,match_result,pattern);
  
  if(match){
    long pos = match_result.position();
    long size = match_result.length();
    long new_pos = pos+size;

    head = circbuf_head_forward(c,new_pos);
  
    scan_newline_left(head-size,read_size,',',delimiter_count,sc_result);
    if(!sc_result.match_nl) throw "Could not find left newline";

    bool print = sc_result.match_delimiter;
    if(print) {  
      char* nl_left = sc_result.target;
      char* nl_right = (char*)memchr(head,'\n',read_size);
      size_t n = nl_right - nl_left;
      fwrite(nl_left, sizeof(char), n, stdout);
    }
  } else {
    if(!c->finished){
      char* del_left = (char*)memrchr(head,',',read_size);
      head = circbuf_head_forward(c,del_left-head);
    } else {
      head = circbuf_head_forward(c,read_size);
    }
  }

  return match;
}


int main(int argc, const char* argv[]){
  uint size = 4096 * 1000;
  char* bytes = new char[size]();
  uint read_size = 4096;
  uint out_size = read_size * 100;
  char* out = new char[out_size];
  setvbuf(stdout, out, _IOFBF, out_size);

  FILE* fd = fopen("sales.csv","r");

  circbuf* c = circbuf_create(bytes,size,read_size,fd);
  boost::regex pattern("Fruits");
  boost::cmatch match_result;
  scan_result sc_result;

  print_header(c,out);

  char* head = circbuf_head(c);
  while(head[0]!=-1){
    match_print_line(c,pattern,match_result,sc_result,out);
    head = circbuf_head(c);
  }

  fprintf(stdout,"\n");
  fflush(stdout);

  // Cleanup
  circbuf_free(c);
  delete[] bytes;
  delete[] out;
  fclose(fd);
    
  return 0;
}

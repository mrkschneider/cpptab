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

using namespace std;
using namespace st;

typedef struct scan_result {
  size_t target_offset;
  size_t delimiter_count;
  bool match;
} scan_result;

void reset_scan_result(scan_result& r){
  r.target_offset = 0;
  r.delimiter_count = 0;
  r.match = false;
}

scan_result create_scan_result(){
  scan_result r = scan_result();
  reset_scan_result(r);
  return r;
}

void scan_newline_left(const char* b, int n, char delimiter, scan_result& result){
  reset_scan_result(result);

  for(int i=0;i<n;i++){
    char ch = b[-i];
    if(ch=='\n') {
      result.target_offset = i;
      result.match = true;
      return;
    } else if(ch==delimiter) {
      result.delimiter_count++;
    }
  }
  return;
}

void scan_newline_right(const char* b, int n, char delimiter, scan_result& result){
  reset_scan_result(result);

  for(int i=0;i<n;i++){
    char ch = b[i];
    if(ch=='\n') {
      result.target_offset = i;
      result.match = true;
      return;
    } else if(ch==delimiter) {
      result.delimiter_count++;
    }
  }
  return;
}

size_t print_header(circbuf* c, char* out, uint out_pos){
  char* head = circbuf_head(c);
  scan_result result = scan_result();
  scan_newline_right(head,c->read_size,',',result);
  int offset_right = result.target_offset;
  for(int i=0;i<offset_right;i++) {
    out[out_pos] = head[i];
    out_pos++;
  }
  out[out_pos] = '\n';
  out_pos++;
  circbuf_head_forward(c,offset_right);
  return out_pos;
}


size_t match_print_line(circbuf* c, 
		      boost::regex& pattern,
		      boost::cmatch& match_result,
		      scan_result& scan_result,
		      char* out,
		      size_t out_pos
		      ){
  const char* head = circbuf_head(c);
  int read_size = c->read_size;
  uint delimiter_count = 2;
  bool match = boost::regex_search(head,head+read_size,match_result,pattern);
  
  if(match){
    long pos = match_result.position();
    long size = match_result.length();

    char* b = circbuf_head(c);
  
    scan_newline_left(head+pos,read_size+pos,',',scan_result);
    long new_pos = pos+size;
    if(!scan_result.match) throw "Could not find left newline";

    bool print = scan_result.delimiter_count == delimiter_count;
    if(print) {  
      uint offset_left = scan_result.target_offset;
      for(int i=pos-offset_left+1;i<new_pos;i++) {
	out[out_pos] = b[i];
	out_pos++;
      }
    }
      
    head = circbuf_head_forward(c,new_pos);
    b = circbuf_head(c);
    if(print) {
      scan_newline_right(b,read_size,',',scan_result);
      if(!scan_result.match) throw "Could not find right newline";
      uint offset_right = scan_result.target_offset;
      for(uint i=0;i<offset_right;i++) {
	out[out_pos] = b[i];
	out_pos++;
      }
      out[out_pos] = '\n';
      out_pos++;
    }
  } else {
    head = circbuf_head_forward(c,read_size);
  }

  return out_pos;

}


int main(int argc, const char* argv[]){

  std::ios_base::sync_with_stdio(false);
  uint size = 16384;
  char* bytes = new char[size]();
  uint read_size = 4096;
  uint out_size = read_size * 10000;
  FILE* fd = fopen("sales.csv","r");
  //FILE* fd = fopen("test.csv","r");

  circbuf* c = circbuf_create(bytes,size,read_size,fd);
  boost::regex pattern("Fruits");
  boost::cmatch match_result;
  scan_result sc_result;

  char* out = new char[out_size];
  uint out_pos = 0;
  out_pos = print_header(c,out,out_pos);
  char* head = circbuf_head(c);
  while(head[0]!=-1){
    out_pos = match_print_line(c,pattern,match_result,sc_result,out,out_pos);
    head = circbuf_head(c);
  }

  out[out_pos] = '\0';
  printf("%s",out);

  // Cleanup
  circbuf_free(c);
  delete[] bytes;
  delete[] out;
  fclose(fd);
    
  return 0;
}

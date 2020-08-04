
#include <select.hpp>
#include <boost/regex.hpp>
#include <CLI/CLI.hpp>
#include "../include/st.hpp"
#include "../include/constants.h"

using namespace std;
using namespace st;
using namespace csv;

void run_select(){

  return;
}


int main(int argc, const char* argv[]){
  uint read_size = 4096;
  CLI::App app{"CppCsv"};

  string column = "";
  string regex = "";
  string csv_path = "";

  app.add_option("-c,--column",column,"Column to match")->required();
  app.add_option("-r,--regex",regex,"Regex to match in selected column")->required();
  app.add_option("csv",csv_path,"CSV path");


  CLI11_PARSE(app, argc, argv);
  
  Context c = Context(csv_path, read_size, read_size * 1000);
  circbuf* cbuf = c.cbuf();

  boost::regex pattern(regex);
  boost::cmatch match_result;
  Line_Scan sc_result;

  print_header(cbuf, sc_result);

  size_t col = 0;
  int rc = 0;
  for(;col<sc_result.n_fields();col++){
    size_t start_offset = sc_result.delimiter_offsets()[col];
    size_t end_offset = sc_result.delimiter_offsets()[col+1];
    rc = column.compare(string(
			       sc_result.begin()+start_offset+1,
			       end_offset - start_offset - 1));
    if(rc==0) break;
  }
  if(rc!=0) {
    cerr << "Requested column: " << column << endl;
    cerr << "Available columns: " << string(sc_result.begin()+1,sc_result.length()-1) << endl;
    throw runtime_error("Could not find matching column"); 
  }

  char* head = circbuf_head(cbuf);
  while(head[0]!=-1){
    match_print_line(cbuf,pattern,match_result,col,sc_result);
    head = circbuf_head(cbuf);
  }
  
  try{
    char out[STDOUT_SIZE];
    setvbuf(stdout, out, _IOFBF, STDOUT_SIZE);

    run_select();
    //putc('\n',stdout);

    fflush(stdout);
  } catch(const std::exception& e){
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
    
  return 0;
}

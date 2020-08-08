
#include <select.hpp>
#include <boost/regex.hpp>
#include <CLI/CLI.hpp>
#include "../include/st.hpp"
#include "../include/constants.hpp"
#include <poll.h>

using namespace std;
using namespace st;
using namespace csv;

string normalize_regex(string regex, char delimiter){
  ostringstream s;
  for(size_t i=0;i<regex.size();i++){
    char c = regex[i];
    if(c=='\\'){
      i++;
      continue;
    } else if(c=='.'){
      s << "[^" << delimiter << "]";
    } else {
      s << c;
    }
  }
  return s.str();
}

Context create_context(string csv_path, uint read_size, uint buffer_size){
    if(!csv_path.empty())
      return Context(csv_path, read_size, buffer_size);

    struct pollfd pfd = { fileno(stdin), POLLIN };
    int rc = poll(&pfd,1,POLL_TIMEOUT);
    if(rc > 0 && pfd.revents & POLLIN)
      return Context(stdin, read_size, buffer_size);
    else
      throw runtime_error("Could not read data from file or STDIN");
}

shared_ptr<Matcher> create_matcher(string regex, string matcher_type,
				   char delimiter){
  shared_ptr<Matcher> r(nullptr);

  if(matcher_type == "regex"){
    boost::regex pattern(regex, REGEX_SYNTAX_FLAGS);
    boost::cmatch match_result;
    r = make_shared<Regex_Matcher>(pattern,match_result,delimiter);
  } else if(matcher_type == "bm"){
    r = make_shared<Boyer_Moore_Matcher>(regex,delimiter);
  } else {
    cerr << "Unknown matcher type: " << matcher_type << endl;
    throw runtime_error("Invalid matcher type");
  }
 
  return r;
}

size_t column_index(const Line_Scan& sc_result, string column){
  
  for(size_t col=0;col<sc_result.n_fields();col++){
    string s = string(sc_result.field(col),sc_result.field_size(col));
    int rc = column.compare(s);
    if(rc==0) return col;
  }

  cerr << "Requested column: " << column << endl;
  cerr << "Available columns: " << string(sc_result.begin(),sc_result.length()) << endl;
  throw runtime_error("Could not find matching column"); 
}

shared_ptr<Line_Scan_Printer> create_printer(const Line_Scan& sc_result, char delimiter,
					     vector<string> out_columns){
  shared_ptr<Line_Scan_Printer> r(nullptr);
  if(out_columns.empty()) {
    r = make_shared<Line_Printer>();
    return r;
  }
  vector<size_t> idxs;
  for(const string& column:out_columns){
    size_t idx = column_index(sc_result,column);
    idxs.push_back(idx);
  }
  r = make_shared<Field_Printer>(idxs,delimiter);
  return r;
}

char str2char(string s){
  if(s.size()!=1) throw runtime_error("Cannot convert string to char: " + s);
  return s[0];
}

void run_select(string csv_path,
		string column,
		string regex,
		string match,
		bool complete_match,
		char delimiter,
		vector<string> out_columns,
		size_t read_size,
		size_t buffer_size){
    string pattern;
    string matcher_type;
    
    if(!regex.empty()) {
      matcher_type = "regex";
      pattern = normalize_regex(regex, delimiter);
    }
    else if(!match.empty()){
      matcher_type = "bm";
      pattern = match;
      complete_match = true;
    }
    else throw runtime_error("Could not determine matcher type");
    
    Context c = create_context(csv_path, read_size, buffer_size);
    circbuf* cbuf = c.cbuf();
    linescan* lscan = c.lscan();
    Line_Scan sc_result;

    scan_header(cbuf, delimiter, lscan, sc_result);

    shared_ptr<Matcher> matcher = create_matcher(pattern, matcher_type, delimiter);
    shared_ptr<Line_Scan_Printer> printer = create_printer(sc_result, delimiter, out_columns);

    printer->print(sc_result);

    size_t col = column_index(sc_result, column);

    char* head = circbuf_head_forward(cbuf, sc_result.length());
    while(head[0] != '\0'){
      scan_match_print_line(cbuf,delimiter,*matcher,col,
                            complete_match,lscan,sc_result,*printer);
      head = circbuf_head(cbuf);
    }

    return;
}

void run_cut(string csv_path,
	     char delimiter,
	     vector<string> out_columns,
	     size_t read_size,
	     size_t buffer_size){
  Context c = create_context(csv_path, read_size, buffer_size);
  circbuf* cbuf = c.cbuf();
  linescan* lscan = c.lscan();
  Line_Scan sc_result;

  scan_header(cbuf, delimiter, lscan, sc_result);

  shared_ptr<Line_Scan_Printer> printer = create_printer(sc_result, delimiter, out_columns);
  printer->print(sc_result);

  char* head = circbuf_head_forward(cbuf, sc_result.length());
  while(head[0] != '\0'){
    char* del = simple_scan_right(head,read_size,delimiter);
    if(del==nullptr) throw runtime_error("Could not find delimiter in line");
    scan(head,read_size,NL,delimiter,lscan,sc_result);
    printer->print(sc_result);
    head = circbuf_head_forward(cbuf, sc_result.length());
  }

  return;
}


int main(int argc, const char* argv[]){
  try{

    char out[STDOUT_SIZE];
    setvbuf(stdout, out, _IOFBF, STDOUT_SIZE);

    CLI::App app{"CppCsv"};
    
    uint read_size = 4096;
    string column = "";
    string regex = "";
    string match = "";
    string csv_path = "";
    string delimiter_str = ",";
    vector<string> out_columns;
    bool complete_match = false;

    app.add_option("-d,--delimiter",delimiter_str,
		   "Column delimiter (default '" + delimiter_str + "')");
    app.add_option("--read-size",read_size,"Size of sequential buffer reads (default 4kb)")
      ->transform(CLI::AsSizeValue(false));

    auto select_cmd = app.add_subcommand("select");
    select_cmd->add_option("-c,--column",column,"Column to match")->required();
    select_cmd->add_option("-o,--out-columns",out_columns,
			  "Output columns,separated by ',' (default all)")->delimiter(',');
    select_cmd->add_flag("--complete",complete_match,"Require that fields match entirely (always active for --match)");
    select_cmd->add_option("csv",csv_path,"CSV path");

    auto input_optg = select_cmd->add_option_group("match")->required();
    auto regex_opt =
      input_optg->add_option("-r,--regex",regex,"Regex to match in selected column");
    auto match_opt =
      input_optg->add_option("-m,--match",match,"Exact string to match in selected column");
    regex_opt->excludes(match_opt);
    match_opt->excludes(regex_opt);

    auto cut_cmd = app.add_subcommand("cut");
    cut_cmd->add_option("-c,--columns",out_columns,
			  "Output columns,separated by ',' (default all)")->delimiter(',');
    cut_cmd->add_option("csv",csv_path,"CSV path");

    app.require_subcommand(1);
    CLI11_PARSE(app, argc, argv);

    uint buffer_size = read_size * 1000;
    char delimiter = str2char(delimiter_str);

    if(select_cmd->parsed()){
      run_select(csv_path, column, regex, match,
		 complete_match, delimiter,
		 out_columns, read_size, buffer_size);
    } else if(cut_cmd->parsed()){
      run_cut(csv_path, delimiter, out_columns, read_size, buffer_size);
    } else {
      throw runtime_error("Unknown subcommand");
    }
    
    fflush(stdout);
  } catch(const std::exception& e){
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
  
  return 0;
}

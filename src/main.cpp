
#include <poll.h>

#include <boost/regex.hpp>
#include <CLI/CLI.hpp>

#include <csv/st.hpp>
#include <csv/constants.hpp>
#include <csv/error.hpp>
#include <csv/select.hpp>

using namespace std;
using namespace st;
using namespace csv;


enum class Matcher_Type
  {
   REGEX, BOYER_MOORE
  };

enum class Buffer_Matcher_Type
  {
   LINE, MULTILINE
  };
		   

bool contains_special_chars(const string& regex){
  bool match = false;
  for(const char& c: ".[]{}()\\*+?|^$") {
    match = regex.find(c) != string::npos;
    if(match) return true;
  }
  return false;
}

Circbuf create_circbuf(string csv_path, size_t read_size, size_t buffer_size){
    if(!csv_path.empty())
      return Circbuf(csv_path, read_size, buffer_size);

    struct pollfd pfd = { fileno(stdin), POLLIN };
    int rc = poll(&pfd,1,POLL_TIMEOUT);
    if(rc > 0 && pfd.revents & POLLIN)
      return Circbuf(stdin, read_size, buffer_size);
    else
      throw runtime_error("Could not read data from file or STDIN");
}

unique_ptr<Matcher> create_matcher(Matcher_Type matcher_type,
				   string regex,
				   char delimiter){
  unique_ptr<Matcher> r(nullptr);

  switch(matcher_type){
  case Matcher_Type::REGEX:
    r = make_unique<Onig_Regex_Matcher>(regex,delimiter);
    break;
  case Matcher_Type::BOYER_MOORE:
    r = make_unique<Boyer_Moore_Matcher>(regex,delimiter);
    break;
  default: throw runtime_error("Invalid matcher type");
  }
 
  return r;
}

unique_ptr<Buffer_Matcher> create_buffer_matcher(Buffer_Matcher_Type matcher_type,
						 char delimiter,
						 size_t pattern_field,
						 bool complete_match){
  unique_ptr<Buffer_Matcher> r(nullptr);

  switch(matcher_type){
  case Buffer_Matcher_Type::LINE:
    r = make_unique<Singleline_BMatcher>(delimiter,pattern_field,complete_match);
    break;
  case Buffer_Matcher_Type::MULTILINE:
    r = make_unique<Multiline_BMatcher>(delimiter,pattern_field,complete_match);
    break;
  default: throw runtime_error("Invalid buffer matcher type");
  }

  return r;
}

size_t column_index(const Linescan& sc_result, string column){

  for(size_t col=0;col<sc_result.n_fields();col++){
    string s = string(sc_result.field(col),sc_result.field_size(col));
    int rc = column.compare(s);
    if(rc==0) return col;
  }

  cerr << "Requested column: " << column << endl;
  cerr << "Available columns: " << string(sc_result.begin(),sc_result.length()) << endl;
  throw runtime_error("Could not find matching column"); 
}

unique_ptr<Linescan_Printer> create_printer(const Linescan& sc_result, char delimiter,
					     const vector<string>& out_columns){
  unique_ptr<Linescan_Printer> r(nullptr);
  if(out_columns.empty()) {
    r = make_unique<Line_Printer>();
    return r;
  }
  vector<size_t> idxs;
  for(const string& column:out_columns){
    size_t idx = column_index(sc_result,column);
    idxs.push_back(idx);
  }
  r = make_unique<Field_Printer>(idxs,delimiter);
  return r;
}

inline char str2char(string s){
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
    Matcher_Type matcher_type;
    Buffer_Matcher_Type bmatcher_type;
    
    if(!regex.empty()) {
      matcher_type = Matcher_Type::REGEX;
      pattern = regex;
    }
    else if(!match.empty()){
      matcher_type = Matcher_Type::BOYER_MOORE;
      pattern = match;
      complete_match = true;
    }
    else throw runtime_error("Could not determine matcher type");

    if(matcher_type == Matcher_Type::REGEX && (!(contains_special_chars(regex)))){
      matcher_type = Matcher_Type::BOYER_MOORE;
    }
    
    if(matcher_type == Matcher_Type::BOYER_MOORE && pattern.size() > 3){
      bmatcher_type = Buffer_Matcher_Type::MULTILINE;
    } else {
      bmatcher_type = Buffer_Matcher_Type::LINE;
    }
    
    Circbuf cbuf = create_circbuf(csv_path, read_size, buffer_size);
    Linescan lscan(delimiter, read_size);

    lscan.do_scan_header(cbuf.head(), cbuf.read_size());

    unique_ptr<Matcher> matcher = create_matcher(matcher_type, pattern, delimiter);
    unique_ptr<Linescan_Printer> printer = create_printer(lscan, delimiter, out_columns);

    printer->print(lscan);

    size_t col = column_index(lscan, column);

    unique_ptr<Buffer_Matcher> bmatcher = create_buffer_matcher(bmatcher_type,
								delimiter, col, complete_match);

    cbuf.advance_head(lscan.length());
    while(!cbuf.at_eof()){
      bool match = bmatcher->do_search(cbuf, *matcher, lscan);
      if(match) printer->print(lscan);
    }

    return;
}

void run_cut(string csv_path,
	     char delimiter,
	     vector<string> out_columns,
	     size_t read_size,
	     size_t buffer_size){
  Circbuf cbuf = create_circbuf(csv_path, read_size, buffer_size);
  Linescan lscan(delimiter, read_size);

  lscan.do_scan_header(cbuf.head(), cbuf.read_size());

  unique_ptr<Linescan_Printer> printer = create_printer(lscan, delimiter, out_columns);
  printer->print(lscan);

  cbuf.advance_head(lscan.length());
  while(!cbuf.at_eof()){
    char* head = cbuf.head();
    char* del = simple_scan_right(head,read_size,delimiter);
    if(del==nullptr) throw runtime_error("Could not find delimiter in line");
    lscan.do_scan(head,read_size);
    printer->print(lscan);
    cbuf.advance_head(lscan.length());
  }
  return;
}


int main(int argc, const char* argv[]){
  try{

    char out[STDOUT_SIZE];
    setvbuf(stdout, out, _IOFBF, STDOUT_SIZE);

    CLI::App app{"CppCsv"};
    
    size_t read_size = 4096 * 4;
    string column = "";
    string regex = "";
    string match = "";
    string csv_path = "";
    string delimiter_str = ",";
    vector<string> out_columns;
    bool complete_match = false;

    app.add_option("-d,--delimiter",delimiter_str,
		   "Column delimiter (default '" + delimiter_str + "')");
    app.add_option("--read-size",read_size,"Size of sequential buffer reads (default 16kb)")
      ->transform(CLI::AsSizeValue(false))
      ->check(CLI::PositiveNumber);

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

    size_t buffer_size = read_size * 1000;
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
    exit_error(e.what());
    return 1;
  }
  
  return 0;
}

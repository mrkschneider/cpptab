
#include <poll.h>

#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <CLI/CLI.hpp>

#include <csv/st.hpp>
#include <csv/constants.hpp>
#include <csv/error.hpp>
#include <csv/match.hpp>
#include <csv/print.hpp>

using namespace std;
using namespace st;
using namespace csv;

static const char ARG_DELIMITER = ',';

enum class Matcher_Type
  {
   REGEX, BOYER_MOORE
  };

enum class Buffer_Matcher_Type
  {
   LINE, MULTILINE
  };

enum class Join_Mode
  {
   NATURAL, LEFT, RIGHT, FULL
  };

static const map<string,Join_Mode> JOIN_MODES =
  {
   {"natural",Join_Mode::NATURAL},
   {"left",Join_Mode::LEFT},
   {"right",Join_Mode::RIGHT},
   {"full",Join_Mode::FULL}
  };
					       

unique_ptr<Circbuf> create_circbuf(string csv_path, size_t read_size, size_t buffer_size){
  if(!csv_path.empty())
    return make_unique<Circbuf>(csv_path,read_size,buffer_size);
  
  struct pollfd pfd = { fileno(stdin), POLLIN };
  int rc = poll(&pfd,1,POLL_TIMEOUT);
  if(rc > 0 && pfd.revents & POLLIN)
    return make_unique<Circbuf>(stdin, read_size, buffer_size);
  else
    throw runtime_error("Could not read data from file or STDIN");
}

unique_ptr<Matcher> create_matcher(Matcher_Type matcher_type,
				   string regex){
  unique_ptr<Matcher> r(nullptr);

  switch(matcher_type){
  case Matcher_Type::REGEX:
    Onig_Regex_Matcher::initialize();
    std::atexit(Onig_Regex_Matcher::finalize);
    r = make_unique<Onig_Regex_Matcher>(regex);
    break;
  case Matcher_Type::BOYER_MOORE:
    r = make_unique<Boyer_Moore_Matcher>(regex);
    break;
  default: throw runtime_error("Invalid matcher type");
  }
 
  return r;
}

unique_ptr<Buffer_Matcher> create_buffer_matcher(Buffer_Matcher_Type bmatcher_type,
						 Matcher_Type matcher_type,
						 const string& regex,
						 char delimiter,
						 size_t pattern_field,
						 bool complete_match){
  unique_ptr<Buffer_Matcher> r(nullptr);
  unique_ptr<Matcher> m = create_matcher(matcher_type, regex);

  switch(bmatcher_type){
  case Buffer_Matcher_Type::LINE:
    r = make_unique<Singleline_BMatcher>(move(m),delimiter,pattern_field,complete_match);
    break;
  case Buffer_Matcher_Type::MULTILINE:
    r = make_unique<Multiline_BMatcher>(move(m),delimiter,pattern_field,complete_match);
    break;
  default: throw runtime_error("Invalid buffer matcher type");
  }

  return r;
}

vector<unique_ptr<Singleline_BMatcher>> create_shadow_buffer_matchers(
					   Matcher_Type matcher_type,
					   const vector<string>& patterns,
					   const vector<size_t>& pattern_fields,
					   char delimiter,
					   bool complete_match){
  vector<unique_ptr<Singleline_BMatcher>> bmatchers;
  for(size_t i=0;i<patterns.size();i++){
    const string& p = patterns[i];
    size_t col = pattern_fields[i];
    unique_ptr<Matcher> m = create_matcher(matcher_type, p);
    auto ptr = make_unique<Singleline_BMatcher>(move(m),
						delimiter,
						col,
						complete_match); 
    bmatchers.push_back(move(ptr));
  }
  return bmatchers;
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
    r = make_unique<Linescan_Line_Printer>();
    return r;
  }
  vector<size_t> idxs;
  for(const string& column:out_columns){
    size_t idx = column_index(sc_result,column);
    idxs.push_back(idx);
  }
  r = make_unique<Linescan_Field_Printer>(make_unique<Field_Printer>(idxs,
								     delimiter,
								     sc_result.crnl(),
								     true,
								     false));
  return r;
}

void run_select(const string& csv_path,
		const vector<string>& columns,
		const vector<string>& regexs,
		const vector<string>& matchs,
		bool complete_match,
		char delimiter,
		const vector<string>& out_columns,
		int lead_regex_idx,
		size_t read_size,
		size_t buffer_size){
  vector<string> patterns;
  Matcher_Type matcher_type;
  Matcher_Type shadow_matcher_type;
  Buffer_Matcher_Type bmatcher_type;

  // Check input, setup vars
  if(!regexs.empty()) {
    matcher_type = shadow_matcher_type = Matcher_Type::REGEX;
    patterns = regexs;
  }
  else if(!matchs.empty()){
    matcher_type = shadow_matcher_type = Matcher_Type::BOYER_MOORE;
    patterns = matchs;
    complete_match = true;
  }
  else throw runtime_error("Could not determine matcher type");

  if(patterns.size() != columns.size())
    throw runtime_error(str(boost::format("Mismatch: %lu patterns vs. %lu columns")
			    % patterns.size()
			    % columns.size()));
  
  string lead_regex = patterns[lead_regex_idx];
  if(matcher_type == Matcher_Type::REGEX && (!(contains_special_chars(lead_regex)))){
    matcher_type = Matcher_Type::BOYER_MOORE;
  }
    
  if(matcher_type == Matcher_Type::BOYER_MOORE && lead_regex.size() > 3){
    bmatcher_type = Buffer_Matcher_Type::MULTILINE;
  } else {
    bmatcher_type = Buffer_Matcher_Type::LINE;
  }

  // Prepare buffers
  unique_ptr<Circbuf> cbuf = create_circbuf(csv_path, read_size, buffer_size);
  Linescan lscan(delimiter, read_size);

  // Scan and print header
  lscan.do_scan_header(cbuf->head(), cbuf->read_size());
  unique_ptr<Linescan_Printer> printer = create_printer(lscan, delimiter, out_columns);
  printer->print(lscan);

  // Find requested column indexes
  vector<size_t> cols;
  for(const string& col:columns)
    cols.push_back(column_index(lscan, col));

  // Find column index of lead column
  size_t lead_col = cols[lead_regex_idx];

  // Prepare leftover column/match pairs for shadow matchers
  patterns.erase(patterns.begin() + lead_regex_idx);
  cols.erase(cols.begin() + lead_regex_idx);
  
  unique_ptr<Buffer_Matcher> lead_bmatcher = create_buffer_matcher(bmatcher_type,
								   matcher_type,
								   lead_regex,
								   delimiter,
								   lead_col, complete_match);
  
  vector<unique_ptr<Singleline_BMatcher>> shadow_bmatchers = create_shadow_buffer_matchers(
								 shadow_matcher_type,
								 patterns,
								 cols,
								 delimiter,
								 complete_match);
  // Move past header
  cbuf->advance_head(lscan.length());

  // Match loop
  while(!cbuf->at_eof()){
  outer:
    bool match = lead_bmatcher->do_search(*cbuf, lscan);
    if(!match) continue;
    for(size_t i=0;i<shadow_bmatchers.size();i++){
      match = shadow_bmatchers[i]->match(lscan);
      if(!match) goto outer;
    }
    printer->print(lscan);
  }

  return;
}

void run_cut(const string& csv_path,
	     char delimiter,
	     const vector<string>& out_columns,
	     size_t read_size,
	     size_t buffer_size){
  unique_ptr<Circbuf> cbuf = create_circbuf(csv_path, read_size, buffer_size);
  Linescan lscan(delimiter, read_size);

  lscan.do_scan_header(cbuf->head(), cbuf->read_size());

  unique_ptr<Linescan_Printer> printer = create_printer(lscan, delimiter, out_columns);
  printer->print(lscan);

  cbuf->advance_head(lscan.length());
  while(!cbuf->at_eof()){
    char* head = cbuf->head();
    char* del = simple_scan_right(head,read_size,delimiter);
    if(del==nullptr) throw runtime_error("Could not find delimiter in line");
    lscan.do_scan(head,read_size);
    printer->print(lscan);
    cbuf->advance_head(lscan.length());
  }
  return;
}

string multi_key(const vector<string>& fields,
		 const vector<size_t>& idxs){
  string r;
  for(size_t i:idxs) {
    if(i >= fields.size())
      throw runtime_error("Key column not found in table");
    r.append(fields[i]);
    r.append(",");
  }
  return r;
}

vector<size_t> key_column_idxs(const vector<string>& columns,
			       const vector<string>& key_columns){
  vector<size_t> key_cols;
  for(const string& key_column:key_columns){
    size_t key_col = find_idx(columns,key_column);
    if(key_col == columns.size())
      throw runtime_error("Key column not found in table");
    key_cols.push_back(key_col);
  }
  return key_cols;
}

unordered_map<string,size_t> keyed_fields(const Csv& csv,
					  const vector<string>& key_columns){
  unordered_map<string,size_t> r;
  vector<string> columns = csv.columns();
  vector<size_t> key_cols = key_column_idxs(columns, key_columns);
  vector<vector<string>> fieldss = csv.fieldss();
  for(size_t i=0;i<fieldss.size();i++){
    const vector<string>& fields = fieldss[i];
    if(fields.size() == 1 && fields[0] == "") continue;
    string key = multi_key(fields, key_cols);
    r[key] = i;
  }
  
  return r;  
} 

void run_join(Join_Mode join_mode,
	      const string& csv_path_1,
	      const string& csv_path_2,
	      char delimiter_1,
	      char delimiter_2,
	      const vector<string>& key_columns,
	      size_t read_size,
	      size_t buffer_size){
  // Read csv_2
  unique_ptr<Csv> csv_2 = Csv::create(create_circbuf(csv_path_2,
						     read_size,
						     buffer_size),
				      delimiter_2);
  vector<string> columns_2 = csv_2->columns();
  vector<string> empty_line_2;
  for(size_t i=0;i<columns_2.size();i++)
    empty_line_2.push_back("");
  // Get keys and line numbers
  unordered_map<string,size_t> keyed_fields_2 = keyed_fields(*csv_2,key_columns);

  // Prepare buffers for reading csv_1
  unique_ptr<Circbuf> cbuf = create_circbuf(csv_path_1,
					    read_size,
					    buffer_size);
  Linescan lscan(delimiter_1, read_size);

  // Read header of csv_1
  lscan.do_scan_header(cbuf->head(), read_size);

  // Get columns of csv_1
  vector<string> columns_1;
  for(size_t i=0;i<lscan.n_fields();i++)
    columns_1.push_back(lscan.field_str(i));

  // Determine common and specific columns
  set<string> columns_set_1 = set<string>(columns_1.begin(),columns_1.end());
  set<string> columns_set_2 = set<string>(columns_2.begin(),columns_2.end());
  set<string> common_columns_set = intersection_set(columns_set_1,columns_set_2);
  vector<string> specific_columns_2 = columns_2;
  remove_from_vec(specific_columns_2, columns_set_1);

  // Find indexes of specific columns
  vector<size_t> specific_columns_2_idxs;
  for(const string& c:specific_columns_2)
    specific_columns_2_idxs.push_back(find_idx(columns_2,c));
  vector<size_t> columns_2_1_idxs;
  for(const string& c:columns_1)
    columns_2_1_idxs.push_back(find_idx(columns_2,c));

  // Initialize printers
  auto printer_1 = Linescan_Field_Printer(
					  make_unique<Field_Printer>(numbers(0,columns_1.size()),
								     delimiter_1,
								     false, false, false));
  auto printer_2 = Field_Printer(specific_columns_2_idxs,delimiter_1,false,true,true);
  auto printer_2_1 = Field_Printer(columns_2_1_idxs,delimiter_1,false,false,false);
  printer_2_1.allow_out_of_bounds(true);

  // Find index of key column in csv_1
  vector<size_t> key_cols = key_column_idxs(columns_1,key_columns);

  // Print header
  printer_1.print(lscan);
  printer_2.print(columns_2);

  // Advance to next line
  cbuf->advance_head(lscan.length());

  std::set<size_t> unused_line_idxs_2;
  for(const auto& it:keyed_fields_2)
    unused_line_idxs_2.insert(it.second);
  
  // Loop through lines
  while(!cbuf->at_eof()){
    char* head = cbuf->head();
    lscan.do_scan(head,read_size);
    if(lscan.length() <= 1){ // Line is empty
      cbuf->advance_head(lscan.length());
      continue;
    }

    string key;
    for(size_t key_col:key_cols){
      if(key_col >= lscan.n_fields()) { // fewer fields than expected for key
	throw runtime_error("Key field missing");
      }
      key.append(lscan.field_str(key_col));
      key.append(",");
    }

    { // Start Block
      unordered_map<string,size_t>::const_iterator it = keyed_fields_2.find(key);
      if(it == keyed_fields_2.end()){ // Key not found in csv_2
	if(join_mode == Join_Mode::LEFT || join_mode == Join_Mode::FULL){
	  printer_1.print(lscan);
	  printer_2.print(empty_line_2);
	}
      } else {
	size_t match_line = it->second;
	unused_line_idxs_2.erase(match_line);
	const vector<string>& match_fields = csv_2->fieldss()[match_line];
	printer_1.print(lscan);
	printer_2.print(match_fields);
      }
    } // End Block
    cbuf->advance_head(lscan.length());
  }

  if(join_mode == Join_Mode::RIGHT || join_mode == Join_Mode::FULL){
    for(size_t idx:unused_line_idxs_2){
      vector<string> match_fields = csv_2->fieldss()[idx];
      if(match_fields.size() == 1 && match_fields[0] == "") continue;
      printer_2_1.print(match_fields);
      printer_2.print(match_fields);
    }
  }
  
}


int main(int argc, const char* argv[]){
  try{
        
    char out[STDOUT_SIZE];
    setvbuf(stdout, out, _IOFBF, STDOUT_SIZE);

    CLI::App app{"tab - tabular processing of CSV files"};
    
    size_t read_size = 4096 * 4;
    string columns_s;
    string regexes_s;
    string matches_s;
    string csv_path = "";
    string delimiter_str = ",";
    string out_columns_s;
    string join_mode = "natural";
    bool complete_match = false;
    string csv_path_2 = "";

    app.add_option("-d,--delimiter",delimiter_str,
		   "Column delimiter (default '" + delimiter_str + "')");
    app.add_option("--read-size",read_size,"Size of sequential buffer reads (default 16kb)")
      ->transform(CLI::AsSizeValue(false))
      ->check(CLI::PositiveNumber);

    auto select_cmd = app.add_subcommand("select");
    select_cmd->add_option("-c,--column",columns_s,"Columns to match, separated by ','")->required();
    select_cmd->add_option("-o,--out-columns",out_columns_s,
			  "Output columns, separated by ',' (default all)");
    select_cmd->add_flag("--complete",complete_match,"Require that fields match entirely (always active for --match)");
    select_cmd->add_option("csv",csv_path,"CSV path");

    auto input_optg = select_cmd->add_option_group("match")->required();
    auto regex_opt =
      input_optg->add_option("-r,--regex",regexes_s,"Regexes to match in selected columns, separated by ','");
    auto match_opt =
      input_optg->add_option("-m,--match",matches_s,"Exact strings to match in selected columns, separated by ','");
    regex_opt->excludes(match_opt);
    match_opt->excludes(regex_opt);

    auto cut_cmd = app.add_subcommand("cut");
    cut_cmd->add_option("-c,--columns",out_columns_s,
			  "Output columns, separated by ',' (default all columns)");
    cut_cmd->add_option("csv",csv_path,"CSV path");

    auto join_cmd = app.add_subcommand("join");
    join_cmd->add_option("-c,--column",columns_s,"Columns to match, separated by ','")->required();
    join_cmd->add_option("-m,--mode",join_mode,"Join mode (either one of 'natural,left,right,full'; default: 'natural')");
    join_cmd->add_option("csv",csv_path,"CSV path 1");
    join_cmd->add_option("csv_2",csv_path_2,"CSV path 2");

    app.require_subcommand(1);
    CLI11_PARSE(app, argc, argv);

    vector<string> regexes = split(regexes_s,ARG_DELIMITER);
    vector<string> matches = split(matches_s,ARG_DELIMITER);
    vector<string> columns = split(columns_s,ARG_DELIMITER);
    vector<string> out_columns = split(out_columns_s,ARG_DELIMITER);

    size_t buffer_size = read_size * 1000;
    char delimiter = str2char(delimiter_str);

    if(select_cmd->parsed()){
      run_select(csv_path, columns, regexes, matches,
		 complete_match, delimiter,
		 out_columns, 0, read_size, buffer_size);
    } else if(cut_cmd->parsed()){
      run_cut(csv_path, delimiter, out_columns, read_size, buffer_size);
    } else if(join_cmd->parsed()){
      if(csv_path_2.empty()){
	csv_path_2 = csv_path;
	csv_path = "";
      }

      map<string,Join_Mode>::const_iterator it = JOIN_MODES.find(join_mode);
      if(it == JOIN_MODES.end()) throw runtime_error("Unknown join mode");
      Join_Mode join_mode_parsed = it->second;
      run_join(join_mode_parsed, csv_path,csv_path_2, delimiter, delimiter, columns, read_size, buffer_size);
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

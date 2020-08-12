#ifndef INCLUDE_CSV_CONSTANTS_HPP_
#define INCLUDE_CSV_CONSTANTS_HPP_

#define STDOUT_SIZE 65535
#define POLL_TIMEOUT 100

#define NL '\n'

#include <boost/regex.hpp>

namespace csv {

  inline const boost::regex_constants::syntax_option_type REGEX_SYNTAX_FLAGS =
    boost::regex_constants::perl;

  inline const boost::regex_constants::match_flag_type REGEX_MATCH_FLAGS =
    boost::regex_constants::match_default |
    boost::regex_constants::match_not_bol |
    boost::regex_constants::match_not_eol |
    boost::regex_constants::match_not_bob |
    boost::regex_constants::match_not_eob |
    boost::regex_constants::match_not_bow |
    boost::regex_constants::match_not_eow |
    boost::regex_constants::match_not_null |
    boost::regex_constants::match_not_dot_newline |
    boost::regex_constants::match_not_dot_null |
    boost::regex_constants::match_nosubs |
    boost::regex_constants::match_any
    ;

  inline const OnigOptionType CSV_ONIG_MATCH_FLAGS =
    ONIG_OPTION_NOTBOL | ONIG_OPTION_NOTEOL
    | ONIG_OPTION_NOT_BEGIN_STRING | ONIG_OPTION_NOT_END_STRING
    | ONIG_OPTION_NOT_BEGIN_POSITION;

  inline const OnigOptionType CSV_ONIG_SYNTAX_OPTIONS =
    ONIG_OPTION_DEFAULT |
    ONIG_OPTION_FIND_NOT_EMPTY;

}
#endif

#ifndef INCLUDE_CSV_CONSTANTS_HPP_
#define INCLUDE_CSV_CONSTANTS_HPP_

#define STDOUT_SIZE 4096 * 100
#define POLL_TIMEOUT 100

#define NL '\n'

#include <boost/regex.hpp>

namespace csv {

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
    boost::regex_constants::match_not_dot_null;

}
#endif

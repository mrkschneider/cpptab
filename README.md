# cpptab - Command line processing of CSV files

## Summary
cpptab is a command line tool for processing CSV files, inspired by [csvkit](https://github.com/wireservice/csvkit) and [xsv](https://github.com/BurntSushi/xsv). It is very much a toy project, aiming at balancing fast runtime speed with clear, maintainable source code. Due to the complexities of character encoding and CSV formats in the wild, bugs are to be expected at this stage.

## Commands
Use tab --help and tab <Subcommand> --help to get a full list of implemented commands and arguments. Input CSV files can be supplied as positional arguments or via STDIN.

* **select:** Print rows with particular columns values. Takes either a regular character string or regular expression.
* **cut:** Print a selection of columns.

## CSV format
Different delimiters can be selected, but are limited to a single character. Strictly speaking, only ASCII encoding is supported, although it should work for most UTF-8 files. Explicit support for different encodings might be added later. Quotes and escape sequences are not supported right now.

## Future plans
Additional possible/planned features include

* Join operations for combining multiple tables
* Different encodings
* Quotes
* Table summary and statistics
* Sorting

## Disclaimer
This is a project I maintain for fun in my free time, with no implied guarantees regarding support, completeness or fitness for any particular purpose. I am grateful for suggestions, bug reports or pull requests, but I might not respond to every request. 



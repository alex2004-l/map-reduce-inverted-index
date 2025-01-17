#ifndef UTILS_HPP
#define UTILS_HPP

#include <algorithm>
#include <queue>
#include <fstream>
#include <cstdlib>
#include <string>
#include <set>
#include <unordered_map>

using namespace std;


/* A function for parsing the initial text file */
/* It returns a queue with all the filenames 
    that need to be processed by mappers */
queue<string> parse_input_file(char *path);

/* Helper function for eliminating non-alphabetically characters */
string process_word(string initial_word);

/* Returns a set with all the unique words from a file */
set<string> parse_file(string file_path);

/* Function for writing the output */
void write_output_file(string filename, unordered_map<string, set<int>> &letter_result);

#endif
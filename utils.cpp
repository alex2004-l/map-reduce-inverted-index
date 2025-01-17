#include "utils.hpp"

using namespace std;


queue<string> parse_input_file(char *path) {
    int num_files;
    string file_name;
    queue<string> result;

    ifstream in(path);
    in >> num_files;
    for (int i = 0; i < num_files; ++i) {
        in >> file_name;
        result.push(file_name);
    }

    in.close();

    return result;
}


string process_word(string initial_word) {
    string result = "";
    for (auto ch : initial_word) {
        if (isalpha(ch))
            result += tolower(ch);
    }
    return result;
}


set<string> parse_file(string file_path) {
    string word, processed_word;
    set<string> unique_words;

    ifstream inputfile(file_path);
    while(inputfile >> word) {
        processed_word = process_word(word);
        if (!processed_word.empty()) {
            unique_words.insert(processed_word);
        }
    }
    return unique_words;
}


void write_output_file(string filename, unordered_map<string, set<int>> &letter_result) {
    ofstream output_file(filename);

    /* Sorting the words beforehand after the number of apparitions and lexicographically afterwards */
    vector<pair<string, set<int>>> sorted_results(letter_result.begin(), letter_result.end());

    sort(sorted_results.begin(), sorted_results.end(), [](const pair<string, set<int>> &a,
                                                            const pair<string, set<int>> &b) {
        if (a.second.size() != b.second.size())
            return a.second.size() > b.second.size();
        return a.first < b.first;
    });

    /* Writing the output formated */
    for(auto &elem : sorted_results) {
        output_file << elem.first << ":[";
        for (auto j = elem.second.begin(); j != elem.second.end(); ++j) {
            if (j != elem.second.begin())
                output_file << " ";
            output_file << *j;
        }
        output_file << "]\n";
    }

    /* Closing the file */
    output_file.close();
}
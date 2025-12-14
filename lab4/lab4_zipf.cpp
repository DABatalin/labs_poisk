#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

const std::string INPUT_FILE = "../data/corpus_final.txt";
const std::string OUTPUT_CSV = "zipf_data.csv";

void to_lower_string(std::string &str) {
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c >= 'A' && c <= 'Z') str[i] = c + 32;
        if (c == 0xD0 && i + 1 < str.size()) {
            unsigned char next = static_cast<unsigned char>(str[i+1]);
            if (next >= 0x90 && next <= 0xAF) {
                if (next <= 0x9F) str[i+1] = next + 0x20; 
                else { str[i] = 0xD1; str[i+1] = next - 0x20; }
            }
        }
    }
}

bool is_alphanum(unsigned char c) {
    if (isalnum(c)) return true;
    if (c > 127) return true;
    return false;
}

int main() {
    std::vector<std::string> all_tokens;
    all_tokens.reserve(10000000); 

    std::ifstream file(INPUT_FILE);
    if (!file.is_open()) {
        std::cerr << "Error opening input file!" << std::endl;
        return 1;
    }

    std::cout << "Reading tokens..." << std::endl;
    std::string line;
    long long total_processed = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        size_t tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        size_t tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;
        
        std::string current_token;
        size_t start_pos = tab2 + 1;
        size_t len = line.size();

        for (size_t i = start_pos; i < len; ++i) {
            unsigned char c = line[i];
            bool is_char_part_of_word = false;

            if (is_alphanum(c)) {
                is_char_part_of_word = true;
            } else {
                bool has_prev = (i > start_pos);
                bool has_next = (i + 1 < len);

                if (c == '.') {
                    if (has_prev && has_next && is_alphanum(line[i-1]) && is_alphanum(line[i+1])) is_char_part_of_word = true;
                }
                else if (c == '-' || c == '+') {
                    if (c == '+' && has_prev && (is_alphanum(line[i-1]) || line[i-1] == '+')) is_char_part_of_word = true;
                    else if (c == '-' && has_prev && has_next && is_alphanum(line[i-1]) && is_alphanum(line[i+1])) is_char_part_of_word = true;
                }
                else if (c == '_') {
                     if (has_prev && has_next && is_alphanum(line[i-1]) && is_alphanum(line[i+1])) is_char_part_of_word = true;
                }
            }

            if (is_char_part_of_word) {
                current_token += c;
            } else {
                if (!current_token.empty()) {
                    to_lower_string(current_token);
                    all_tokens.push_back(current_token);
                    current_token.clear();
                }
            }
        }
        if (!current_token.empty()) {
            to_lower_string(current_token);
            all_tokens.push_back(current_token);
        }

        total_processed++;
        if (total_processed % 1000 == 0) std::cout << "\rDocs processed: " << total_processed << std::flush;
    }
    file.close();

    std::cout << "\nTotal raw tokens: " << all_tokens.size() << std::endl;
    std::cout << "Sorting tokens alphabetically..." << std::endl;
    std::sort(all_tokens.begin(), all_tokens.end());

    std::cout << "Counting frequencies..." << std::endl;
    std::vector<std::pair<std::string, int>> sorted_vocab;
    if (!all_tokens.empty()) {
        int count = 1;
        for (size_t i = 1; i < all_tokens.size(); ++i) {
            if (all_tokens[i] == all_tokens[i-1]) {
                count++;
            } else {
                sorted_vocab.push_back({all_tokens[i-1], count});
                count = 1;
            }
        }
        sorted_vocab.push_back({all_tokens.back(), count});
    }

    std::vector<std::string>().swap(all_tokens);

    std::cout << "Unique terms: " << sorted_vocab.size() << std::endl;
    std::cout << "Sorting by frequency..." << std::endl;

    std::sort(sorted_vocab.begin(), sorted_vocab.end(), 
        [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
            return a.second > b.second; 
        }
    );

    std::cout << "Saving " << OUTPUT_CSV << "..." << std::endl;
    std::ofstream out(OUTPUT_CSV);
    out << "rank,word,frequency\n";
    
    long long rank = 1;
    for (const auto& pair : sorted_vocab) {
        std::string word = pair.first;
        if (word.find(',') != std::string::npos) word = "\"" + word + "\"";
        
        out << rank << "," << word << "," << pair.second << "\n";
        rank++;
    }
    
    std::cout << "Done!" << std::endl;
    return 0;
}

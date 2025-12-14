#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

const std::string INPUT_FILE = "../data/corpus_final.txt";

struct Stats {
    long long total_tokens = 0;
    long long total_token_chars = 0;
    long long total_bytes_processed = 0;
};

size_t count_utf8_chars(const std::string& str) {
    size_t count = 0;
    for (unsigned char c : str) {
        if ((c & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

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

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    tokens.reserve(text.size() / 5);
    
    std::string current_token;
    
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        bool is_char_part_of_word = false;

        if (is_alphanum(c)) {
            is_char_part_of_word = true;
        } 
        else {
            bool has_prev = (i > 0);
            bool has_next = (i + 1 < text.size());
            
            if (c == '.') {
                if (has_prev && has_next && is_alphanum(text[i-1]) && is_alphanum(text[i+1])) {
                    is_char_part_of_word = true;
                }
            }
            else if (c == '-') {
                if (has_prev && has_next && is_alphanum(text[i-1]) && is_alphanum(text[i+1])) {
                    is_char_part_of_word = true;
                }
            }
            else if (c == '+') {
                if (has_prev && (is_alphanum(text[i-1]) || text[i-1] == '+')) {
                    is_char_part_of_word = true;
                }
            }
            else if (c == '_') {
                if (has_prev && has_next && is_alphanum(text[i-1]) && is_alphanum(text[i+1])) {
                    is_char_part_of_word = true;
                }
            }
        }

        if (is_char_part_of_word) {
            current_token += c;
        } else {
            if (!current_token.empty()) {
                to_lower_string(current_token);
                tokens.push_back(current_token);
                current_token.clear();
            }
        }
    }
    
    if (!current_token.empty()) {
        to_lower_string(current_token);
        tokens.push_back(current_token);
    }
    
    return tokens;
}

int main() {
    std::ifstream file(INPUT_FILE);
    if (!file.is_open()) {
        std::cerr << "Error!" << std::endl;
        return 1;
    }

    Stats stats;
    std::string line;
    int debug_count = 0;

    auto start_t = std::chrono::high_resolution_clock::now();

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        size_t tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        size_t tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;
        size_t tab3 = line.find('\t', tab2 + 1);
        
        std::string text_content;
        std::string title_debug;
        
        if (tab3 != std::string::npos) {
            std::string title = line.substr(tab2 + 1, tab3 - tab2 - 1);
            std::string raw_text = line.substr(tab3 + 1);
            text_content = title + " " + raw_text;
            title_debug = title;
        } else {
            text_content = line.substr(tab2 + 1);
            title_debug = "No Title";
        }

        stats.total_bytes_processed += text_content.size();
        std::vector<std::string> tokens = tokenize(text_content);

        if (debug_count < 3) {
            std::cout << "\n=== DOC " << debug_count << ": " << title_debug << " ===" << std::endl;
            std::cout << "TOKENS: ";
            int k = 0;
            for (const auto& t : tokens) {
                std::cout << t << " "; 
                if (++k > 20) break;
            }
            std::cout << "..." << std::endl;
            debug_count++;
        }

        stats.total_tokens += tokens.size();
        for (const auto& t : tokens) {
            stats.total_token_chars += count_utf8_chars(t);
        }
    }
    
    auto end_t = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_t - start_t;
    
    std::cout << "\n=== FINAL STATS ===" << std::endl;
    std::cout << "Tokens: " << stats.total_tokens << std::endl;
    if (stats.total_tokens > 0) {
        std::cout << "Avg Length (chars): " << (double)stats.total_token_chars / stats.total_tokens << std::endl;
    }
    std::cout << "Time: " << elapsed.count() << " s" << std::endl;
    double mb = stats.total_bytes_processed / (1024.0 * 1024.0);
    std::cout << "Speed: " << mb / elapsed.count() << " MB/s" << std::endl;

    return 0;
}

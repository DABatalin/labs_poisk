#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <chrono>

const std::string INPUT_FILE = "../data/corpus_final.txt";
const std::string FORWARD_INDEX_FILE = "../data/docs.bin";
const std::string INVERTED_INDEX_FILE = "../data/index.bin";

bool is_alphanum(unsigned char c) {
    if (isalnum(c)) return true;
    if (c > 127) return true; 
    return false;
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

struct TermEntry {
    std::string term;
    uint32_t doc_id;

    bool operator<(const TermEntry& other) const {
        if (term != other.term) {
            return term < other.term;
        }
        return doc_id < other.doc_id;
    }
};

class Indexer {
private:
    std::vector<TermEntry> entries; 
    uint32_t total_docs = 0;
    
    size_t total_term_len_sum = 0;
    size_t corpus_text_bytes = 0;

public:
    void run() {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::cout << "Phase 1: Parsing Corpus and Building Forward Index..." << std::endl;
        build_forward_index_and_collect_terms();

        std::cout << "Phase 2: Sorting " << entries.size() << " index entries..." << std::endl;
        std::sort(entries.begin(), entries.end());
        
        auto last = std::unique(entries.begin(), entries.end(), [](const TermEntry& a, const TermEntry& b){
            return a.term == b.term && a.doc_id == b.doc_id;
        });
        entries.erase(last, entries.end());

        std::cout << "Phase 3: Writing Inverted Index to disk..." << std::endl;
        write_inverted_index();

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> total_time = end_time - start_time;
        
        print_stats(total_time.count());
    }

private:
    void build_forward_index_and_collect_terms() {
        std::ifstream infile(INPUT_FILE);
        std::ofstream docs_out(FORWARD_INDEX_FILE, std::ios::binary);
        
        if (!infile) { std::cerr << "No corpus file!\n"; exit(1); }
        if (!docs_out) { std::cerr << "Cannot write docs.bin\n"; exit(1); }

        std::vector<uint64_t> doc_offsets;
        
        uint32_t zero = 0;
        docs_out.write((char*)&total_docs, sizeof(total_docs)); 

        std::string docs_data_buffer; 
        
        std::string line;
        while (std::getline(infile, line)) {
            if (line.empty()) continue;
            
            size_t tab1 = line.find('\t');
            size_t tab2 = line.find('\t', tab1 + 1);
            size_t tab3 = line.find('\t', tab2 + 1);
            
            if (tab3 == std::string::npos) continue;

            std::string url = line.substr(tab1 + 1, tab2 - tab1 - 1);
            std::string title = line.substr(tab2 + 1, tab3 - tab2 - 1);
            std::string text = line.substr(tab3 + 1);

            doc_offsets.push_back(docs_data_buffer.size());
            
            uint16_t u_len = (uint16_t)url.size();
            docs_data_buffer.append((char*)&u_len, 2);
            docs_data_buffer.append(url);
            
            uint16_t t_len = (uint16_t)title.size();
            docs_data_buffer.append((char*)&t_len, 2);
            docs_data_buffer.append(title);
            
            corpus_text_bytes += text.size();
            tokenize_and_add(text, total_docs);

            total_docs++;
            if (total_docs % 2000 == 0) std::cout << "\rProcessed " << total_docs << " docs..." << std::flush;
        }
        std::cout << "\n";

        docs_out.seekp(0);
        docs_out.write((char*)&total_docs, 4);
        
        uint64_t data_start_pos = 4 + (uint64_t)total_docs * 8;
        
        for (uint64_t &off : doc_offsets) {
            off += data_start_pos;
            docs_out.write((char*)&off, 8);
        }
        
        docs_out.write(docs_data_buffer.data(), docs_data_buffer.size());
        docs_out.close();
    }

    void tokenize_and_add(const std::string& text, uint32_t doc_id) {
        std::string current_token;
        size_t len = text.size();

        for (size_t i = 0; i < len; ++i) {
            unsigned char c = text[i];
            bool is_word = false;

            if (is_alphanum(c)) {
                is_word = true;
            } else {
                if (c == '.' && i > 0 && i+1 < len && is_alphanum(text[i-1]) && is_alphanum(text[i+1])) is_word = true;
                else if ((c == '-' || c == '+') && i > 0 && (is_alphanum(text[i-1]) || text[i-1]=='+')) is_word = true;
                else if (c == '_' && i > 0 && i+1 < len && is_alphanum(text[i-1]) && is_alphanum(text[i+1])) is_word = true;
            }

            if (is_word) {
                current_token += c;
            } else {
                if (!current_token.empty()) {
                    to_lower_string(current_token);
                    entries.push_back({current_token, doc_id});
                    current_token.clear();
                }
            }
        }
        if (!current_token.empty()) {
             to_lower_string(current_token);
             entries.push_back({current_token, doc_id});
        }
    }

    void write_inverted_index() {
        std::ofstream idx_out(INVERTED_INDEX_FILE, std::ios::binary);
        if (!idx_out) { std::cerr << "Error writing index.bin\n"; exit(1); }

        std::vector<char> dict_buffer;
        std::vector<char> post_buffer;
        
        uint32_t unique_terms_count = 0;
        
        if (entries.empty()) return;

        size_t i = 0;
        size_t n = entries.size();
        
        while (i < n) {
            std::string term = entries[i].term;
            uint32_t doc_freq = 0;
            
            uint64_t rel_offset = post_buffer.size();
            
            while (i < n && entries[i].term == term) {
                 uint32_t did = entries[i].doc_id;
                 const char* did_ptr = (const char*)&did;
                 post_buffer.insert(post_buffer.end(), did_ptr, did_ptr + 4);
                 doc_freq++;
                 i++;
            }
            
            if (term.size() > 255) term.resize(255);
            uint8_t t_len = (uint8_t)term.size();
            
            dict_buffer.push_back((char)t_len);
            dict_buffer.insert(dict_buffer.end(), term.begin(), term.end());
            const char* df_ptr = (const char*)&doc_freq;
            dict_buffer.insert(dict_buffer.end(), df_ptr, df_ptr + 4);
            const char* off_ptr = (const char*)&rel_offset;
            dict_buffer.insert(dict_buffer.end(), off_ptr, off_ptr + 8);
            
            unique_terms_count++;
            total_term_len_sum += term.size();
        }

        uint64_t dict_size = dict_buffer.size();
        
        idx_out.write((char*)&unique_terms_count, 4);
        idx_out.write((char*)&dict_size, 8);
        
        idx_out.write(dict_buffer.data(), dict_buffer.size());
        idx_out.write(post_buffer.data(), post_buffer.size());
        
        idx_out.close();
    }

    void print_stats(double seconds) {
        std::cout << "\n=== INDEXING REPORT ===\n";
        std::cout << "Documents: " << total_docs << "\n";
        std::cout << "Total Time: " << seconds << " s\n";
        
        double speed_doc = (total_docs > 0) ? (seconds / total_docs) : 0;
        double speed_kb = (corpus_text_bytes > 0) ? (corpus_text_bytes / 1024.0 / seconds) : 0;
        
        std::cout << "Avg time per doc: " << speed_doc * 1000 << " ms\n";
        std::cout << "Indexing Speed: " << speed_kb << " KB/s\n";
    }
};

int main() {
    Indexer idx;
    idx.run();
    return 0;
}

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <chrono>

const std::string INPUT_FILE = "../data/corpus_final.txt";

bool ends_with(const std::string& word, const std::string& suffix) {
    if (word.length() < suffix.length()) return false;
    return word.compare(word.length() - suffix.length(), suffix.length(), suffix) == 0;
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

std::string stem_word(std::string word) {
    to_lower_string(word);
    
    if (word.size() <= 4) return word; 

    static const std::vector<std::string> ru_suffixes = {
        "вшимися", "вшими", "вшем", "вшего", "вшая", "вшие", "вшую", "вшим",
        "щимися", "щими", "вше", "вши",
        "иеся", "аяся", "оеся", "ыеся", "имися", "ымися",
        "ившийся", "ывшийся", "ившись", "ывшись",
        "уюся", "ююся", "авше", "авши", "евше", "евши",
        "ившая", "ывшая", "ившее", "ывшее", "ившие", "ывшие",
        "ивший", "ывший", "ившую", "ывшую", "ившим", "ывшим",
        "ующая", "юющая", "ующее", "юющее", "ующие", "юющие",
        "ующий", "юющий", "ующую", "юющую", "ующим", "юющим",
        "авшем", "авшего", "авшую", "авшим", "евшем", "евшего", "евшую", "евшим",
        "ки", "ие", "ые", "ое", "ий", "ый", "ой", "ей", "уй", "ая", "яя", "ою", "ею",
        "ями", "ами", "ье", "иям", "иях", "ием", "иев", "ей", "ям", "ем", "ам", "ом", 
        "ах", "ях", "ых", "их", "ов", "ев", "ью",
        "ешь", "ете", "ишь", "ите", "ят", "ут", "ют", "ит", "ет", "ть", "ти", "л", "ла", "ло", "ли",
        "а", "е", "и", "о", "у", "ы", "э", "ю", "я", "й", "ь"
    };

    for (const auto& suffix : ru_suffixes) {
        if (ends_with(word, suffix)) {
            if (word.length() - suffix.length() >= 3) { 
                return word.substr(0, word.length() - suffix.length());
            }
        }
    }

    static const std::vector<std::string> en_suffixes = {
        "ational", "tional", "enci", "anci", "izer", "bli", "alli", "entli", "eli", "ousli", 
        "ization", "ation", "ator", "alism", "iveness", "fulness", "ousness", "aliti", "iviti", "biliti", 
        "logi", "icate", "ative", "alize", "iciti", "ical", "ful", "ness", 
        "ing", "ed", "es", "ly", "s"
    };

    for (const auto& suffix : en_suffixes) {
        if (ends_with(word, suffix)) {
             if (word.length() - suffix.length() >= 3) { 
                return word.substr(0, word.length() - suffix.length());
            }
        }
    }

    return word;
}

int main() {
    std::string query_word;
    std::cout << "Enter a search term (single word) to test Lemmatization: ";
    std::cin >> query_word;
    
    if (query_word.empty()) return 0;

    std::string q_stem = query_word;
    to_lower_string(query_word); 
    q_stem = stem_word(query_word); 

    std::cout << "\nAnalyzing...\n";
    std::cout << "Original Query: [" << query_word << "]\n";
    std::cout << "Stemmed Query:  [" << q_stem << "]\n\n";

    std::ifstream file(INPUT_FILE);
    if (!file.is_open()) {
        std::cerr << "Error opening corpus!\n";
        return 1;
    }

    long long exact_matches = 0;
    long long stemmed_matches = 0;
    long long total_docs = 0;
    
    std::vector<std::string> new_forms_found; 

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        size_t tab1 = line.find('\t'); if(tab1==std::string::npos) continue;
        size_t tab2 = line.find('\t', tab1+1); if(tab2==std::string::npos) continue;
        size_t tab3 = line.find('\t', tab2+1);
        
        std::string text_content;
        if (tab3 != std::string::npos) {
            text_content = line.substr(tab2 + 1); 
        } else {
            text_content = line.substr(tab2 + 1);
        }

        std::string current;
        bool doc_has_exact = false;
        bool doc_has_stemmed = false;
        
        for (size_t i = 0; i < text_content.size(); ++i) {
            unsigned char c = text_content[i];
            bool is_alpha = isalnum(c) || c > 127;
            
            if (is_alpha) {
                current += c;
            } else {
                if (!current.empty()) {
                    to_lower_string(current);
                    
                    if (current == query_word) {
                        doc_has_exact = true;
                    }
                    
                    std::string s_curr = stem_word(current);
                    if (s_curr == q_stem) {
                        doc_has_stemmed = true;
                        if (current != query_word && new_forms_found.size() < 5) {
                            bool exists = false;
                            for(const auto& w : new_forms_found) if(w == current) exists = true;
                            if(!exists) new_forms_found.push_back(current);
                        }
                    }
                    current.clear();
                }
            }
        }
        
        if (doc_has_exact) exact_matches++;
        if (doc_has_stemmed) stemmed_matches++;
        
        total_docs++;
        if (total_docs % 5000 == 0) std::cout << "\rScanned " << total_docs << " docs..." << std::flush;
    }
    
    std::cout << "\rScan complete. Total: " << total_docs << "\n";
    std::cout << "------------------------------------------------\n";
    std::cout << "RESULTS for query '" << query_word << "':\n";
    std::cout << "1. Exact Match Documents:  " << exact_matches << "\n";
    std::cout << "2. Stemmed Match Documents: " << stemmed_matches << "\n";
    
    if (stemmed_matches > exact_matches) {
        std::cout << "   -> IMPACT: Found " << (stemmed_matches - exact_matches) << " additional documents!\n";
        std::cout << "   -> Example words found thanks to stemming: ";
        for (const auto& w : new_forms_found) std::cout << w << ", ";
        std::cout << "\n";
    } else {
        std::cout << "   -> No improvement (maybe the word is unchangeable or rare).\n";
    }

    return 0;
}

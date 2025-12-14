#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stack>
#include <sstream>
#include <cstring>

const std::string DOCS_FILE = "../data/docs.bin";
const std::string INDEX_FILE = "../data/index.bin";

struct DocInfo
{
    uint64_t offset;
};

struct TermInfo
{
    std::string term;
    uint32_t doc_freq;
    uint64_t postings_offset;

    bool operator<(const TermInfo &other) const
    {
        return term < other.term;
    }
    bool operator<(const std::string &val) const
    {
        return term < val;
    }
};

bool is_alphanum(unsigned char c)
{
    if (isalnum(c))
        return true;
    if (c > 127)
        return true;
    return false;
}

void to_lower_string(std::string &str)
{
    for (size_t i = 0; i < str.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c >= 'A' && c <= 'Z')
            str[i] = c + 32;
        if (c == 0xD0 && i + 1 < str.size())
        {
            unsigned char next = static_cast<unsigned char>(str[i + 1]);
            if (next >= 0x90 && next <= 0xAF)
            {
                if (next <= 0x9F)
                    str[i + 1] = next + 0x20;
                else
                {
                    str[i] = 0xD1;
                    str[i + 1] = next - 0x20;
                }
            }
        }
    }
}

class SearchEngine
{
private:
    std::vector<TermInfo> dictionary;
    std::ifstream idx_in;
    std::ifstream docs_in;

    uint32_t total_docs = 0;
    std::vector<uint64_t> doc_offsets;

    uint64_t postings_start_pos = 0;

public:
    SearchEngine()
    {
        idx_in.open(INDEX_FILE, std::ios::binary);
        docs_in.open(DOCS_FILE, std::ios::binary);

        if (!idx_in || !docs_in)
        {
            std::cerr << "CRITICAL ERROR: Could not open index files. Run Lab 6 first.\n";
            exit(1);
        }

        load_docs_index();
        load_dictionary();
    }

    void load_docs_index()
    {
        docs_in.seekg(0);
        docs_in.read((char *)&total_docs, 4);
        doc_offsets.resize(total_docs);
        docs_in.read((char *)doc_offsets.data(), total_docs * 8);
    }

    void load_dictionary()
    {
        uint32_t num_terms;
        uint64_t dict_size;

        idx_in.seekg(0);
        idx_in.read((char *)&num_terms, 4);
        idx_in.read((char *)&dict_size, 8);

        postings_start_pos = 12 + dict_size;

        std::vector<char> dict_buffer(dict_size);
        idx_in.read(dict_buffer.data(), dict_size);

        dictionary.reserve(num_terms);

        size_t ptr = 0;
        while (ptr < dict_size)
        {
            uint8_t t_len = (uint8_t)dict_buffer[ptr];
            ptr++;
            std::string t_str(dict_buffer.data() + ptr, t_len);
            ptr += t_len;

            uint32_t d_freq;
            memcpy(&d_freq, dict_buffer.data() + ptr, 4);
            ptr += 4;

            uint64_t p_off;
            memcpy(&p_off, dict_buffer.data() + ptr, 8);
            ptr += 8;

            dictionary.push_back({t_str, d_freq, p_off});
        }
    }

    std::vector<uint32_t> get_postings(const std::string &raw_term)
    {
        std::string term = raw_term;
        to_lower_string(term);

        auto it = std::lower_bound(dictionary.begin(), dictionary.end(), term);

        if (it != dictionary.end() && it->term == term)
        {
            std::vector<uint32_t> result(it->doc_freq);

            uint64_t abs_offset = postings_start_pos + it->postings_offset;
            idx_in.seekg(abs_offset);
            idx_in.read((char *)result.data(), it->doc_freq * 4);

            return result;
        }
        return {};
    }

    std::vector<uint32_t> op_and(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
    {
        std::vector<uint32_t> res;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size())
        {
            if (a[i] < b[j])
                i++;
            else if (b[j] < a[i])
                j++;
            else
            {
                res.push_back(a[i]);
                i++;
                j++;
            }
        }
        return res;
    }

    std::vector<uint32_t> op_or(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
    {
        std::vector<uint32_t> res;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size())
        {
            if (a[i] < b[j])
            {
                res.push_back(a[i]);
                i++;
            }
            else if (b[j] < a[i])
            {
                res.push_back(b[j]);
                j++;
            }
            else
            {
                res.push_back(a[i]);
                i++;
                j++;
            }
        }
        while (i < a.size())
            res.push_back(a[i++]);
        while (j < b.size())
            res.push_back(b[j++]);
        return res;
    }

    std::vector<uint32_t> op_not(const std::vector<uint32_t> &a)
    {
        std::vector<uint32_t> res;
        size_t i = 0;
        for (uint32_t doc_id = 0; doc_id < total_docs; ++doc_id)
        {
            if (i < a.size() && a[i] == doc_id)
            {
                i++;
            }
            else
            {
                res.push_back(doc_id);
            }
        }
        return res;
    }

    int precedence(const std::string &op)
    {
        if (op == "!")
            return 3;
        if (op == "&&")
            return 2;
        if (op == "||")
            return 1;
        return 0;
    }

    std::vector<uint32_t> execute_query(std::string query)
    {
        std::vector<std::string> tokens;
        std::string current;

        for (size_t i = 0; i < query.size(); ++i)
        {
            char c = query[i];
            if (c == ' ' || c == '(' || c == ')' || c == '!' || c == '&' || c == '|')
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }

                if (c == '&' && i + 1 < query.size() && query[i + 1] == '&')
                {
                    tokens.push_back("&&");
                    i++;
                }
                else if (c == '|' && i + 1 < query.size() && query[i + 1] == '|')
                {
                    tokens.push_back("||");
                    i++;
                }
                else if (c != ' ')
                {
                    std::string op(1, c);
                    tokens.push_back(op);
                }
            }
            else
            {
                current += c;
            }
        }
        if (!current.empty())
            tokens.push_back(current);

        std::vector<std::string> fixed_tokens;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            fixed_tokens.push_back(tokens[i]);
            if (i + 1 < tokens.size())
            {
                std::string t1 = tokens[i];
                std::string t2 = tokens[i + 1];
                bool t1_is_op = (t1 == "&&" || t1 == "||" || t1 == "!" || t1 == "(");
                bool t2_is_op = (t2 == "&&" || t2 == "||" || t2 == ")" || t2 == "!");

                bool t1_is_val = (t1 != "&&" && t1 != "||" && t1 != "!" && t1 != "(");
                bool t2_is_val = (t2 != "&&" && t2 != "||" && t2 != ")");

                if (t1_is_val && t2_is_val)
                {
                    fixed_tokens.push_back("&&");
                }
            }
        }

        std::vector<std::string> rpn;
        std::stack<std::string> ops;

        for (const auto &t : fixed_tokens)
        {
            if (t == "&&" || t == "||" || t == "!")
            {
                while (!ops.empty() && ops.top() != "(" && precedence(ops.top()) >= precedence(t))
                {
                    rpn.push_back(ops.top());
                    ops.pop();
                }
                ops.push(t);
            }
            else if (t == "(")
            {
                ops.push(t);
            }
            else if (t == ")")
            {
                while (!ops.empty() && ops.top() != "(")
                {
                    rpn.push_back(ops.top());
                    ops.pop();
                }
                if (!ops.empty())
                    ops.pop();
            }
            else
            {
                rpn.push_back(t);
            }
        }
        while (!ops.empty())
        {
            rpn.push_back(ops.top());
            ops.pop();
        }

        std::stack<std::vector<uint32_t>> eval_stack;

        for (const auto &t : rpn)
        {
            if (t == "&&")
            {
                if (eval_stack.size() < 2)
                    continue;
                auto b = eval_stack.top();
                eval_stack.pop();
                auto a = eval_stack.top();
                eval_stack.pop();
                eval_stack.push(op_and(a, b));
            }
            else if (t == "||")
            {
                if (eval_stack.size() < 2)
                    continue;
                auto b = eval_stack.top();
                eval_stack.pop();
                auto a = eval_stack.top();
                eval_stack.pop();
                eval_stack.push(op_or(a, b));
            }
            else if (t == "!")
            {
                if (eval_stack.empty())
                    continue;
                auto a = eval_stack.top();
                eval_stack.pop();
                eval_stack.push(op_not(a));
            }
            else
            {
                eval_stack.push(get_postings(t));
            }
        }

        if (eval_stack.empty())
            return {};
        return eval_stack.top();
    }

    struct PrintResult
    {
        std::string url;
        std::string title;
    };

    PrintResult get_doc_details(uint32_t doc_id)
    {
        if (doc_id >= doc_offsets.size())
            return {"", ""};

        docs_in.seekg(doc_offsets[doc_id]);

        uint16_t u_len;
        docs_in.read((char *)&u_len, 2);
        std::string url(u_len, ' ');
        docs_in.read(&url[0], u_len);

        uint16_t t_len;
        docs_in.read((char *)&t_len, 2);
        std::string title(t_len, ' ');
        docs_in.read(&title[0], t_len);

        return {url, title};
    }
};

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    SearchEngine engine;

    if (argc > 1 && std::string(argv[1]) == "--cli")
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty())
                continue;
            auto results = engine.execute_query(line);
            std::cout << "Query: " << line << " Found: " << results.size() << "\n";
            for (size_t i = 0; i < std::min((size_t)5, results.size()); ++i)
            {
                auto doc = engine.get_doc_details(results[i]);
                std::cout << "  " << doc.title << " (" << doc.url << ")\n";
            }
            std::cout << "-----------------------\n";
        }
    }
    else if (argc > 2 && std::string(argv[1]) == "--web")
    {
        std::string query = argv[2];
        int offset = (argc > 3) ? std::stoi(argv[3]) : 0;
        int limit = (argc > 4) ? std::stoi(argv[4]) : 50;

        auto start = std::chrono::high_resolution_clock::now();
        auto results = engine.execute_query(query);
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << results.size() << "\n";
        std::cout << time_ms << "\n";

        for (int i = offset; i < offset + limit && i < results.size(); ++i)
        {
            auto doc = engine.get_doc_details(results[i]);
            std::replace(doc.title.begin(), doc.title.end(), '\n', ' ');
            std::cout << doc.url << "\t" << doc.title << "\n";
        }
    }
    else
    {
        std::cout << "Usage:\n";
        std::cout << "  ./searcher --cli < queries.txt\n";
        std::cout << "  ./searcher --web \"query string\" offset limit\n";
    }

    return 0;
}

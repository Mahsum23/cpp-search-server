#include "string_processing.h"
#include "read_input_functions.h"

using namespace std;

vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }
    return words;
}

vector<string_view> SplitIntoWordsView(string_view str) {
    vector<string_view> result;
    const uint64_t pos_end = str.npos;
    while (true) {
        if (str.find(' ') == pos_end)
        {
            result.push_back(str);
            return result;
        }
        result.push_back(str.substr(0, str.find(' ')));
        str.remove_prefix(str.find(' ') + 1);
    }

    return result;
}

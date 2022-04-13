#include "test_example_functions.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
    map<set<string_view>, int> count_docs;
    set<int> ids_of_duplicates;

    for (int doc_id : search_server)
    {
        set<string_view> words_in_doc;
        for (auto& [word, _] : search_server.GetWordFrequencies(doc_id))
        {
            words_in_doc.insert(word);
        }
        if (++count_docs[words_in_doc] > 1)
        {
            ids_of_duplicates.insert(doc_id);
        }
    }
    for (int id : ids_of_duplicates)
    {
        search_server.RemoveDocument(id);
    }
}
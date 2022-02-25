#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
    map<map<std::string, double>, int> count_equal_content;
    for (int doc_id : search_server.GetIdsOfDuplicates())
    {
        cout << "Found duplicate document id " << doc_id << endl;
        search_server.RemoveDocument(doc_id);
    }
}
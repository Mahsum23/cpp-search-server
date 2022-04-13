#include "search_server.h"
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&](const auto& query)
        {
            return search_server.FindTopDocuments(query);
        });
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::list<Document> result;
    std::vector<std::vector<Document>> tmp(queries.size());
    tmp = ProcessQueries(search_server, queries);
    
    for (const auto& docs : tmp)
    {
        for (const auto& doc : docs)
        {
            result.push_back(doc);
        }
    }
    return result;
}
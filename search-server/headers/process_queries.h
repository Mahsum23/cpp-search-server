#pragma once

#include "document.h"
//#include "search_server.h"

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <execution>
#include <list>

class SearchServer;
struct Document;

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);


std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);
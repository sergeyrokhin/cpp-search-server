#pragma once
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string_view>& queries);
std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries);
/* набор объектов Document */
std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string_view>& queries);
std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);
std::list<Document> ProcessQueriesJoined_list(    const SearchServer& search_server,    const std::vector<std::string_view>& queries);
std::vector<Document> ProcessQueriesJoined_insert(    const SearchServer& search_server,    const std::vector<std::string_view>& queries);
#include "process_queries.h"
#include <algorithm>
#include <execution>


vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
	vector<vector<Document>> documents_lists(queries.size());
	transform(execution::par, queries.cbegin(), queries.cend(), documents_lists.begin(),
		[&search_server](const auto& query) {
			return search_server.FindTopDocuments(query);
		}
	);
	return documents_lists;
}

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string_view>& queries) {
	vector<vector<Document>> documents_lists(queries.size());
	transform(execution::par, queries.cbegin(), queries.cend(), documents_lists.begin(),
		[&search_server](const auto& query) {
			return search_server.FindTopDocuments(query);
		}
	);
	return documents_lists;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
	list<Document> documents;

	for (auto& documents_list : ProcessQueries(search_server, queries)) {
		documents.insert(documents.end(), documents_list.begin(), documents_list.end());
	}
	return documents;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string_view>& queries) {
	list<Document> documents;

	for (auto& documents_list : ProcessQueries(search_server, queries)) {
		documents.insert(documents.end(), documents_list.begin(), documents_list.end());
	}
	return documents;
}

list<Document> ProcessQueriesJoined_list(const SearchServer& search_server, const vector<string_view>& queries) {
	list<Document> documents;
	for (
		auto& documents_list : ProcessQueries(search_server, queries)
		) {
		documents.insert(documents.end(), make_move_iterator(documents_list.begin()), make_move_iterator(documents_list.end()));
	}
	return documents;
}
vector<Document> ProcessQueriesJoined_insert(const SearchServer& search_server, const vector<string_view>& queries) {
	vector<Document> documents;
	for (
		const auto& documents_list : ProcessQueries(search_server, queries)
		) {
		documents.insert(documents.end(), make_move_iterator(documents_list.begin()), make_move_iterator(documents_list.end()));
	}
	return documents;
}


#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result =
            search_server_.FindTopDocuments(raw_query, document_predicate);
        SaveResult(result.size() == 0);
        return result;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const {
        return count_empty;
    }
private:
    void SaveResult(bool is_empty);
    struct QueryResult {
        QueryResult(bool empty) : empty_request_queue(empty) {}
        bool empty_request_queue;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int count_empty;
    const SearchServer& search_server_;
};

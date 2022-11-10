#include "search_server.h"
#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : count_empty(0), search_server_(search_server) {
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> result =
        search_server_.FindTopDocuments(raw_query, status);
    save_result(result.size() == 0);
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> result =
        search_server_.FindTopDocuments(raw_query);
    save_result(result.size() == 0);
    return result;
}

void RequestQueue::save_result(bool is_empty){
    requests_.emplace_back(QueryResult(is_empty));
    if (is_empty) ++count_empty;
    if (min_in_day_ < requests_.size())
    {
        if (count_empty > 0)
            if (requests_.front().empty_request_queue) --count_empty;
        requests_.pop_front();
    }

}

#include <numeric>
#include <algorithm>
#include "search_server.h"
#include "log_duration.h"

using namespace std;

// Start own function of class SearchServer
SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const std::string_view raw_document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    auto words = SplitIntoWordsNoStop(raw_document);

    auto& doc_id = doc_id_word_freq_[document_id];

    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    for (const auto& word_view : words) {
        string_view word_v = static_cast<std::string_view>(*(all_words_.insert(string(word_view)).first));
        word_to_document_freqs_[word_v][document_id] += inv_word_count;
        doc_id[word_to_document_freqs_.find(word_v)->first] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}
std::set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

std::set<int>::iterator SearchServer::cbegin() const {
    return document_ids_.begin();
}
std::set<int>::iterator SearchServer::cend() const {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty = {};
    return (doc_id_word_freq_.count(document_id) == 0 ? empty : doc_id_word_freq_.at(document_id));
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query,
        [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
            return document_status == status;
        });
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    auto all_words = SplitIntoWordsView(text);
    words.reserve(all_words.size());
    for (const std::string_view word : all_words) {
        if (!IsStopWord(word)) {
            words.emplace_back(word);
        }
    }
    return words;
}

SearchServer::QueryView SearchServer::ParseQueryView(const std::string_view text) const {
    QueryView result;
    auto words = SplitIntoWordsView(text);

    result.plus_words.reserve(words.size());
    result.minus_words.reserve(words.size());
    for (auto& word : words) {
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word.remove_prefix(1);
            if (word.empty() || word[0] == '-') {
                throw std::invalid_argument("Query minus-word "s + string(text) + " is invalid"s);
            }
        }
        else
            if (word.empty()) {
                throw std::invalid_argument("Query word "s + string(text) + " is invalid"s);
            }
        if (IsStopWord(word)) {
            continue;
        }

        if (is_minus) {
            result.minus_words.emplace_back(word);
        }
        else {
            result.plus_words.emplace_back(word);
        }
    }
    return result;
}

MatchOfDocument SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

MatchOfDocument SearchServer::MatchDocument(execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Передан несуществующий document_id "s + to_string(document_id));
    }
    auto query = ParseQueryView(raw_query);

    //отсортирован, уникален
    sort(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.resize(std::distance(query.plus_words.begin(), std::unique(query.plus_words.begin(), query.plus_words.end())));

    for (auto& minus_word : query.minus_words)
        if (word_to_document_freqs_.find(minus_word)->second.count(document_id))
            return { {}, documents_.at(document_id).status };

    vector<string_view> matched_words; //подобранные слова
    matched_words.reserve(query.plus_words.size());
    for_each(query.plus_words.begin(), query.plus_words.end(), [this, document_id, &matched_words](const auto& word) { //перебор слов запроса
        if (word_to_document_freqs_.count(word) != 0) {
            const auto& word_to_document = word_to_document_freqs_.find(word);
            if (word_to_document->second.count(document_id)) { //если слово-запрос в составе документа(document_id)
                matched_words.emplace_back(word_to_document->first);
            }
        }
        });

    sort(matched_words.begin(), matched_words.end());
    matched_words.resize(std::distance(matched_words.begin(), std::unique(matched_words.begin(), matched_words.end())));

    return { matched_words, documents_.at(document_id).status };
}

MatchOfDocument SearchServer::MatchDocument(execution::parallel_policy, const std::string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Передан несуществующий document_id "s + to_string(document_id));
    }
    const auto query = ParseQueryView(raw_query);
    if( //execution::par ,  - хуже
        any_of(query.minus_words.begin(), query.minus_words.end(), [this, document_id](const std::string_view word) {
            const auto it = word_to_document_freqs_.find(word);
                if(it == word_to_document_freqs_.end()) return false;
                if (it->second.find(document_id) == it->second.end()) return false;
                return true;
        }
        )        
      )  return { {}, documents_.at(document_id).status };
        
    vector<string_view> matched_words(query.plus_words.size());

    auto& word_of_doc = doc_id_word_freq_.at(document_id);
    vector<string_view> words;
    words.reserve(word_of_doc.size());
    for (auto& word: word_of_doc) words.emplace_back(word.first);
    

    auto it = copy_if(execution::par, words.begin(), words.end(), matched_words.begin(),
            [&query](const auto& word) { //перебор слов запроса
                if (find(query.plus_words.begin(), query.plus_words.end(), word) == query.plus_words.end())  //если слово-запрос в составе документа(document_id)
                return false;
                else
                return true;
        });
        matched_words.resize(distance(matched_words.begin(), it));

        //отсортирован, уникален
        sort(matched_words.begin(), matched_words.end());
        matched_words.resize(std::distance(matched_words.begin(), std::unique(matched_words.begin(), matched_words.end())));

        return make_pair(matched_words, documents_.at(document_id).status);
}

// Finish for class SearchServer

///*******************
// Start service function for class SearchServer
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        LOG_DURATION("Operation time");
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Error is seaching: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    std::cout << "Матчинг документов по запросу: "s << query << std::endl;
    try {
        LOG_DURATION("Operation time");
        for (auto index = search_server.cbegin(); index != search_server.cend(); ++index) {
            const auto [words, status] = search_server.MatchDocument(query, *index);
            PrintMatchDocumentResult(*index, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
}


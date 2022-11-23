#include <numeric>
#include <algorithm>
#include "search_server.h"
#include "log_duration.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text))  // Invoke delegating constructor from std::string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    auto words = SplitIntoWordsNoStop(document);
    set<string> uniq_words;

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        uniq_words.insert(word);
    }
    int doc_ind = 0;
    for (const std::string& word : uniq_words) {
        doc_ind += (int)word.size();
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
    documents_words.emplace( document_id, words );
    index_documents[doc_ind].emplace(document_id);
    documents_index.emplace(document_id, doc_ind);
    //for (auto word : uniq_words) std::cout << word << " ";
    //std::cout << std::endl << "id = " << document_id << " size = " << doc_ind << std::endl;
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

//int SearchServer::GetDocumentId(int index) const {
//    return document_ids_.at(index);
//}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
            return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}


std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    PrintMatchDocumentResult(document_id, matched_words, documents_.at(document_id).status, std::cerr);
    return { matched_words, documents_.at(document_id).status };
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text + " is invalid"s);
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}


void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        LOG_DURATION_STREAM("Operation time");
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
        LOG_DURATION_STREAM("Operation time");
        for (auto index = search_server.cbegin(); index != search_server.cend(); ++index) {
            const auto [words, status] = search_server.MatchDocument(query, *index);
            PrintMatchDocumentResult(*index, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
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
std::set<int>::iterator SearchServer::cend() const{
    return document_ids_.end();
}

const map<string, double> SearchServer::GetWordFrequencies(int document_id) const {
	map<string, double> result;
	if (documents_.count(document_id))
        for (auto& word : documents_words.at(document_id))
        {
				result.insert({ word, word_to_document_freqs_.at(word).at(document_id) });
			}
	return result;
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id)) {
        documents_.erase(document_id);
        for (auto & word : documents_words.at(document_id))
        {
            word_to_document_freqs_[word].erase(document_id);
        }
        documents_words.erase(document_id);
        auto const & words = documents_index[document_id];
        index_documents[words].erase(document_id);
        documents_index.erase(document_id);
        document_ids_.erase(document_id);
    }
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

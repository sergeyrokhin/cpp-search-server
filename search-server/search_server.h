#pragma once
#include <memory>
#include <stdexcept>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <cmath>
#include <execution>
#include <functional>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int NUMBER_OF_DOCUMENTS_IN_THE_BASKET = 1000;

using MatchOfDocument = std::tuple<std::vector<std::string_view>, DocumentStatus>;

class SearchServer {

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> all_words_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; //<word, <id, freq>>
    std::map<int, std::map<std::string_view, double>> doc_id_word_freq_; //<id, <word, freq>> для метода GetWordFrequencies
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

public:
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
    {
        for (const std::string_view stop_word : stop_words) {
            if ( ! IsValidWord(stop_word)) {
                throw std::invalid_argument("Some of stop words are invalid"s);
            }
            if (!stop_word.empty()) {
                stop_words_.insert(std::string{ stop_word.data(), stop_word.size() });
            }
        }
    }

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status = DocumentStatus::ACTUAL, const std::vector<int>& ratings = {});

    size_t GetDocumentCount() const;

    std::set<int>::iterator begin();
    std::set<int>::iterator end();
    std::set<int>::const_iterator cbegin() const;
    std::set<int>::const_iterator cend() const;

private:
    bool IsStopWord(const std::string& word) const;
    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryView {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    QueryView ParseQueryView(const std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments([[maybe_unused]] std::execution::sequenced_policy par, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        auto query = ParseQueryView(raw_query);
        //отсортирован, уникален
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(std::distance(query.plus_words.begin(), std::unique(query.plus_words.begin(), query.plus_words.end())));

        std::map<int, double> document_to_relevance;
        for (const auto& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            auto& document_freqs = word_to_document_freqs_.find(word)->second;
            const double inverse_document_freq = log(GetDocumentCount() * 1.0 / document_freqs.size());
                //ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : document_freqs) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const auto& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.find(word)->second) {
                document_to_relevance.erase(document_id);
            }
        }

        ////отсортирован, уникален
        //sort(document_to_relevance.begin(), document_to_relevance.end());
        //document_to_relevance.resize(std::distance(document_to_relevance.begin(),
        //    std::unique(document_to_relevance.begin(), document_to_relevance.end())));


        std::vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.emplace_back(
                Document{ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments([[maybe_unused]] std::execution::parallel_policy par, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        auto query = ParseQueryView(raw_query);

        //отсортирован, уникален
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(std::distance(query.plus_words.begin(), std::unique(query.plus_words.begin(), query.plus_words.end())));

        ConcurrentMap<int, double> document_to_relevance(GetDocumentCount() / NUMBER_OF_DOCUMENTS_IN_THE_BASKET + 1);

		for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&document_to_relevance, document_predicate, this](const auto& word) {

            if (word_to_document_freqs_.count(word) == 0) {
				return; //этого плюс-слова в нашем сервере нет
			}

            auto& document_freqs = word_to_document_freqs_.find(word)->second;
	    	const double inverse_document_freq = log(GetDocumentCount() * 1.0 / document_freqs.size());

		    for (const auto& [document_id, term_freq] : document_freqs) { //а это тоже попробуем разогнать, потом
			    const auto& document_data = documents_.at(document_id);
			    if (document_predicate(document_id, document_data.status, document_data.rating)) {
				    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
			    }
		    }
		});

        for (const auto& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.find(word)->second) {
                document_to_relevance.Erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.emplace_back(
                Document{ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

public:
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    MatchOfDocument MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    MatchOfDocument MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;
    MatchOfDocument MatchDocument(const std::string_view raw_query, int document_id) const;

    void RemoveDuplicates();

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy, typename PredicateStatus>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query, PredicateStatus predicate_status) const {
        
        std::vector < Document> matched_documents;

        if constexpr (std::is_same_v<std::decay_t<PredicateStatus>, DocumentStatus>) {
            matched_documents = FindAllDocuments(policy, raw_query, 
                [predicate_status]([[maybe_unused]] int document_id, PredicateStatus status, [[maybe_unused]] int rating) {
                    return predicate_status == status; }
            );
        }
        else{
            matched_documents = FindAllDocuments(policy, raw_query, predicate_status);
        }

        sort(matched_documents.begin(), matched_documents.end(), std::greater<Document>());
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, Predicate predicate) const {
        return FindTopDocuments(std::execution::seq, raw_query, predicate);
    }

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query) const {
        return FindTopDocuments(policy, raw_query,
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) {
                return DocumentStatus::ACTUAL == status;
            });
    }

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy policy, int document_id) {

        std::vector<std::string_view> words;
        words.reserve(doc_id_word_freq_[document_id].size());
        for_each(doc_id_word_freq_[document_id].begin(), doc_id_word_freq_[document_id].end(),
            [&words](auto& elem) { words.emplace_back(elem.first); });

                for_each(policy, words.begin(), words.end(), //map<int, map<string, double>>
                    [this, document_id](auto& word) { //map<string, double>>
                        word_to_document_freqs_.find(word)->second.erase(document_id); //map<string, map<int, double>> 
                        //защита от деления на ноль при вычислении freg
                        auto it_document_freqs = word_to_document_freqs_.find(word);
                        if (it_document_freqs->second.empty())
                            word_to_document_freqs_.erase(it_document_freqs);
                    }
                );
        documents_.erase(document_id);
        document_ids_.erase(document_id);
        doc_id_word_freq_.erase(document_id);
    }
};

void MatchDocuments(const SearchServer& search_server, const std::string& query);
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

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
#include <variant>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"


using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int NUMBER_OF_DOCUMENTS_IN_THE_BASKET = 1000;

using parallel_policy_holder = variant
<
    execution::sequenced_policy,
    execution::parallel_policy,
    execution::parallel_unsequenced_policy
>;

class SearchServer {

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string, less<>> all_words_;
    //deque<string> all_words_;
    set<string, less<>> stop_words_;
    map<string_view, map<int, double>> word_to_document_freqs_; //<word, <id, freq>>
    map<int, map<string_view, double>> doc_id_word_freq_; //<id, <word, freq>> для метода GetWordFrequencies
    map<int, DocumentData> documents_;
    set<int> document_ids_;

public:
    explicit SearchServer(const string& stop_words_text);
    explicit SearchServer(const string_view& stop_words_text);
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
    {
        for (const string_view stop_word : stop_words) {
            if ( ! IsValidWord(stop_word)) {
                throw invalid_argument("Some of stop words are invalid"s);
            }
            if (!stop_word.empty()) {
                stop_words_.insert(string{ stop_word.data(), stop_word.size() });
            }
        }
    }

    void AddDocument(int document_id, const string_view& document, DocumentStatus status = DocumentStatus::ACTUAL, const vector<int>& ratings = {});
    size_t GetDocumentCount() const;
    set<int>::iterator begin();
    set<int>::iterator end();
    set<int>::const_iterator cbegin() const;
    set<int>::const_iterator cend() const;

private:
    bool IsStopWord(const string& word) const;
    bool IsStopWord(const string_view& word) const;
    static bool IsValidWord(const string_view& word);

    vector<string_view> SplitIntoWordsNoStop(const string_view& text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    struct QueryWordView {
        string_view data;
        bool is_minus;
        bool is_stop;
    };
    struct QueryView {
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    };
    QueryView ParseQueryView(const string_view& text) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments([[maybe_unused]] execution::sequenced_policy par, const string_view& raw_query, DocumentPredicate document_predicate) const {
        auto query = ParseQueryView(raw_query);
        //отсортирован, уникален
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(std::distance(query.plus_words.begin(), std::unique(query.plus_words.begin(), query.plus_words.end())));

        map<int, double> document_to_relevance;
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
    vector<Document> FindAllDocuments([[maybe_unused]] execution::parallel_policy par, const string_view& raw_query, DocumentPredicate document_predicate) const {
        auto query = ParseQueryView(raw_query);

        //отсортирован, уникален
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(std::distance(query.plus_words.begin(), std::unique(query.plus_words.begin(), query.plus_words.end())));

        ConcurrentMap<int, double> document_to_relevance(GetDocumentCount() / NUMBER_OF_DOCUMENTS_IN_THE_BASKET + 1);
        //map<int, double> document_to_relevance;

		for_each(execution::par, query.plus_words.begin(), query.plus_words.end(), [&document_to_relevance, document_predicate, this](const auto& word) {
			if (word_to_document_freqs_.count(word) == 0) {
				return; //этого плюс-слова в нашем сервере нет
			}
    		auto& document_freqs = word_to_document_freqs_.find(word)->second;
	    	const double inverse_document_freq = log(GetDocumentCount() * 1.0 / document_freqs.size());
		    //ComputeWordInverseDocumentFreq(word);
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

        ////отсортирован, уникален
        //sort(document_to_relevance.begin(), document_to_relevance.end());
        //document_to_relevance.resize(std::distance(document_to_relevance.begin(),
        //    std::unique(document_to_relevance.begin(), document_to_relevance.end())));
        std::vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.emplace_back(
                Document{ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

public:
    const map<string_view, double> GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(parallel_policy_holder pol, int document_id);

    //tuple<vector<string_view>, DocumentStatus> MatchDocument(parallel_policy_holder pol, const string_view& raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::sequenced_policy, const string_view& raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::parallel_policy, const string_view& raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(const string_view& raw_query, int document_id) const;

    void RemoveDuplicates();

    template <typename ExecutionPolicy, typename DocumentPredicate>
    vector<Document> FindTopDocumentsPr([[maybe_unused]] ExecutionPolicy pol, const string_view& raw_query, DocumentPredicate document_predicate) const {

        auto matched_documents = FindAllDocuments(pol, raw_query, document_predicate);
            
        sort(matched_documents.begin(), matched_documents.end(), std::greater<Document>());
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    template <typename ExecutionPolicy, typename PredicateStatus>
    vector<Document> FindTopDocuments(ExecutionPolicy pol, const string_view& raw_query, PredicateStatus predicate_status) const {
		if constexpr (is_same_v<decay_t<PredicateStatus>, DocumentStatus>) {
			return FindTopDocumentsPr(pol, raw_query, 
                [predicate_status]([[maybe_unused]] int document_id, PredicateStatus status, [[maybe_unused]] int rating) {
					return predicate_status == status;
				});
        }
        else {
            return FindTopDocumentsPr(pol, raw_query, predicate_status);
        }
	}
    template <typename Predicate>
    vector<Document> FindTopDocuments(const string_view& raw_query, Predicate predicate) const {
        return FindTopDocuments(execution::seq, raw_query, predicate);
    }
    template <typename ExecutionPolicy>
    vector<Document> FindTopDocuments(ExecutionPolicy pol, const string_view& raw_query) const {
        return FindTopDocuments(pol, raw_query,
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) {
                return DocumentStatus::ACTUAL == status;
            });
    }
    vector<Document> FindTopDocuments(const string_view& raw_query) const {
        return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
    }
    vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const {
        return FindTopDocuments(execution::seq, raw_query,
            [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating) {
                return document_status == status;
            });
    }


    //vector<Document> FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    //    FindTopDocuments(execution::seq, raw_query, status);
    //}
    //template <typename DocumentPredicate>
    //vector<Document> FindTopDocuments(execution::parallel_policy par, const string_view& raw_query, DocumentPredicate document_predicate) const {
    //    return FindTopDocuments(raw_query, document_predicate);
    //}
    //vector<Document> FindTopDocuments(execution::parallel_policy par, const string_view& raw_query) const {
    //    return FindTopDocuments(raw_query);
    //}

};

void MatchDocuments(const SearchServer& search_server, const string& query);
void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const string& raw_query);

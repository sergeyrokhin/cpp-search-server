#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

   void AddDocument(int document_id, const string& document) {
        const vector<string> &words = SplitIntoWordsNoStop(document);
        for(auto word : words) {
            auto &doc = documents_[word];
            doc[document_id] += (double)1 / (double) words.size();
        };
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const set<string> query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        // sort(matched_documents.begin(), matched_documents.end(),
        //      [](const Document& lhs, const Document& rhs) {
        //          return lhs.relevance > rhs.relevance;
        //      });
        // if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        //     matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        // }
        return matched_documents;
    }

private:
    /// @brief 
    map<string, map<int, double>> documents_; //слово, список ( id документов, количество раз это слово в документе / количество слов в документе) FT.
    //map<string, set<int>> documents_; //слово, список id документов.
    set<string> stop_words_;

    /// @brief количество документов, чтоб не считать каждый раз
    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    set<string> ParseQuery(const string& text) const {
        set<string> query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            query_words.insert(word);
        }
        return query_words;
    }

    vector<Document> FindAllDocuments(const set<string>& query_words) const {
        vector<Document> matched_documents;
        //id документа, количество найденных слов
        map<int, double> result;  //doc_id, relevance
        for (const string & word : query_words) { //каждое слово из запроса
            if(word[0] == '-') continue; // минус слова в следующем цикле
            if( documents_.find(word) == documents_.end()) continue;
            auto &word_docs = documents_.at(word); //находим слово в словаре
            //word_docs.size(); //количество документов с этим словом (без разделения сколько раз)
            //document_count_  //всего документов
            double match = log( (double)document_count_ / (double)word_docs.size()); //значимость слова для поиска т.е. в скольких док. встречается
            //if(match != 0)
            for(auto doc_id : word_docs) { //перебираем документы, где встречается это слово
                //doc_id.second // количество раз слово (word) в этом документе / на количество слов TF
                if(doc_id.second != 0) {
                    result[doc_id.first] += match  * (double) doc_id.second; 
                }
                // добавляем релевантность по IDF * TF
            }
        }
        for (const auto& word : query_words) { //каждое минус-слово из запроса
            if(word[0] != '-') continue; // только минус слова дальше будут обработаны
            string w_temp = word;
            w_temp.erase(0,1);
            if(documents_.count(w_temp) == 0) continue;
            auto &word_docs = documents_.at(w_temp); //находим слово в словаре
            for(auto doc_id : word_docs) { //перебираем документы, где встречается это слово
                result.erase(doc_id.first); // удаляем документ из списка релевантных
            }
        }
        for(auto doc : result) {
            Document doc_new;
            doc_new.id = doc.first;
            doc_new.relevance = doc.second;
            matched_documents.emplace_back(doc_new);
        }
////////////////////////////
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
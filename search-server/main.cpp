#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
inline static constexpr int INVALID_DOCUMENT_ID = -1;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text)
{
    vector<string> words;
    string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

static bool MyIsValidWord(const string &word)
{
    if (word.size() == 0)
        return false;
    if (word[0] == '-')
        return false;
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c)
                   { return c >= '\0' && c < ' '; });
}


struct Document
{
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating)
    {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings)
{
    set<string> non_empty_strings;
    for (const string &str : strings)
    {
        if (!str.empty())
        {
            if (!MyIsValidWord(str))
            {
                throw invalid_argument("недопустимые минус-слова в строке поиска");
            }
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

template <typename StringContainer>
bool WordsAreCorrect(const StringContainer &words, bool empty_test = false)
{
    if (words.empty() && empty_test)  return false;

   return none_of(words.begin(), words.end(), [](const auto word)
                   { return ! MyIsValidWord(word); });    
}

class SearchServer
{
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
    }

    explicit SearchServer(const string &stop_words_text)
        : SearchServer(
              SplitIntoWords(stop_words_text)) // Invoke delegating constructor from string container
    {
    }
    int GetDocumentId(int index) const
    {
        if (!(index >= 0 && static_cast<size_t>(index) < documents_.size())) throw out_of_range("за пределами индекса");
        for (auto it = begin(documents_); it != end(documents_); it++)
        {
            if (0 == index--)
            {
                return (*it).first;
            }
        }
        return INVALID_DOCUMENT_ID; // такого быть не может
    }
    void AddDocument(int document_id, const string &document, DocumentStatus status,
                                   const vector<int> &ratings)
    {
        if (document_id < 0)  throw invalid_argument("отрицательный ID");
            
        if (documents_.count(document_id) != 0) throw invalid_argument("повторный ID");

        const vector<string> words = SplitIntoWordsNoStop(document);
        
        if (!WordsAreCorrect(words)) throw invalid_argument("недопустимые слова в документе");

        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {

        const Query query = ParseQuery(raw_query);
        if (!WordsAreCorrect(query.minus_words))            throw invalid_argument("недопустимые минус-слова в строке поиска");
        if (!WordsAreCorrect(query.plus_words))            throw invalid_argument("недопустимые слова в строке поиска");

        auto result = FindAllDocuments(query, document_predicate);

        sort(result.begin(), result.end(),
             [](const Document &lhs, const Document &rhs)
             {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 else
                 {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return result;
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus document_status, [[maybe_unused]] int rating)
            { return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string &raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus>
    MatchDocument(const string &raw_query, int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        if (!WordsAreCorrect(query.minus_words))            throw invalid_argument("недопустимые минус-слова в строке поиска");
        if (!WordsAreCorrect(query.plus_words))            throw invalid_argument("недопустимые слова в строке поиска");
        

        vector<string> matched_words;
        for (const string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }
        tuple<vector<string>, DocumentStatus> result;
        get<0>(result) = matched_words;
        get<1>(result) = documents_.at(document_id).status;
        return result;
    }

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const
    {
        vector<string> words;
        for (const string &word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings)
        {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const
    {
        Query query;
        for (const string &word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string &word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentPredicate document_predicate) const
    {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto &document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

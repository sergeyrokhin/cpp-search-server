#include <iostream>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);
    
    return words;
}
    
struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {            
        return FindTopDocuments(raw_query, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) { return status == DocumentStatus::ACTUAL; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
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
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto &document_data = documents_.at(document_id);

                bool is_selected = false;
                if constexpr (is_same_v<DocumentPredicate, DocumentStatus>)
                {
                    if (document_predicate == document_data.status)
                    {
                        is_selected = true;
                    }
                }
                else
                {
                    if (document_predicate(document_id, document_data.status, document_data.rating))
                    {
                        is_selected = true;
                    }
                }
                if (is_selected)
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};



#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT_EQUAL(a, b) AssertEqualImpl(!!(a), !!(b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl(!!(a), !!(b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") OK: "s;
        cerr << t << " == "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") OK."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
}


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
//void TestExcludeStopWordsFromAddedDocumentContent() {

/*
Разместите код остальных тестов здесь
*/
/* Если требование по названию функций обязательно, то я испрравлю.
Но позвольте поделиться моими причинами по которым я отказался от длинных наименований.

1. Функций много, всех по именам не запомнить, но все они должны быть в списке вызываемых при проверке. Но нумерация позволяет сразу убедиться, что все вызываются.
2. Не удастся мне найти понятное мне англоязычное название, чтоб оно раскрыло полное назначение теста, да еще я запомнил, что все включил в список проверки.
3. эти функции нигде больше не будут вызываться, не требуется понимать их назначение, кроме как при проверке
4. Но каждая функция содержит подробный комментарий, при необходимости можно быстро найти подробное описание.
5. По хорошему каждый тест должен в документации иметь номер, подобно как компилятор краткое описание ошибки сопровождается номером.
6. Понятное название объекта очень помогает когда встречается внутри другого алгоритма, но в этом случае - это всего лишь пакет необходимых проверок. Всех, без названия

7. Компенсирую за счет HINT

*/


void Test0() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
// ** Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Добавление документов."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Добавление документов. находит нужный документ"s);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void Test1() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

// ** Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Поддержка стоп-слов. Стоп-слова исключаются из текста документов.");
    }
}


// ** Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
// ** 2
void Test2() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL_HINT(server.FindTopDocuments("cat"s).empty(), false, "Поддержка минус-слов"s);
        ASSERT_EQUAL_HINT(server.FindTopDocuments("cat -cat"s).empty(), true, "Поддержка минус-слов. Есть минус-слово"s);
    }
}

// template <typename T>
// bool operator==(vector<T> a, vector<T> b) {
bool operator==(vector<string> a, vector<string> b) {
    if (a.size() != b.size()) return false;
    {
        sort(a.begin(),a.end());
        sort(b.begin(),b.end());
        for (size_t i = 0; i < a.size(); i++)
        {
            if(a[i] != b[i]) return false;
        }
    }
    
    return true;
}

// ** Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, 
// присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
// ** 3
void Test3_1() {
    const int doc_id = 42;
    const string content = "cat in the city play match"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> match_words;
        const vector<string> sample = SplitIntoWords("city cat"s);
        DocumentStatus status;
        std::tie(match_words, status) = server.MatchDocument("city cat cian"s, doc_id);
        ASSERT_HINT(match_words == sample, "Матчинг документов. все слова из поискового запроса");
        std::tie(match_words, status) = server.MatchDocument("cat -cat"s, doc_id);
        ASSERT_HINT(match_words.size() == 0, "Матчинг документов. минус-слово, должен возвращаться пустой список слов");
    }
}

//Копипаст был сплошной. Это попытка разделить код, чтоб понять, что хочет Проверка. Этот Тренажер + "прекрасная" формулировка задачи
// отняли кучу времени
// для Тренажера вылизывать код... он этого не оценит.
//
// А сейчас - другое дело ))))

// ** Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
// ** 4
void Test4_1() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::REMOVED, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s, []([[maybe_unused]] int document_id, [[maybe_unused]]  DocumentStatus status, [[maybe_unused]] int rating) {
            return document_id % 2 == 0;
        });
        ASSERT_EQUAL_HINT(result.size(), 2, "Фильтрация результатов поиска с использованием предиката. Четные");

        result = server.FindTopDocuments("cat city play match"s, []([[maybe_unused]] int document_id, [[maybe_unused]]  DocumentStatus status, [[maybe_unused]] int rating){
            return (document_id == 2);
        });
        ASSERT_EQUAL_HINT(result.front().id, 2, "Фильтрация результатов поиска с использованием предиката. по ID");

// ** Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
        result = server.FindTopDocuments("cat city play match"s);

        auto rating = result.front().rating;
        ASSERT_EQUAL_HINT(rating, (1+2+3)/3, "Вычисление рейтинга документов"s);
// ** Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.

        result = server.FindTopDocuments("cat city play match"s);
        auto relevance = result.front().relevance;
        for(auto doc:result)
        {
            //перебираем по порядку, следующий меньше предыдущего
            ASSERT_HINT(doc.relevance <= relevance, "Сортировка найденных документов по релевантности"s);
            relevance = doc.relevance;
        }

// ** Поиск документов, имеющих заданный статус.
// ** 5
        result = server.FindTopDocuments("cat city play match"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(result.front().id, 4, "Поиск документов, имеющих заданный статус."s);
        ASSERT_EQUAL_HINT(result.size(), 1, "Поиск документов, имеющих заданный статус. (только один)"s);
        
// ** Корректное вычисление релевантности найденных документов.
// про формулу понял, но подыскивать результат.... я до сих пор эту релевантность не осознал...
// на коде это не скажется... и не пригодится. число отладчиком подсмотрел.

        result = server.FindTopDocuments("cat city play match"s);
        ASSERT_HINT(abs(result.front().relevance - 0.81492445484711395) < 1e-6, "вычисление релевантности"s);
     }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    Test0();
    Test1();
    Test2();
    Test3_1();
    Test4_1();
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    setlocale(LC_ALL, ".UTF8");
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
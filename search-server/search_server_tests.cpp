#include "search_server_tests.h"
#include "search_server.h"
#include "process_queries.h"
#include "test_framework.h"
#include <assert.h>
#include <numeric>

// -------- Начало модульных тестов поисковой системы ----------


using namespace std;
// Тест проверяет инициализацию сервера стоп-словами
void TestInitServer() {
    try {
        SearchServer server(""s);
    }  catch (...) {
        ASSERT_HINT (false, "Problems with initializing server with no stop words"s);
    }

    try {
        SearchServer server("и в на"s);
    }  catch (...) {
        ASSERT_HINT (false, "Problems with initializing server with normal stop words"s);
    }

    try {
        SearchServer server("и в на о\x12"s);
        ASSERT_HINT (false, "Server initialized with stop words contain service symbols"s);
    }  catch (...) {}
}

// Тест проверяет, что корректные документы добавляются на сервер, а некорректные - нет
void TestAddDocuments () {
    SearchServer server(""s);
    ASSERT_HINT(server.GetDocumentCount() == 0, "New server is not empty"s);

    try {
        server.AddDocument(0, "белый кот и модный ошейник"s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1, "Server should contain one document"s);
    }  catch (...) {
        ASSERT_HINT (false, "Problems with adding correct document"s);
    }

    try {
        server.AddDocument(1, "пушистый кот пушистый хвост"s);
        server.AddDocument(2, "ухоженный пёс выразительные глаза"s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "Server should contain three documents"s);
    }  catch (...) {
        ASSERT_HINT (false, "Problems with adding correct documents");
    }

    //try {
    //    server.AddDocument(3, ""s);
    //    ASSERT_HINT (false, "Server add empty document"s);
    //}  catch (...) {}
    try {
        server.AddDocument(3, ""s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "Server add empty document"s);
    }  catch (...) {
        ASSERT_HINT(false, "Problems with add empty document"s);
    }

    try {
        server.AddDocument(2, "поющая птичка невеличка"s);
        ASSERT_HINT (false, "Server add document with id already exist"s);
    }  catch (...) {}

    try {
        server.AddDocument(-1, "поющая птичка невеличка"s);
        ASSERT_HINT (false, "Server add document with negative id"s);
    }  catch (...) {}

    try {
        server.AddDocument(4, "большой пёс скво\x12рец"s);
        ASSERT_HINT (false, "Server add documents with service symbols"s);
    }  catch (...) {}
}

// Тест проверяет удаление документа
void TestRemoveDocument() {
    // проверка обычной версии
    {
        SearchServer server(""s);
        server.AddDocument(0, "белый кот и модный ошейник"s);
        server.AddDocument(1, "пушистый кот пушистый хвост"s);
        server.AddDocument(2, "ухоженный кот выразительные глаза"s);
        server.AddDocument(3, "милый кот пухлые щеки"s);
        server.AddDocument(4, "страшный кот драная шерсть"s);

        server.RemoveDocument(1);
        server.RemoveDocument(3);
        server.RemoveDocument(4);

        vector<Document> result = server.FindTopDocuments ("кот");
        vector<Document> expected_result = {Document{0, 0.0, 0}, Document{2, 0.0, 0}};
        ASSERT_EQUAL_HINT(result, expected_result, "Error removing document"s);
    }
    // проверка последовательной версии версии
    {
        SearchServer server(""s);
        server.AddDocument(0, "белый кот и модный ошейник"s);
        server.AddDocument(1, "пушистый кот пушистый хвост"s);
        server.AddDocument(2, "ухоженный кот выразительные глаза"s);
        server.AddDocument(3, "милый кот пухлые щеки"s);
        server.AddDocument(4, "страшный кот драная шерсть"s);

        server.RemoveDocument(std::execution::seq, 1);
        server.RemoveDocument(std::execution::seq, 3);
        server.RemoveDocument(std::execution::seq, 4);

        vector<Document> result = server.FindTopDocuments ("кот");
        vector<Document> expected_result = {Document{0, 0.0, 0}, Document{2, 0.0, 0}};
        ASSERT_EQUAL_HINT(result, expected_result, "Error removing document (seq version)"s);
    }
    // проверка параллельной версии
    {
        SearchServer server(""s);
        server.AddDocument(0, "белый кот и модный ошейник"s);
        server.AddDocument(1, "пушистый кот пушистый хвост"s);
        server.AddDocument(2, "ухоженный кот выразительные глаза"s);
        server.AddDocument(3, "милый кот пухлые щеки"s);
        server.AddDocument(4, "страшный кот драная шерсть"s);

        server.RemoveDocument(std::execution::par, 1);
        server.RemoveDocument(std::execution::par, 3);
        server.RemoveDocument(std::execution::par, 4);

        vector<Document> result = server.FindTopDocuments ("кот");
        vector<Document> expected_result = {Document{0, 0.0, 0}, Document{2, 0.0, 0}};
        ASSERT_EQUAL_HINT(result, expected_result, "Error removing document (par version)"s);
    }
}

// Тест проверяет, что не проходят некорректно сформированные поисковые запросы
void TestExcludeIncorrectFindDocuments() {
    SearchServer server(""s);
    server.AddDocument(0 ,"cat in the city"s);
    try {
        server.FindTopDocuments("--cat"s);
        ASSERT_HINT(false, "Server can't search for documents with two minus before words"s);
    }  catch (...) {}
    try {
        server.FindTopDocuments("cat - city"s);
        ASSERT_HINT(false, "Server can't search for documents with minus without words"s);
    }  catch (...) {}
    try {
        server.FindTopDocuments("большой пёс скво\x12рец"s);
        ASSERT_HINT(false, "Server can't search for documents with service symbols"s);
    }  catch (...) {}
    // так же проверим что корректный запрос пройдет
    try {
        server.FindTopDocuments("cat the"s);
    }  catch (...) {
        ASSERT_HINT(false, "Problems with searching correct documents"s);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs.at(0).id, doc_id);
    }

    {
        SearchServer server("    in     the     "s);
        server.AddDocument(doc_id, content);
        auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.empty(), "Stop words must be excluded from documents"s);
    }

    {
        vector<string> stop_words = {"in"s, ""s, "the"s, "the"s};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content);
        auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет что поисковая система исключает из выдачи документы с минус-словами (и не исключает другие)
void TestExcludeDocumentsWithMinusWords () {
    const int doc1_id = 32;
    const int doc2_id = 64;
    const string content1 = "cat in the city"s;
    const string content2 = "cat in the village"s;

    SearchServer server(""s);
    server.AddDocument (doc1_id, content1);
    server.AddDocument (doc2_id, content2);

    auto found_docs = server.FindTopDocuments("cat -village"s);
    ASSERT_HINT(found_docs.size() == 1u, "Documents with minus words must be excluded from response"s);
    ASSERT_EQUAL_HINT(found_docs.at(0).id, doc1_id, "Document without minus word was excluded from response"s);
}

// Проверка матчинга документов. При матчинге документа по поисковому запросу должны быть возвращены все
// слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову,
// должен возвращаться пустой список слов.
void TestDocumentsMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;

    // Проверяем что возвращаются все слова из поискового запроса
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            auto response = server.MatchDocument("cat the"s, doc_id);
            tuple<vector<string_view>, DocumentStatus> expected_response({"cat"sv, "the"sv}, {});
            ASSERT_EQUAL(get<0>(response), get<0>(expected_response));
        }  catch (...) {
            ASSERT_HINT (false, "Problems with matching correct document"s);
        }
    }

    // Затем проверяем что при наличии минус-слова возвращается пустой список
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            auto response = server.MatchDocument("cat the -city"s, doc_id);
            tuple<vector<string_view>, DocumentStatus> expected_response({}, {});
            ASSERT_EQUAL_HINT(get<0>(response), get<0>(expected_response), "Empty list should be returned if document contain minus word"s);
        }  catch (...) {
            ASSERT_HINT (false, "Problems with matching correct document"s);
        }
    }

    // Проверяем, что отслеживаются двойные минусы перед словами и минусы без слов, а так же спецсимволы
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            server.MatchDocument("--cat"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with two minus before words"s);
        }  catch (...) {}
        try {
            server.MatchDocument("-"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with minus without words"s);
        }  catch (...) {}
        try {
            server.MatchDocument("большой пёс скво\x12рец"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with service symbols"s);
        }  catch (...) {}
    }
    // проверяем поиск по несуществующему документу
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            server.MatchDocument("cat"s, 10);
            ASSERT_HINT(false, "Can't matching documents with incorrect id"s);
        }  catch (...) {}
    }
}

void TestDocumentsMatching_PAR() {
    const int doc_id = 42;
    const string content = "cat in the city"s;

    // Проверяем что возвращаются все слова из поискового запроса
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            auto response = server.MatchDocument(execution::par, "cat the"s, doc_id);
            tuple<vector<string_view>, DocumentStatus> expected_response({"cat"sv, "the"sv}, {});
            ASSERT_EQUAL(get<0>(response), get<0>(expected_response));
        }  catch (...) {
            ASSERT_HINT (false, "Problems with matching correct document"s);
        }
    }

    // Затем проверяем что при наличии минус-слова возвращается пустой список
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            auto response = server.MatchDocument(execution::par, "cat the -city"s, doc_id);
            tuple<vector<string_view>, DocumentStatus> expected_response({}, {});
            ASSERT_EQUAL_HINT(get<0>(response), get<0>(expected_response), "Empty list should be returned if document contain minus word"s);
        }  catch (...) {
            ASSERT_HINT (false, "Problems with matching correct document"s);
        }
    }

    // Проверяем, что отслеживаются двойные минусы перед словами и минусы без слов, а так же спецсимволы
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            server.MatchDocument(execution::par, "--cat"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with two minus before words"s);
        }  catch (...) {}
        try {
            server.MatchDocument(execution::par, "-"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with minus without words"s);
        }  catch (...) {}
        try {
            server.MatchDocument(execution::par, "большой пёс скво\x12рец"s, doc_id);
            ASSERT_HINT(false, "Server can't match document with service symbols"s);
        }  catch (...) {}
    }
    // проверяем поиск по несуществующему документу
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content);
        try {
            server.MatchDocument(execution::par, "cat"s, 10);
            ASSERT_HINT(false, "Can't matching documents with incorrect id"s);
        }  catch (...) {}
    }
}


// Проверка сортировки возвращаемых документов. Возвращаемые при поиске документов результаты должны быть
// отсортированы в порядке убывания релевантности.
void TestFindedDocumentsSort() {
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

    auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL_HINT(found_docs.size(), 3u, "Not all documents are presented as requested"s);
    ASSERT_HINT(found_docs.at(0).id == 1 && found_docs.at(1).id == 2 && found_docs.at(2).id == 0,
                "The order of output of documents does not correspond to the descending relevance"s);

    auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"s);
    ASSERT_EQUAL_HINT(found_docs_par.size(), 3u, "Not all documents are presented as requested"s);
    ASSERT_HINT(found_docs_par.at(0).id == 1 && found_docs_par.at(1).id == 2 && found_docs_par.at(2).id == 0,
                "The order of output of documents does not correspond to the descending relevance"s);
}

// Проверка вычисления рейтинга документа. Рейтинг добавленного документа равен среднему
// арифметическому оценок документа.
void TestComputeAverageRating () {
    SearchServer server("и в на"sv);
    vector<int> ratings = {-1, 1, 3, 5};
    // вычисляем для проверки средний рейтинг
    int average_rating = accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, ratings);

    auto found_docs = server.FindTopDocuments("кот"sv);
    ASSERT_EQUAL_HINT(found_docs.at(0).rating, average_rating, "Average Rating compute incorrect"s);
}

// Проверка фильтрации результатов поиска с использованием предиката, задаваемого пользователем.
void TestFindedDocumentsPredicate () {
    SearchServer server("и в на"s);
    vector<int> ratings1 = {-1, 1, 3, 5};
    vector<int> ratings2 = {5, 5, 5};
    // вычисляем для проверки средний рейтинг для второго документа
    int average_rating2 = accumulate(ratings2.begin(), ratings2.end(), 0) / static_cast<int>(ratings2.size());

    server.AddDocument(0, "белый кот и модный ошейник"sv,  DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, ratings2);

    //проверка на фильтрацию по id
    {
        auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s,
                                                  [](int document_id, DocumentStatus, int) {
            return document_id % 2 == 0;
        });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Incorrect documents filtering by IDs with predicate"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, 0, "Incorrect documents filtering by IDs with predicate"s);

        auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"sv,
                                                  [](int document_id, DocumentStatus, int) {
            return document_id % 2 == 0;
        });
        ASSERT_EQUAL_HINT(found_docs_par.size(), 1u, "Incorrect documents filtering by IDs with predicate"s);
        ASSERT_EQUAL_HINT(found_docs_par.at(0).id, 0, "Incorrect documents filtering by IDs with predicate"s);
    }

    //проверка на фильтрацию по статусу
    {
        auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"sv,
                                                  [](int, DocumentStatus status, int) {
            return status == DocumentStatus::BANNED;
        });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Incorrect documents filtering by STATUS with predicate"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, 1, "Incorrect documents filtering by STATUS with predicate"s);

        auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"s,
                                                  [](int, DocumentStatus status, int) {
            return status == DocumentStatus::BANNED;
        });
        ASSERT_EQUAL_HINT(found_docs_par.size(), 1u, "Incorrect documents filtering by STATUS with predicate"s);
        ASSERT_EQUAL_HINT(found_docs_par.at(0).id, 1, "Incorrect documents filtering by STATUS with predicate"s);
    }

    //проверка на фильтрацию по рейтингу
    {
        auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s,
                                                  [average_rating2](int, DocumentStatus, int rating) {
            return rating == average_rating2;
        });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Incorrect documents filtering by RATINGS with predicate"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, 1, "Incorrect documents filtering by RATINGS with predicate"s);

        auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"s,
                                                  [average_rating2](int, DocumentStatus, int rating) {
            return rating == average_rating2;
        });
        ASSERT_EQUAL_HINT(found_docs_par.size(), 1u, "Incorrect documents filtering by RATINGS with predicate"s);
        ASSERT_EQUAL_HINT(found_docs_par.at(0).id, 1, "Incorrect documents filtering by RATINGS with predicate"s);
    }

}


//проверка на минус-слова
void TestFindedDocumentsMinus() {

	SearchServer server("и в на"s);

	server.AddDocument(0, "найден белый кот . на нём модный ошейник"s);
	server.AddDocument(1, "пушистый кот ищет хозяина . особые приметы : пушистый хвост"s);
	server.AddDocument(2, "в парке горького найден ухоженный пёс с выразительными глазами"sv);

	{
		auto found_docs = server.FindTopDocuments("пушистый ухоженный кот -ошейник"sv);

		ASSERT_EQUAL_HINT(found_docs.size(), 2u, "Incorrect documents filtering by minus-word"s);
		ASSERT_EQUAL_HINT(found_docs.at(0).id, 1, "Incorrect documents filtering by minus-word"s);
		ASSERT_EQUAL_HINT(found_docs.at(1).id, 2, "Incorrect documents filtering by minus-word"s);
	}
	{
		auto found_docs = server.FindTopDocuments("пушистый ухоженный кот -кот -пёс"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 0u, "Incorrect documents filtering by minus-word"s);
	}
};


//Поиск документов, имеющих заданный статус.
void TestFindedDocumentsStatus() {
    SearchServer server("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s,  DocumentStatus::ACTUAL);
    server.AddDocument(1, "пушистый кот пушистый хвост"sv, DocumentStatus::BANNED);

    //проверка на фильтрацию по статусу
    {
        auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Incorrect documents filtering by STATUS"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, 1, "Incorrect documents filtering by STATUS"s);

        auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"s, DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Incorrect documents filtering by STATUS"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, 1, "Incorrect documents filtering by STATUS"s);
    }

    //проверка на фильтрацию документов с несуществующим статусом
    {
        auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 0u, "Incorrect documents filtering by STATUS"s);

        auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый ухоженный кот"sv, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 0u, "Incorrect documents filtering by STATUS"s);
    }
}

//Корректное вычисление релевантности найденных документов.
void TestFindedDocumentsRelevance () {
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"sv, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "ухоженный скворец евгений"sv,         DocumentStatus::ACTUAL, {1});

    // создаем запрос с повторяющимися словами, для проверки, что они не учитываются дважды
    auto found_docs = server.FindTopDocuments("пушистый пушистый ухоженный кот"s);
    auto found_docs_par = server.FindTopDocuments(std::execution::par, "пушистый пушистый ухоженный кот"sv);
    // общее кол-во документов делим на кол-во в которых встречается каждое слово из запроса
    vector<double> idf = {log(4.0 / 1.0), log(4.0 / 2.0), log(4.0 / 2.0)};
    // кол-во вхождений каждого слова из запроса в каждый документ делим на кол-во слов в этом документе
    vector<vector<double>> tf = {{        0,       0, 1.0 / 4.0},
                                 {2.0 / 4.0,       0, 1.0 / 4.0},
                                 {        0, 1.0/4.0,         0},
                                 {        0, 1.0/3.0,         0}};
    // расчет idf_tf для каждого документа
    vector<double> idf_tf = {tf[0][0]*idf[0] + tf[0][1]*idf[1] + tf[0][2]*idf[2],
                             tf[1][0]*idf[0] + tf[1][1]*idf[1] + tf[1][2]*idf[2],
                             tf[2][0]*idf[0] + tf[2][1]*idf[1] + tf[2][2]*idf[2],
                             tf[3][0]*idf[0] + tf[3][1]*idf[1] + tf[3][2]*idf[2]};

    vector<Document> expected_response({Document{1, idf_tf[1], 1},
                                        Document{3, idf_tf[3], 1},
                                        Document{0, idf_tf[0], 1},
                                        Document{2, idf_tf[2], 1}});

    ASSERT_EQUAL_HINT(found_docs.size(), 4u, "Not all documents are presented as requested"s);
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_EQUAL_HINT (found_docs.at(i), expected_response.at(i), "Relevance compute incorrect"s);
    }

    ASSERT_EQUAL_HINT(found_docs_par.size(), 4u, "Not all documents are presented as requested"s);
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_EQUAL_HINT (found_docs_par.at(i), expected_response.at(i), "Relevance compute incorrect"s);
    }
}

// Проверка возврата частоты слов по id документа
void TestGetWordFrequencies() {
    SearchServer server("и в на"s);
    server.AddDocument(42, "пушистый кот пушистый хвост"s);
    // проверяем, что выдается пустой результат для несуществующего id
    {
        map<string_view, double> result = server.GetWordFrequencies(12);
        ASSERT_EQUAL_HINT(result.size(), 0u, "Should be empty result with nonexistent id"s);
    }
    // проверяем, что выдается нужный результат
    {
        map<string_view, double> result = server.GetWordFrequencies(42);
        map<string_view, double> expected_result = {{"пушистый", 2.0 / 4.0},
                                               {"кот", 1.0 / 4.0},
                                               {"хвост", 1.0 / 4.0}};
        ASSERT_EQUAL_HINT(result, expected_result, "Incorrect result"s);
    }
}

// Проверка удаления дубликатов
void TestRemoveDuplicates() {
 // добавить тесты !!!
}

// Проверка ProcessQueriesJoined
void TestProcessQueriesJoined() {

    SearchServer search_server("and with"sv);
    int id = 0;
    for (
        const string_view text : {
            "funny pet and nasty rat"sv,
            "funny pet with curly hair"sv,
            "funny pet and not very nasty rat"sv,
            "pet with rat and rat and rat"sv,
            "nasty rat with curly hair"sv,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }
    const vector<string_view> queries = {
        "nasty rat -not"sv,
        "not very funny nasty pet"sv,
        "curly hair"sv
    };

    vector<Document> expected_response({ 
        Document {1, 0.183492, 1},
        Document {5, 0.183492, 1},
        Document {4, 0.167358, 1},
        Document {3, 0.743945, 1},
        Document {1, 0.311199, 1},
        Document {2, 0.183492, 1},
        Document {5, 0.127706, 1},
        Document {4, 0.0557859, 1},
        Document {2, 0.458145, 1},
        Document {5, 0.458145, 1}
        });

    auto found_docs = ProcessQueriesJoined(search_server, queries);

    ASSERT_EQUAL_HINT(found_docs.size(), 10u, "Not all documents are presented as requested"s);
    size_t i = 0;
    for_each(found_docs.begin(), found_docs.end(), [&i, &expected_response](auto& doc) {
        ASSERT_EQUAL_HINT(doc, expected_response.at(i), "Relevance compute incorrect"s); ++i;
    });
}



//one_document_excluded
//Вы неправильно обрабатываете случай, когда один документ исключён из запроса

void one_document_excluded() {
    const std::vector<int> ratings1 = { 1 };
    const std::vector<int> ratings2 = { 1 };
    const std::vector<int> ratings3 = { 1 };

    std::string stop_words = "белый кот и модный ошейник";
    SearchServer search_server(stop_words);

    search_server.AddDocument(0, "белый кот и модный ошейник", DocumentStatus::ACTUAL, ratings1);
    search_server.AddDocument(1, "пушистый кот пушистый хвост", DocumentStatus::ACTUAL, ratings2);
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, ratings3);

    const SearchServer const_search_server = search_server;

    const std::string query = "пушистый и ухоженный кот";
    const auto documents = const_search_server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    for (const Document& document : documents) {
        std::cout << "{ "
            << "document_id = " << document.id << ", "
            << "relevance = " << document.relevance << ", "
            << "rating = " << document.rating << " }" << std::endl;
    }
}
//all_documents_excluded
//Вы неправильно обрабатываете случай, когда все документы исключены из запроса

//process_queries
//Неправильно работает ProcessQueries

//process_queries_joined
//Неправильно работает ProcessQueriesJoined


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    //RUN_TEST(one_document_excluded);
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestProcessQueriesJoined);
    RUN_TEST(TestInitServer);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestExcludeIncorrectFindDocuments);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestDocumentsMatching);
    RUN_TEST(TestDocumentsMatching_PAR);
    RUN_TEST(TestFindedDocumentsSort);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestFindedDocumentsPredicate);
    RUN_TEST(TestFindedDocumentsStatus);
    RUN_TEST(TestFindedDocumentsMinus);
    RUN_TEST(TestFindedDocumentsRelevance);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDuplicates);
}
// --------- Окончание модульных тестов поисковой системы -----------





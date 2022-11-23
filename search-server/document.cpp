#include "document.h"

using namespace std::literals;

Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
}


void PrintDocument(const Document& document, std::ostream& out) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << std::endl;
}
void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status, std::ostream& out) {
    out << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const std::string& word : words) {
        out << ' ' << word;
    }
    out << "}"s << std::endl;
}

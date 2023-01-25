#include "document.h"

using namespace std::literals;

Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
}

std::ostream& operator<<(std::ostream& out, const Document& rhs) {
    out << "{ "s
        << "document_id = "s << rhs.id << ", "s
        << "relevance = "s << rhs.relevance << ", "s
        << "rating = "s << rhs.rating << " }"s << std::endl;
    return out;
}

bool operator==(const Document & lhs, const Document & rhs){
    return lhs.id == rhs.id && (abs(lhs.relevance - rhs.relevance) < MAX_RELEVANCE_INACCURACY) && lhs.rating == rhs.rating;
}

bool operator!=(const Document& lhs, const Document& rhs) {
    return !(lhs == rhs);
}

bool operator<(const Document& lhs, const Document& rhs) {
    //сравниваем документы по убыванию релевантности и рейтинга
    if (abs(lhs.relevance - rhs.relevance) < MAX_RELEVANCE_INACCURACY) {
        return lhs.rating < rhs.rating;
    }
    return lhs.relevance < rhs.relevance;
}

void PrintDocument(const Document& document, std::ostream& out) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << std::endl;
}
void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status, std::ostream& out) {
    out << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const std::string_view word : words) {
        out << ' ' << word;
    }
    out << "}"s << std::endl;
}

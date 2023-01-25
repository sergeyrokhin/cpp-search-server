#pragma once
#include <vector>
#include <string>
#include <iostream>

const double MAX_RELEVANCE_INACCURACY = 1e-6;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating);
    friend bool operator==(const Document& lhs, const Document& rhs);
    friend bool operator!=(const Document& lhs, const Document& rhs);
    friend bool operator<(const Document& lhs, const Document& rhs);
    friend bool operator>(const Document& lhs, const Document& rhs) {
        return rhs < lhs;
    }
    friend std::ostream& operator<<(std::ostream& os, const Document& rhs);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

void PrintDocument(const Document& document, std::ostream& out = std::cerr);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status, std::ostream& out = std::cerr);

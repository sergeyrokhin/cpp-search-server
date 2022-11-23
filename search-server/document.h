#pragma once
#include <vector>
#include <string>
#include <iostream>
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

void PrintDocument(const Document& document, std::ostream& out = std::cerr);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status, std::ostream& out = std::cerr);

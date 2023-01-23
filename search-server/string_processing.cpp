#include <stdexcept>
#include <string>
#include <algorithm>
#include <execution>
#include "string_processing.h"

using namespace std;



std::vector<std::string_view> SplitIntoWordsView(const std::string_view& text) {

    if (any_of(text.begin(), text.end(), [](auto& c) {
        return c < ' ' && c >= '\0';
        })) throw std::invalid_argument("Word "s + string(text) + " is invalid"s);


        std::vector<std::string_view> words;
        size_t start = text.find_first_not_of(' ');
        while (start != string_view::npos) {
            size_t end = text.find(' ', start);
            words.push_back(text.substr(start, end - start));
            start = text.find_first_not_of(' ', end);
        }
        return words;
}
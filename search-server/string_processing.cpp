#include <stdexcept>
#include <string>
#include <algorithm>
#include <execution>
#include "string_processing.h"

using namespace std;
//принимает и константные, проверка на наличие спецсимволов
std::vector<std::string_view> SplitIntoWordsView(const string_view text) {

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

//авторская. константные не принимает
vector<string_view> Split(string_view str) {
    vector<string_view> result;
    while (true) {
        const auto space = str.find(' ');
        if (space != 0 && !str.empty()) {
            result.push_back(str.substr(0, space));
        }
        if (space == str.npos) {
            break;
        }
        else {
            str.remove_prefix(space + 1);
        }
    }
    return result;
}
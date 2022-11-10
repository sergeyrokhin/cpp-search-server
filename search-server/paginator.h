#pragma once
#include <cassert>
#include <iostream>
#include <vector>
#include <iostream>


using namespace std;

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange() = delete;
    IteratorRange(const Iterator& begin, const Iterator& end, size_t size)
        : size_(size), begin_(begin), end_(end) { }

    auto begin() const { return begin_; }
    auto end() const { return end_; }

private:
    size_t size_;
    Iterator begin_;
    Iterator end_;
};


template <typename Iterator>
class Paginator
{
public:
    Paginator() = delete;
    Paginator(const Iterator begin, const Iterator end, size_t page_size)
        : page_size_(page_size)
    {
        assert(end >= begin && page_size == 0);
        Iterator it2 = begin;
        while (it2 != end)
        {
            Iterator it1 = it2;
            size_t i = 0;
            for (i = 0; i < page_size_; i++)
            {
                if (++it2 == end)
                {
                    break;
                }
            }
            pages_.emplace_back(IteratorRange<Iterator>(it1, it2, i));
        }
    }
    int size() const { return pages_.size(); }
    auto end() const { return pages_.end(); }
    auto begin() const { return pages_.begin(); }
private:
    size_t page_size_;
    vector<IteratorRange<Iterator>> pages_;
};


template <typename T, typename Iterator>
T& operator<<(T& out, const IteratorRange<Iterator>& page) {
    for (auto doc = page.begin(); doc != page.end(); doc++)
    {
        out << "{ document_id = "s << (*doc).id << ", relevance = " << (*doc).relevance << ", rating = " << (*doc).rating << " }"s;
    }
    return out;
}

template <typename Container>
auto Paginate(Container& c, size_t page_size)
{
    return Paginator(begin(c), end(c), page_size);
}

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	search_server.RemoveDuplicates();
}

void SearchServer::RemoveDuplicates() {
	for (auto& ind_docs : index_documents)
	{
		if (ind_docs.second.size() > 1)
		{
			RemoveDuplicates(ind_docs.second);
		}
	}
}

void SearchServer::RemoveDuplicates(set<int>& dup) {
	for (auto i = dup.begin(); i != dup.end(); i++)
	{
		vector<int> for_del;
		auto j = i;
		j++;
		for (; j != dup.end(); j++)
		{
			set<string> uniq_words1, uniq_words2;

			for (const std::string& word : documents_words[*i]) {
				uniq_words1.insert(word);
			}
			for (const std::string& word : documents_words[*j]) {
				uniq_words2.insert(word);
			}


			if (uniq_words1 == uniq_words2)
			{
				for_del.emplace_back(*j);
			}
		}
		for (auto k : for_del)
		{
			std::cout << "Found duplicate document id " << k << std::endl;
			RemoveDocument(k);
		}
	}
}

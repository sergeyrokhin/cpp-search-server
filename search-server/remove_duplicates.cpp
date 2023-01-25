#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	search_server.RemoveDuplicates();
}

void SearchServer::RemoveDuplicates() {
	map<int, bool> select_documents; //Список для удаления
	map<int, set<string>> dictionary_documents; //для документа его уникальные слова
	set<string> doc_words;
	for (auto& [id, words] : doc_id_word_freq_)
	{
		select_documents.insert(make_pair( id , false ));
		doc_words.clear();
		for(auto& word : words)
		{
			doc_words.emplace(word.first);
		}
		dictionary_documents.insert({id, doc_words });

	}
	for (auto it_sel = select_documents.begin(); it_sel != select_documents.end(); it_sel++) {
		for (auto it_for_del = select_documents.begin(); it_for_del != select_documents.end(); it_for_del++) {
			if (!(*it_sel).second) //если пока не помечен для удаления
			if ((*it_sel).first != (*it_for_del).first) // сам себя не удаляю
			{
				if (! (*it_for_del).second) //если пока не помечен для удаления
				if (dictionary_documents[(*it_sel).first] == dictionary_documents[(*it_for_del).first])
				{
					(*it_for_del).second = true;
				}
			}
		}
	}
	for (auto& [id, for_del] : select_documents)
	{
		if (for_del)
		{
			std::cout << "Found duplicate document id " << id << std::endl;
			RemoveDocument(id);
		}
	}

}

#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <utility>
#include <stdexcept>
#include <execution>

#include "read_input_functions.h"
#include "process_queries.h"
#include "log_duration.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include "document.h"

extern int MAX_RESULT_DOCUMENT_COUNT;
extern double EPSILON;

class SearchServer
{
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    //void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    //    template <typename DocumentPredicate>
    //    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
    //    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
    //    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename Ex_Pol, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(Ex_Pol ep, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename Ex_Pol>
    std::vector<Document> FindTopDocuments(Ex_Pol ep, std::string_view raw_query, DocumentStatus status) const;
    template <typename Ex_Pol>
    std::vector<Document> FindTopDocuments(Ex_Pol ep, std::string_view raw_query) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;

    void RemoveDocument(int document_id);
    template <typename Ex_Pol>
    void RemoveDocument(Ex_Pol ep, int document_id);
private:

    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<int> document_ids_;
    std::map<int, DocumentData> documents_;

    bool IsStopWord(std::string_view word) const;
    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;
    Query ParseQuery(std::string_view text, bool skip_sort=false) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename Ex_Pol, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(Ex_Pol ep, const Query& query, DocumentPredicate document_predicate) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))
    {
        throw std::invalid_argument(std::string("Some of stop words are invalid"));
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const
{
    return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const
{
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename Ex_Pol, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(Ex_Pol ep, std::string_view raw_query, DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(ep, query, document_predicate);
    sort(ep, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs)
         {
             if (std::abs(lhs.relevance - rhs.relevance) < EPSILON)
             {
                 return lhs.rating > rhs.rating;
             }
             else
             {
                 return lhs.relevance > rhs.relevance;
             }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Ex_Pol>
std::vector<Document> SearchServer::FindTopDocuments(Ex_Pol ep, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(ep, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        {
            return document_status == status;
        });
}

template <typename Ex_Pol>
std::vector<Document> SearchServer::FindTopDocuments(Ex_Pol ep, std::string_view raw_query) const
{
    return FindTopDocuments(ep, raw_query, DocumentStatus::ACTUAL);
}

template <typename Ex_Pol, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(Ex_Pol ep, const Query& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(document_ids_.size());
    std::for_each(ep, query.plus_words.begin(), query.plus_words.end(), [&](std::string_view word)
        {
            if (word_to_document_freqs_.count(word) != 0)
            {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto& [document_id, term_freq] : (*word_to_document_freqs_.find(word)).second)
                {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating) && 
                        std::all_of(query.minus_words.begin(), query.minus_words.end(), [&](std::string_view minus_word)
                            {
                                return (document_to_word_freqs_.at(document_id).count(minus_word) == 0);
                            }))
                    {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });
    
    std::vector<Document> matched_documents;
    const auto doc_to_rel = document_to_relevance.BuildOrdinaryMap();
    for (const auto& [document_id, relevance] : doc_to_rel)
    {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename Ex_Pol>
void SearchServer::RemoveDocument(Ex_Pol ep, int document_id)
{
    if (document_ids_.find(document_id) == document_ids_.end())
    {
        return;
    }
    std::vector<std::string_view> words;
    words.reserve(document_to_word_freqs_[document_id].size());
    for (auto& [word, _] : document_to_word_freqs_[document_id])
    {
        words.push_back(std::move(word));
    }
    std::for_each(ep, words.begin(), words.end(), [this, document_id](const std::string& word)
        {
            this->word_to_document_freqs_[word].erase(document_id);
        });
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}



void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);
void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);

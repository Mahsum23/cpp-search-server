#pragma once

#include <deque>
#include "search_server.h"
#include "paginator.h"


class RequestQueue 
{
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename Ex_Pol, typename DocumentPredicate>
    std::vector<Document> AddFindRequest(Ex_Pol ep, std::string_view raw_query, DocumentPredicate document_predicate);
    
    template <typename Ex_Pol>
    std::vector<Document> AddFindRequest(Ex_Pol ep, std::string_view raw_query, DocumentStatus status);

    template <typename Ex_Pol>
    std::vector<Document> AddFindRequest(Ex_Pol ep, std::string_view raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult 
    {
        bool is_empty;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    size_t number_of_no_result_requests_ = 0;
};

template <typename Ex_Pol>
std::vector<Document> RequestQueue::AddFindRequest(Ex_Pol ep, std::string_view raw_query, DocumentStatus status)
{
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        {
            return document_status == status;
        });
}

template <typename Ex_Pol>
std::vector<Document> RequestQueue::AddFindRequest(Ex_Pol ep, std::string_view raw_query)
{
    return AddFindRequest(raw_query, [](int document_id, DocumentStatus document_status, int rating)
        {
            return document_status == DocumentStatus::ACTUAL;
        });
}

template <typename Ex_Pol, typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(Ex_Pol ep, std::string_view raw_query, DocumentPredicate document_predicate)
{
    std::vector<Document> res = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() >= min_in_day_)
    {
        if (requests_.front().is_empty && !res.empty())
        {
            --number_of_no_result_requests_;
        }
        requests_.pop_front();
    }
    number_of_no_result_requests_ += res.empty();
    requests_.push_back({ res.empty() });
    return res;
}

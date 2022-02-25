#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
{

}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) 
{
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating)
                          {
                              return document_status == status;
                          });
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) 
{
    return AddFindRequest(raw_query, [](int document_id, DocumentStatus document_status, int rating)
                          {
                              return document_status == DocumentStatus::ACTUAL;
                          });    
}

int RequestQueue::GetNoResultRequests() const 
{
    return static_cast<int>(number_of_no_result_requests_);
}

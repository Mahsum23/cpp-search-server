#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
{

}

int RequestQueue::GetNoResultRequests() const
{
    return static_cast<int>(number_of_no_result_requests_);
}

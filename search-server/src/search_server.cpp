#include "search_server.h"

using namespace std;

int MAX_RESULT_DOCUMENT_COUNT = 5;
double EPSILON = 1e-6;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{

}

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWords(static_cast<string>(stop_words_text)))
{

}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings)
{
    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (string_view word : words)
    {
        word_to_document_freqs_[static_cast<string>(word)][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const
{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
    return static_cast<int>(documents_.size());
}

std::set<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static map<string_view, double> empty_map = {};
    if (document_to_word_freqs_.find(document_id) == document_to_word_freqs_.end())
    {
        return empty_map;
    }
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    if (document_ids_.find(document_id) == document_ids_.end())
    {
        return;
    }
    for (const auto& [word, _] : document_to_word_freqs_[document_id])
    {
        (*word_to_document_freqs_.find(word)).second.erase(document_id);
        if ((*word_to_document_freqs_.find(word)).second.empty())
        {
            word_to_document_freqs_.erase(word_to_document_freqs_.find(word));
        }
    }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const
{
    const Query query = ParseQuery(raw_query);
    auto status = documents_.at(document_id).status;
    vector<string_view> matched_words;
    for (string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if ((*word_to_document_freqs_.find(word)).second.count(document_id))
        {
            return { {}, status};
        }
    }
    for (string_view word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        
        if ((*word_to_document_freqs_.find(word)).second.count(document_id))
        {
            matched_words.push_back(word);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy,
    std::string_view raw_query, int document_id) const
{

    using namespace std;

    Query query = ParseQuery(raw_query, true);
    auto status = documents_.at(document_id).status;
    

    auto words_checker = [this, document_id](string_view word)
    {
        auto it = word_to_document_freqs_.find(word);
        return (it != word_to_document_freqs_.end()) && it->second.count(document_id);
    };
    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), words_checker))
    {
        return { {}, status };
    }
    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), words_checker);
    sort(execution::par, matched_words.begin(), words_end);
    matched_words.erase(unique(matched_words.begin(), words_end), matched_words.end());
    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy,
    std::string_view raw_query, int document_id) const
{
    return MatchDocument(raw_query, document_id);
}


bool SearchServer::IsStopWord(string_view word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word)
{
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c)
        {
            return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const
{
    vector<string_view> words;
    for (string_view word : SplitIntoWordsView(text))
    {
        if (!IsValidWord(word))
        {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings)
{
    if (ratings.empty())
    {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view text) const
{
    if (text.empty())
    {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        throw invalid_argument("Query word "s + string(text) + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const string_view text, bool skip_sort) const
{
    Query result;
    for (string_view word : SplitIntoWordsView(text))
    {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.push_back(query_word.data);
            }
            else
            {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (!skip_sort)
    {
        sort(result.minus_words.begin(), result.minus_words.end());
        auto it = unique(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(it, result.minus_words.end());
        sort(result.plus_words.begin(), result.plus_words.end());
        it = unique(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(it, result.plus_words.end());
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const
{
    return log(GetDocumentCount() * 1.0 / (*word_to_document_freqs_.find(word)).second.size());
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (string_view word : words)
    {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
    const vector<int>& ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const invalid_argument& e)
    {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string_view raw_query)
{
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& e)
    {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, string_view query)
{
    try
    {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (int doc_id : search_server)
        {
            const auto [words, status] = search_server.MatchDocument(query, doc_id);
            PrintMatchDocumentResult(doc_id, words, status);
        }
    }
    catch (const invalid_argument& e)
    {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

#include <iostream>
#include <string>

#include <curl/curl.h>
#include <gumbo.h>

#include "Wrapper.h"

using namespace std;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    auto fetcher = fetch_and_parse("https://www.example.com");
    if (fetcher) {
        cout << "HTTP Status: " << fetcher->doc->http_status << endl;
        cout << "HTML Content Length: " << fetcher->doc->content_length << endl;
        cout << "HTML Content: " << GetText(fetcher->parse_tree->root) << endl;
    } else {
        cerr << "Failed to fetch and parse the URL." << endl;
    }

    auto reset_result = reset_fetcher(fetcher, "https://github.com/hxomak?tab=packages");
    if (reset_result == 0) {
        cout << "Fetcher reset successfully." << endl;
        cout << "New HTTP Status: " << fetcher->doc->http_status << endl;
        cout << "New HTML Content Length: " << fetcher->doc->content_length << endl;
        auto taga = FindTag(fetcher->parse_tree->root, GUMBO_TAG_A, "", "");
        cout << "New HTML Content: " << fetcher->doc->html_content << endl;
    } else {
        cerr << "Failed to reset the fetcher." << endl;
    }

    curl_global_cleanup();
    return 0;
}

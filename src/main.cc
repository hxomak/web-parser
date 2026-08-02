#include <iostream>
#include <string>

#include "../third_party/include/gumbo.h"
#include "../third_party/include/curl/curl.h"

#include "../include/web_parser.h"

using namespace std;

using namespace wp;

int main() {
    CURL_INIT
    auto fet = fetch_and_parse("https://example.com/");
    if (fet) {
        cout << fet->doc->html_content << endl;
        reset_fetcher(fet, "https://example.org/");
        cout << fet->doc->html_content << endl;
    }
    CURL_CLEAN
    return 0;
}

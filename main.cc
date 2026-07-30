#include <iostream>
#include <string>

#include <curl/curl.h>
#include <gumbo.h>

#include "Wrapper.h"

using namespace std;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    HTMLFetcher *fetcher = fetch_and_parse("https://www.tek-stock.com/ut-020-/");

    if (!fetcher) {
        std::cerr << "Failed to fetch and parse the URL." << std::endl;
        curl_global_cleanup();
        return 1;
    }

    GumboNode *root = fetcher->parse_tree->root;

    auto tags = FindAllTagsAnysubval(root, GUMBO_TAG_A, "class", "productView-thumbnail-");

    cout << "Found " << tags.size() << " <a> tags with class 'productView-thumbnail-'" << endl;

    for (const auto &tag : tags) {
        auto href = gumbo_get_attribute(&tag->v.element.attributes, "href");
        if (href) {
            cout << "Found href: " << href->value << endl;
        }
    }

    free_fetcher(fetcher);
    curl_global_cleanup();
    return 0;
}

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <gumbo.h>

#include "../include/web_parser.h"

using namespace wp;

static const char *kTestHtml =
    "<html><head><style>.x{color:red}</style><script>var a=1;</script></head>"
    "<body>"
    "<div id='main' class='container box'>"
    "<p class='text'>Hello World</p>"
    "<a href='#' class='link special'>Click</a>"
    "<span class='hidden-text'>Nested</span>"
    "</div>"
    "<div class='excluded'><p>Skip me</p></div>"
    "</body></html>";

int main() {
    GumboOutput *output = gumbo_parse(kTestHtml);
    GumboNode *root = output->root;

    // get_text
    std::string txt = get_text(root);
    assert(txt.find("Hello World") != std::string::npos);
    printf("get_text: %s\n", txt.c_str());

    // get_text_excstylescript
    std::string txt2 = get_text_excstylescript(root);
    assert(txt2.find("var a=1") == std::string::npos);
    printf("get_text_excstylescript: %s\n", txt2.c_str());

    // get_html
    std::string html1 = get_html(root);
    assert(!html1.empty());
    printf("get_html length: %zu\n", html1.size());

    // get_html_excstylescript
    std::string html2 = get_html_excstylescript(root);
    assert(html2.find("<script>") == std::string::npos);
    printf("get_html_excstylescript length: %zu\n", html2.size());

    // get_html_excstylescript_less
    std::string html3 = get_html_excstylescript_less(root);
    printf("get_html_excstylescript_less length: %zu\n", html3.size());

    // find
    GumboNode *p = find(root, GUMBO_TAG_P, "class", "text");
    assert(p != nullptr);
    printf("find P: %s\n", get_text(p).c_str());

    // find_all
    std::vector<GumboNode *> divs = find_all(root, GUMBO_TAG_DIV, "", "");
    assert(divs.size() == 2);
    printf("find_all DIV count: %zu\n", divs.size());

    // find_anyv
    GumboNode *a1 = find_anyv(root, GUMBO_TAG_A, "class", "special");
    assert(a1 != nullptr);
    printf("find_anyv A: %s\n", get_text(a1).c_str());

    // find_anysubv
    GumboNode *span = find_anysubv(root, GUMBO_TAG_SPAN, "class", "hidden");
    assert(span != nullptr);
    printf("find_anysubv SPAN: %s\n", get_text(span).c_str());

    // find_all_anyv
    std::vector<GumboNode *> boxed = find_all_anyv(root, GUMBO_TAG_DIV, "class", "container");
    assert(boxed.size() == 1);
    printf("find_all_anyv count: %zu\n", boxed.size());

    // find_all_anysubv
    std::vector<GumboNode *> subv = find_all_anysubv(root, GUMBO_TAG_DIV, "class", "excl");
    assert(subv.size() == 1);
    printf("find_all_anysubv count: %zu\n", subv.size());

    // find_excclass
    GumboNode *exc = find_excclass(root, GUMBO_TAG_P, "class", "", "excluded");
    printf("find_excclass result: %p\n", (void *)exc);

    // find_exctag
    GumboNode *noscript = find_exctag(root, GUMBO_TAG_P, "", "", GUMBO_TAG_SCRIPT);
    assert(noscript != nullptr);
    printf("find_exctag P: %s\n", get_text(noscript).c_str());

    // extract
    GumboNode *extracted = extract(divs, GUMBO_TAG_DIV, "class", "excluded");
    assert(extracted != nullptr);
    printf("extract DIV class=excluded found\n");

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    // fetch_and_parse / reset_fetcher / free_fetcher
    HTMLFetcher *fetcher = fetch_and_parse("https://example.com");
    if (fetcher) {
        printf("fetch_and_parse status: %d\n", fetcher->doc->http_status);
        int rc = reset_fetcher(fetcher, "https://example.org");
        printf("reset_fetcher rc: %d, status: %d\n", rc, fetcher->doc->http_status);
        free_fetcher(fetcher);
    } else {
        printf("fetch_and_parse failed (no network?)\n");
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}

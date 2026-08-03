#ifndef WEB_PARSER_H
#define WEB_PARSER_H

#include <string>
#include <vector>
#include <sstream>
#include <cstring>

#include "../third_party/include/gumbo.h"
#include "../third_party/include/curl/curl.h"

#ifndef CURL_INIT
#define CURL_INIT curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

#ifndef CURL_CLEAN
#define CURL_CLEAN curl_global_cleanup();
#endif

namespace wp {

    template<typename T>
    std::vector<T> &operator+=(std::vector<T> &lv, const std::vector<T> &rv) {
        lv.insert(lv.end(), rv.begin(), rv.end());
        return lv;
    }

    inline std::vector<std::string> splitstr(const std::string &str) {
        if (str.empty())
            return {};
        std::vector<std::string> tokens;
        std::istringstream string_stream(str);
        std::string token;
        while (string_stream >> token)
            tokens.push_back(token);
        return tokens;
    }

    inline std::string removewhspaces(const std::string &str) {
        if (str.empty())
            return {};
        std::string res;
        res.reserve(str.length() / 4 * 3);
        bool started = false;
        bool space_pending = false;
        for (const char c: str) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (started)
                    space_pending = true;
            } else {
                if (space_pending) {
                    res += ' ';
                    space_pending = false;
                }
                res += c;
                started = true;
            }
        }
        return res;
    }

    inline std::string gettagstr(const GumboStringPiece &piece) {
        if (!piece.data || piece.length == 0)
            return "";
        return {piece.data, piece.length};
    }

    typedef struct {
        char *html_content;
        size_t content_length;
        char *url;
        int http_status;
    } FetchedDocument;

    typedef struct {
        CURL *curl_handle;
        GumboOutput *parse_tree;
        FetchedDocument *doc;
    } HTMLFetcher;

    inline void GetText(const GumboNode *tag, std::string &res) {
        if (!tag)
            return;
        if (tag->type == GUMBO_NODE_TEXT || tag->type == GUMBO_NODE_WHITESPACE) {
            res += std::string(tag->v.text.text);
            return;
        }
        if (tag->type != GUMBO_NODE_ELEMENT)
            return;
        const GumboVector *children = &tag->v.element.children;
        for (size_t i = 0; i < children->length; ++i) {
            res.push_back(' ');
            GetText(static_cast<GumboNode *>(children->data[i]), res);
        }
    }

    inline void GetTextExcStyleScript(const GumboNode *tag, std::string &res) {
        if (!tag)
            return;
        if (tag->type == GUMBO_NODE_TEXT || tag->type == GUMBO_NODE_WHITESPACE) {
            res += std::string(tag->v.text.text);
            return;
        }
        if (tag->type != GUMBO_NODE_ELEMENT)
            return;
        if (tag->v.element.tag == GUMBO_TAG_STYLE || tag->v.element.tag == GUMBO_TAG_SCRIPT)
            return;
        const GumboVector *children = &tag->v.element.children;
        for (size_t i = 0; i < children->length; ++i) {
            res.push_back(' ');
            GetTextExcStyleScript(static_cast<GumboNode *>(children->data[i]), res);
        }
    }

    inline std::string GetHtmlView(const GumboNode *node) {
        if (!node)
            return "";
        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE)
            return gettagstr(node->v.text.original_text);
        if (node->type != GUMBO_NODE_ELEMENT)
            return "";
        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";
        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i)
            result += GetHtmlView(static_cast<const GumboNode *>(children->data[i]));
        static const GumboTag void_tags[] = {
            GUMBO_TAG_AREA, GUMBO_TAG_BASE, GUMBO_TAG_BR, GUMBO_TAG_COL,
            GUMBO_TAG_EMBED, GUMBO_TAG_HR, GUMBO_TAG_IMG, GUMBO_TAG_INPUT,
            GUMBO_TAG_LINK, GUMBO_TAG_META, GUMBO_TAG_PARAM, GUMBO_TAG_SOURCE,
            GUMBO_TAG_TRACK, GUMBO_TAG_WBR
        };
        bool is_void = false;
        for (size_t j = 0; j < sizeof(void_tags) / sizeof(GumboTag); ++j) {
            if (node->v.element.tag == void_tags[j]) {
                is_void = true;
                break;
            }
        }
        if (!is_void)
            result += "</" + std::string(tag_name) + ">";
        return result;
    }

    inline std::string GetHtmlViewExcStyleScript(const GumboNode *node) {
        if (!node)
            return "";
        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE)
            return gettagstr(node->v.text.original_text);
        if (node->type != GUMBO_NODE_ELEMENT)
            return "";
        if (node->v.element.tag == GUMBO_TAG_STYLE || node->v.element.tag == GUMBO_TAG_SCRIPT)
            return "";
        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";
        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i)
            result += GetHtmlViewExcStyleScript(static_cast<const GumboNode *>(children->data[i]));
        static const GumboTag void_tags[] = {
            GUMBO_TAG_AREA, GUMBO_TAG_BASE, GUMBO_TAG_BR, GUMBO_TAG_COL,
            GUMBO_TAG_EMBED, GUMBO_TAG_HR, GUMBO_TAG_IMG, GUMBO_TAG_INPUT,
            GUMBO_TAG_LINK, GUMBO_TAG_META, GUMBO_TAG_PARAM, GUMBO_TAG_SOURCE,
            GUMBO_TAG_TRACK, GUMBO_TAG_WBR
        };
        bool is_void = false;
        for (size_t j = 0; j < sizeof(void_tags) / sizeof(GumboTag); ++j) {
            if (node->v.element.tag == void_tags[j]) {
                is_void = true;
                break;
            }
        }
        if (!is_void)
            result += "</" + std::string(tag_name) + ">";
        return result;
    }

    inline std::string GetHtmlViewExcStyleScriptLess(const GumboNode *node) {
        if (!node)
            return "";
        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE)
            return gettagstr(node->v.text.original_text);
        if (node->type != GUMBO_NODE_ELEMENT)
            return "";
        if (node->v.element.tag == GUMBO_TAG_STYLE || node->v.element.tag == GUMBO_TAG_SCRIPT)
            return "";
        static const GumboTag void_tags[] = {
            GUMBO_TAG_AREA, GUMBO_TAG_BASE, GUMBO_TAG_BR, GUMBO_TAG_COL,
            GUMBO_TAG_EMBED, GUMBO_TAG_HR, GUMBO_TAG_IMG, GUMBO_TAG_INPUT,
            GUMBO_TAG_LINK, GUMBO_TAG_META, GUMBO_TAG_PARAM, GUMBO_TAG_SOURCE,
            GUMBO_TAG_TRACK, GUMBO_TAG_WBR
        };
        for (auto void_tag: void_tags)
            if (node->v.element.tag == void_tag)
                return "";
        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";
        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i)
            result += GetHtmlViewExcStyleScriptLess(static_cast<const GumboNode *>(children->data[i]));
        result += "</" + std::string(tag_name) + ">";
        return result;
    }

    inline size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
        size_t realsize = size * nmemb;
        FetchedDocument *doc = (FetchedDocument *)userp;
        char *ptr = (char *)realloc((void *)doc->html_content, doc->content_length + realsize + 1);
        if (!ptr) {
            fprintf(stderr, "Not enough memory\n");
            return 0;
        }
        doc->html_content = ptr;
        memcpy(&(doc->html_content[doc->content_length]), contents, realsize);
        doc->content_length += realsize;
        doc->html_content[doc->content_length] = 0;
        return realsize;
    }

    inline HTMLFetcher *fetch_and_parse(const char *url) {
        HTMLFetcher *fetcher = (HTMLFetcher *)malloc(sizeof(HTMLFetcher));
        if (!fetcher)
            return nullptr;
        fetcher->doc = (FetchedDocument *)malloc(sizeof(FetchedDocument));
        if (!fetcher->doc) {
            free(fetcher);
            return nullptr;
        }
        fetcher->doc->html_content = (char *)malloc(1);
        if (!fetcher->doc->html_content) {
            free(fetcher->doc);
            free(fetcher);
            return nullptr;
        }
        fetcher->doc->content_length = 0;
        fetcher->doc->url = strdup(url);
        if (!fetcher->doc->url) {
            free(fetcher->doc->html_content);
            free(fetcher->doc);
            free(fetcher);
            return nullptr;
        }
        fetcher->curl_handle = curl_easy_init();
        if (!fetcher->curl_handle) {
            free(fetcher->doc->url);
            free(fetcher->doc->html_content);
            free(fetcher->doc);
            free(fetcher);
            return nullptr;
        }
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_CAINFO, "../config/cacert.pem");
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_WRITEDATA, fetcher->doc);
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        CURLcode res = curl_easy_perform(fetcher->curl_handle);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(fetcher->curl_handle);
            free(fetcher->doc->url);
            free(fetcher->doc->html_content);
            free(fetcher->doc);
            free(fetcher);
            return nullptr;
        }
        long http_code = 0;
        curl_easy_getinfo(fetcher->curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
        fetcher->doc->http_status = http_code;
        fetcher->parse_tree = gumbo_parse(fetcher->doc->html_content);
        return fetcher;
    }

    inline int reset_fetcher(HTMLFetcher *fetcher, const char *url) {
        if (!fetcher)
            return -1;
        FetchedDocument *old_doc = fetcher->doc;
        FetchedDocument *new_doc = (FetchedDocument *)malloc(sizeof(FetchedDocument));
        if (!new_doc)
            return -1;
        new_doc->html_content = (char *)malloc(1);
        if (!new_doc->html_content) {
            free(new_doc);
            return -1;
        }
        new_doc->content_length = 0;
        new_doc->url = strdup(url);
        if (!new_doc->url) {
            free(new_doc->html_content);
            free(new_doc);
            return -1;
        }
        fetcher->doc = new_doc;
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(fetcher->curl_handle, CURLOPT_WRITEDATA, new_doc);
        CURLcode res = curl_easy_perform(fetcher->curl_handle);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
            free(fetcher->doc->html_content);
            free(fetcher->doc->url);
            free(fetcher->doc);
            fetcher->doc = old_doc;
            return -1;
        }
        long http_code = 0;
        curl_easy_getinfo(fetcher->curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
        fetcher->doc->http_status = http_code;
        if (fetcher->parse_tree)
            gumbo_destroy_output(&kGumboDefaultOptions, fetcher->parse_tree);
        fetcher->parse_tree = gumbo_parse(fetcher->doc->html_content);
        if (old_doc) {
            free(old_doc->html_content);
            free(old_doc->url);
            free(old_doc);
        }
        return 0;
    }

    inline void free_fetcher(HTMLFetcher *fetcher) {
        if (!fetcher)
            return;
        if (fetcher->parse_tree)
            gumbo_destroy_output(&kGumboDefaultOptions, fetcher->parse_tree);
        if (fetcher->curl_handle)
            curl_easy_cleanup(fetcher->curl_handle);
        if (fetcher->doc) {
            free(fetcher->doc->html_content);
            free(fetcher->doc->url);
            free(fetcher->doc);
        }
        free(fetcher);
    }

    inline std::string get_text(const GumboNode *tag) {
        std::string res{};
        GetText(tag, res);
        return removewhspaces(res);
    }

    inline std::string get_text_excstylescript(const GumboNode *tag) {
        std::string res{};
        GetTextExcStyleScript(tag, res);
        return removewhspaces(res);
    }

    inline std::string get_html(const GumboNode *tag) {
        std::string res{};
        res = GetHtmlView(tag);
        return removewhspaces(res);
    }

    inline std::string get_html_excstylescript(const GumboNode *tag) {
        std::string res{};
        res = GetHtmlViewExcStyleScript(tag);
        return removewhspaces(res);
    }

    inline std::string get_html_excstylescript_less(const GumboNode *tag) {
        std::string res{};
        res = GetHtmlViewExcStyleScriptLess(tag);
        return removewhspaces(res);
    }

    inline GumboNode *find(const GumboNode *tag,
                           GumboTag tag_name,
                           const std::string &attr_name,
                           const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return nullptr;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty())
                return const_cast<GumboNode *>(tag);
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr && std::string(attr->value) == attr_value)
                return const_cast<GumboNode *>(tag);
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            GumboNode *found_node = find(static_cast<GumboNode *>(children->data[i]), tag_name, attr_name, attr_value);
            if (found_node)
                return found_node;
        }
        return nullptr;
    }

    inline std::vector<GumboNode *> find_all(const GumboNode *tag,
                                             GumboTag tag_name,
                                             const std::string &attr_name,
                                             const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return {};
        std::vector<GumboNode *> results;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty()) {
                results.push_back(const_cast<GumboNode *>(tag));
            } else {
                GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
                if (attr && std::string(attr->value) == attr_value)
                    results.push_back(const_cast<GumboNode *>(tag));
            }
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            std::vector<GumboNode *> child_results = find_all(static_cast<GumboNode *>(children->data[i]), tag_name,
                                                                   attr_name, attr_value);
            results += child_results;
        }
        return results;
    }

    inline GumboNode *find_anyv(const GumboNode *tag,
                                GumboTag tag_name,
                                const std::string &attr_name,
                                const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return nullptr;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty())
                return const_cast<GumboNode *>(tag);
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr) {
                std::vector<std::string> values = splitstr(attr->value);
                for (const auto &val: values) {
                    if (val == attr_value)
                        return const_cast<GumboNode *>(tag);
                }
            }
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            GumboNode *found = find_anyv(static_cast<GumboNode *>(children->data[i]), tag_name, attr_name, attr_value);
            if (found)
                return found;
        }
        return nullptr;
    }

    inline GumboNode *find_anysubv(const GumboNode *tag,
                                   GumboTag tag_name,
                                   const std::string &attr_name,
                                   const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return nullptr;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty())
                return const_cast<GumboNode *>(tag);
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr && std::string(attr->value).find(attr_value) != std::string::npos)
                return const_cast<GumboNode *>(tag);
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            GumboNode *found = find_anysubv(static_cast<GumboNode *>(children->data[i]), tag_name, attr_name,
                                                attr_value);
            if (found)
                return found;
        }
        return nullptr;
    }

    inline std::vector<GumboNode *> find_all_anyv(const GumboNode *tag,
                                                  GumboTag tag_name,
                                                  const std::string &attr_name,
                                                  const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return {};
        std::vector<GumboNode *> results;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty()) {
                results.push_back(const_cast<GumboNode *>(tag));
            } else {
                GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
                if (attr) {
                    std::vector<std::string> values = splitstr(attr->value);
                    for (const auto &val: values) {
                        if (val == attr_value) {
                            results.push_back(const_cast<GumboNode *>(tag));
                            break;
                        }
                    }
                }
            }
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            std::vector<GumboNode *> child_results = find_all_anyv(static_cast<GumboNode *>(children->data[i]),
                                                                   tag_name, attr_name, attr_value);
            results += child_results;
        }
        return results;
    }

    inline std::vector<GumboNode *> find_all_anysubv(const GumboNode *tag,
                                                     GumboTag tag_name,
                                                     const std::string &attr_name,
                                                     const std::string &attr_value) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return {};
        std::vector<GumboNode *> results;
        if (tag->v.element.tag == tag_name) {
            if (attr_name.empty()) {
                results.push_back(const_cast<GumboNode *>(tag));
            } else {
                GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
                if (attr && std::string(attr->value).find(attr_value) != std::string::npos)
                    results.push_back(const_cast<GumboNode *>(tag));
            }
        }
        const GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            std::vector<GumboNode *> child_results = find_all_anysubv(
                static_cast<GumboNode *>(children->data[i]), tag_name, attr_name, attr_value);
            results += child_results;
        }
        return results;
    }

    inline GumboNode *find_excclass(GumboNode *tag, GumboTag target_tag, const std::string &attr_name,
                                    const std::string &attr_value,
                                    const std::string &exc_class) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return nullptr;
        if (!exc_class.empty() && tag->v.element.tag == GUMBO_TAG_DIV) {
            GumboAttribute *class_attr = gumbo_get_attribute(&tag->v.element.attributes, "class");
            if (class_attr && std::string(class_attr->value) == exc_class);
            return nullptr;
        }
        if (tag->v.element.tag == target_tag) {
            if (exc_class.empty())
                return tag;
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr && std::string(attr->value) == attr_value)
                return tag;
        }
        GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            auto found = find_excclass(static_cast<GumboNode *>(children->data[i]), target_tag, attr_name, attr_value, exc_class);
            if (found)
                return found;
        }
        return nullptr;
    }

    inline GumboNode *find_exctag(GumboNode *tag, GumboTag target_tag, const std::string &attr_name,
                                  const std::string &attr_value,
                                  const GumboTag &exc_tag) {
        if (!tag || tag->type != GUMBO_NODE_ELEMENT)
            return nullptr;
        if (tag->v.element.tag == exc_tag)
            return nullptr;
        if (tag->v.element.tag == target_tag) {
            if (attr_value.empty())
                return tag;
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr && std::string(attr->value) == attr_value)
                return tag;
        }
        GumboVector *children = &tag->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            auto found = find_exctag(static_cast<GumboNode *>(children->data[i]), target_tag, attr_name, attr_value, exc_tag);
            if (found)
                return found;
        }
        return nullptr;
    }

    inline GumboNode *extract(const std::vector<GumboNode *> &tags,
                              GumboTag tag_name,
                              const std::string &attr_name,
                              const std::string &attr_value) {
        if (tags.empty())
            return nullptr;
        for (GumboNode *tag: tags) {
            if (!tag || tag->type != GUMBO_NODE_ELEMENT)
                continue;
            if (tag->v.element.tag != tag_name)
                continue;
            if (attr_name.empty())
                return tag;
            GumboAttribute *attr = gumbo_get_attribute(&tag->v.element.attributes, attr_name.c_str());
            if (attr && attr->value && std::string(attr->value) == attr_value)
                return tag;
        }
        return nullptr;
    }

} // namespace wp

#endif // WEB_PARSER_H

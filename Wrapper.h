#ifndef WRAPPER_H
#define WRAPPER_H

#include <string>
#include <vector>
#include <sstream>

#include <gumbo.h>
#include <curl/curl.h>

template<typename T>
std::vector<T>& operator+=(std::vector<T> &_lv, const std::vector<T> &_rv) {
    _lv.insert(_lv.end(), _rv.begin(), _rv.end());
    return _lv;
}

std::vector<std::string> SplitStr(const std::string &_str) {
    if (_str.empty()) {
        return {};
    }

    std::vector<std::string> tokens;
    std::istringstream string_stream(_str);
    std::string token;

    while (string_stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string RemoveWhspaces(const std::string &_str) {
    if (_str.empty()) {
        return {};
    }

    std::string res;
    res.reserve(_str.length() / 4 * 3);

    bool started = false;
    bool spacePending = false;

    for (const char c: _str) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (started) {
                spacePending = true;
            }
        } else {
            if (spacePending) {
                res += ' ';
                spacePending = false;
            }
            res += c;
            started = true;
        }
    }

    return res;
}

std::string GetTagStr(const GumboStringPiece &_piece) {
    if (!_piece.data || _piece.length == 0) {
        return "";
    }
    return {_piece.data, _piece.length};
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

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
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

HTMLFetcher* fetch_and_parse(const char *url) {
    HTMLFetcher *fetcher = (HTMLFetcher *)malloc(sizeof(HTMLFetcher));
    fetcher->doc = (FetchedDocument *)malloc(sizeof(FetchedDocument));
    fetcher->doc->html_content = (char *)malloc(1);
    fetcher->doc->content_length = 0;
    fetcher->doc->url = strdup(url);
    
    fetcher->curl_handle = curl_easy_init();
    if (!fetcher->curl_handle) return NULL;
    
    curl_easy_setopt(fetcher->curl_handle, CURLOPT_CAINFO, "config/cacert.pem");
    curl_easy_setopt(fetcher->curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(fetcher->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(fetcher->curl_handle, CURLOPT_WRITEDATA, fetcher->doc);
    curl_easy_setopt(fetcher->curl_handle, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(fetcher->curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
        return NULL;
    }
    
    long http_code = 0;
    curl_easy_getinfo(fetcher->curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    fetcher->doc->http_status = http_code;
    
    // Parse the HTML
    fetcher->parse_tree = gumbo_parse(fetcher->doc->html_content);
    
    return fetcher;
}

void free_fetcher(HTMLFetcher *fetcher) {
    if (!fetcher) return;
    
    if (fetcher->parse_tree) {
        gumbo_destroy_output(&kGumboDefaultOptions, fetcher->parse_tree);
    }
    if (fetcher->curl_handle) {
        curl_easy_cleanup(fetcher->curl_handle);
    }
    if (fetcher->doc) {
        free(fetcher->doc->html_content);
        free(fetcher->doc->url);
        free(fetcher->doc);
    }
    free(fetcher);
}

GumboNode* find_element(GumboNode *root, GumboTag tag) {
    if (root->type != GUMBO_NODE_ELEMENT) return NULL;
    if (root->v.element.tag == tag) return root;
    
    GumboVector *children = &root->v.element.children;
    for (size_t i = 0; i < children->length; i++) {
        GumboNode *result = (GumboNode *)find_element((GumboNode *)children->data[i], tag);
        if (result) return result;
    }
    return NULL;
}

const char* get_element_text(GumboNode *element) {
    if (element->type != GUMBO_NODE_ELEMENT) return NULL;
    
    GumboVector *children = &element->v.element.children;
    if (children->length > 0) {
        GumboNode *text_node = (GumboNode *)children->data[0];
        if (text_node->type == GUMBO_NODE_TEXT) {
            return text_node->v.text.text;
        }
    }
    return NULL;
}

namespace {
    void __GetText(const GumboNode *_tag, std::string &_res) {
        if (!_tag) {
            return;
        }
        if (_tag->type == GUMBO_NODE_TEXT || _tag->type == GUMBO_NODE_WHITESPACE) {
            _res += std::string(_tag->v.text.text);
            return;
        }
        if (_tag->type != GUMBO_NODE_ELEMENT) {
            return;
        }
        const GumboVector *children = &_tag->v.element.children;
        for (size_t i = 0; i < children->length; ++i) {
            _res += " ";
            __GetText(static_cast<GumboNode *>(children->data[i]), _res);
        }
    }

    void __GetTextExcStyleScript(const GumboNode *_tag, std::string &_res) {
        if (!_tag) {
            return;
        }
        if (_tag->type == GUMBO_NODE_TEXT || _tag->type == GUMBO_NODE_WHITESPACE) {
            _res += std::string(_tag->v.text.text);
            return;
        }
        if (_tag->type != GUMBO_NODE_ELEMENT) {
            return;
        }
        if (_tag->v.element.tag == GUMBO_TAG_STYLE || _tag->v.element.tag == GUMBO_TAG_SCRIPT)
            return;
        const GumboVector *children = &_tag->v.element.children;
        for (size_t i = 0; i < children->length; ++i) {
            _res += " ";
            __GetTextExcStyleScript(static_cast<GumboNode *>(children->data[i]), _res);
        }
    }

    std::string __GetHtmlView(const GumboNode *node) {
        if (!node)
            return "";

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            return GetTagStr(node->v.text.original_text);
        }

        if (node->type != GUMBO_NODE_ELEMENT)
            return "";

        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";

        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            result += __GetHtmlView(static_cast<const GumboNode *>(children->data[i]));
        }

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

        if (!is_void) {
            result += "</" + std::string(tag_name) + ">";
        }

        return result;
    }

    std::string __GetHtmlViewExcStyleScript(const GumboNode *node) {
        if (!node)
            return "";

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            return GetTagStr(node->v.text.original_text);
        }

        if (node->type != GUMBO_NODE_ELEMENT)
            return "";

        if (node->v.element.tag == GUMBO_TAG_STYLE || node->v.element.tag == GUMBO_TAG_SCRIPT)
            return "";

        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";

        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            result += __GetHtmlViewExcStyleScript(static_cast<const GumboNode *>(children->data[i]));
        }

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

        if (!is_void) {
            result += "</" + std::string(tag_name) + ">";
        }

        return result;
    }

    std::string __GetHtmlViewExcStyleScriptLess(const GumboNode *node) {
        if (!node)
            return "";

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            return GetTagStr(node->v.text.original_text);
        }

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

        for (size_t j = 0; j < sizeof(void_tags) / sizeof(GumboTag); ++j) {
            if (node->v.element.tag == void_tags[j])
                return "";
        }

        const char *tag_name = gumbo_normalized_tagname(node->v.element.tag);
        std::string result = "<" + std::string(tag_name) + ">";

        const GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            result += __GetHtmlViewExcStyleScriptLess(static_cast<const GumboNode *>(children->data[i]));
        }

        result += "</" + std::string(tag_name) + ">";

        return result;
    }

} // namespace

std::string GetText(const GumboNode *_tag) {
    std::string res{};
    __GetText(_tag, res);
    return RemoveWhspaces(res);
}

std::string GetTextExcStyleScript(const GumboNode *_tag) {
    std::string res{};
    __GetTextExcStyleScript(_tag, res);
    return RemoveWhspaces(res);
}

std::string GetHtmlViewExcStyleScript(const GumboNode *_tag) {
    std::string res{};
    res = __GetHtmlViewExcStyleScript(_tag);
    return RemoveWhspaces(res);
}

std::string GetHtmlViewExcStyleScriptLess(const GumboNode *_tag) {
    std::string res{};
    res = __GetHtmlViewExcStyleScriptLess(_tag);
    return RemoveWhspaces(res);
}

GumboNode *FindTag(const GumboNode *_tag,
                   GumboTag _tagName,
                   const std::string &_attrName,
                   const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            return const_cast<GumboNode *>(_tag);
        }
        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());
        if (attr && std::string(attr->value) == _attrValue) {
            return const_cast<GumboNode *>(_tag);
        }
    }
    const GumboVector *children = &_tag->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode *foundNode = FindTag(static_cast<GumboNode *>(children->data[i]), _tagName, _attrName, _attrValue);
        if (foundNode) {
            return foundNode;
        }
    }
    return nullptr;
}

std::vector<GumboNode *> FindAllTags(const GumboNode *_tag,
                                     GumboTag _tagName,
                                     const std::string &_attrName,
                                     const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    std::vector<GumboNode *> results;

    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            results.push_back(const_cast<GumboNode *>(_tag));
        } else {
            GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());

            if (attr && std::string(attr->value) == _attrValue) {
                results.push_back(const_cast<GumboNode *>(_tag));
            }
        }
    }

    const GumboVector *children = &_tag->v.element.children;

    for (unsigned int i = 0; i < children->length; ++i) {
        std::vector<GumboNode *> child_results = FindAllTags(static_cast<GumboNode *>(children->data[i]), _tagName,
                                                              _attrName, _attrValue);
        results += child_results;
    }

    return results;
}

GumboNode *FindTagAnyval(const GumboNode *_tag,
                         GumboTag _tagName,
                         const std::string &_attrName,
                         const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            return const_cast<GumboNode *>(_tag);
        }

        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());

        if (attr) {
            std::vector<std::string> values = SplitStr(attr->value);
            for (const auto &val: values) {
                if (val == _attrValue) {
                    return const_cast<GumboNode *>(_tag);
                }
            }
        }
    }

    const GumboVector *children = &_tag->v.element.children;

    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode *found = FindTagAnyval(static_cast<GumboNode *>(children->data[i]), _tagName, _attrName, _attrValue);

        if (found) {
            return found;
        }
    }

    return nullptr;
}

GumboNode *FindTagAnysubval(const GumboNode *_tag,
                            GumboTag _tagName,
                            const std::string &_attrName,
                            const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            return const_cast<GumboNode *>(_tag);
        }

        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());

        if (attr && std::string(attr->value).find(_attrValue) != std::string::npos) {
            return const_cast<GumboNode *>(_tag);
        }
    }

    const GumboVector *children = &_tag->v.element.children;

    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode *found = FindTagAnysubval(static_cast<GumboNode *>(children->data[i]), _tagName, _attrName,
                                            _attrValue);

        if (found) {
            return found;
        }
    }

    return nullptr;
}

std::vector<GumboNode *> FindAllTagsAnyval(const GumboNode *_tag,
                                           GumboTag _tagName,
                                           const std::string &_attrName,
                                           const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    std::vector<GumboNode *> results;

    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            results.push_back(const_cast<GumboNode *>(_tag));
        } else {
            GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());

            if (attr) {
                std::vector<std::string> values = SplitStr(attr->value);
                for (const auto &val: values) {
                    if (val == _attrValue) {
                        results.push_back(const_cast<GumboNode *>(_tag));
                        break;
                    }
                }
            }
        }
    }

    const GumboVector *children = &_tag->v.element.children;

    for (unsigned int i = 0; i < children->length; ++i) {
        std::vector<GumboNode *> childResults = FindAllTagsAnyval(static_cast<GumboNode *>(children->data[i]),
                                                                  _tagName, _attrName, _attrValue);
        results += childResults;
    }

    return results;
}

std::vector<GumboNode *> FindAllTagsAnysubval(const GumboNode *_tag,
                                              GumboTag _tagName,
                                              const std::string &_attrName,
                                              const std::string &_attrValue) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    std::vector<GumboNode *> results;

    if (_tag->v.element.tag == _tagName) {
        if (_attrName.empty()) {
            results.push_back(const_cast<GumboNode *>(_tag));
        } else {
            GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());

            if (attr && std::string(attr->value).find(_attrValue) != std::string::npos) {
                results.push_back(const_cast<GumboNode *>(_tag));
            }
        }
    }

    const GumboVector *children = &_tag->v.element.children;

    for (unsigned int i = 0; i < children->length; ++i) {
        std::vector<GumboNode *> childResults = FindAllTagsAnysubval(
            static_cast<GumboNode *>(children->data[i]), _tagName, _attrName, _attrValue);
        results += childResults;
    }

    return results;
}

std::string GetHtmlView(const GumboNode *_tag) {
    std::string res{};
    res = __GetHtmlView(_tag);
    return RemoveWhspaces(res);
}

GumboNode *FindTagWithClassExc(GumboNode *_tag, GumboTag _targetTag, const std::string &_attrName,
                                       const std::string &_attrValue,
                                       const std::string &_excClass) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT)
        return nullptr;

    if (!_excClass.empty() && _tag->v.element.tag == GUMBO_TAG_DIV) {
        GumboAttribute *class_attr = gumbo_get_attribute(&_tag->v.element.attributes, "class");
        if (class_attr && std::string(class_attr->value) == _excClass) {
            return nullptr;
        }
    }

    if (_tag->v.element.tag == _targetTag) {
        if (_excClass.empty())
            return _tag;
        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());
        if (attr && std::string(attr->value) == _attrValue)
            return _tag;
    }

    GumboVector *children = &_tag->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        auto found = FindTagWithClassExc(static_cast<GumboNode *>(children->data[i]), _targetTag, _attrName, _attrValue, _excClass);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

GumboNode *FindTagWithTagExc(GumboNode *_tag, GumboTag _targetTag, const std::string &_attrName,
                                     const std::string &_attrValue,
                                     const GumboTag &_excTag) {
    if (!_tag || _tag->type != GUMBO_NODE_ELEMENT)
        return nullptr;

    if (_tag->v.element.tag == _excTag) {
        return nullptr;
    }

    if (_tag->v.element.tag == _targetTag) {
        if (_attrValue.empty())
            return _tag;
        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());
        if (attr && std::string(attr->value) == _attrValue)
            return _tag;
    }

    GumboVector *children = &_tag->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        auto found = FindTagWithTagExc(static_cast<GumboNode *>(children->data[i]), _targetTag, _attrName, _attrValue, _excTag);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

GumboNode* ExtractTag(const std::vector<GumboNode*> &_tags,
                       GumboTag _tagName,
                       const std::string &_attrName,
                       const std::string &_attrValue) {
    if (_tags.empty()) {
        return nullptr;
    }

    for (GumboNode* _tag: _tags) {
        if (!_tag || _tag->type != GUMBO_NODE_ELEMENT) {
            continue;
        }
        if (_tag->v.element.tag != _tagName) {
            continue;
        }

        if (_attrName.empty()) {
            return _tag;
        }

        GumboAttribute *attr = gumbo_get_attribute(&_tag->v.element.attributes, _attrName.c_str());
        if (attr && attr->value && std::string(attr->value) == _attrValue) {
            return _tag;
        }
    }

    return nullptr;
}

#endif // WRAPPER_H

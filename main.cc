#include <iostream>
#include <string>

#include <curl/curl.h>
#include <gumbo.h>

#include "Wrapper.h"

using namespace std;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    

    curl_global_cleanup();
    return 0;
}

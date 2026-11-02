#ifndef ASSEMBLY_SERVER_H
#define ASSEMBLY_SERVER_H
#include "mystring.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
class SearchService {
public:
    const char* name;
    mstring     title;
    const char* host;
    uint16_t    port;
    const char* client_id;
    const char* url_search;
    const char* url_patterns;
    const char* url_entries;
    const char* url_download;

    SearchService(const char* _name, const char* _host, uint16_t _port,
                  const char* _client_id, const char* _usearch,
                  const char* _upatterns, const char* _uentries,
                  const char* _udownload)
    {
        name = strdup(_name);
        title += "\em  ";
        title += name;
        title += " Search";

        host = strdup(_host);
        port = _port;
        client_id    = strdup(_client_id);
        url_search   = strdup(_usearch);
        url_patterns = strdup(_upatterns);
        url_entries  = strdup(_uentries);
        url_download = strdup(_udownload);
    }
    ~SearchService() {
        free((void*)name); free((void*)host); free((void*)client_id);
        free((void*)url_search); free((void*)url_patterns);
        free((void*)url_entries); free((void*)url_download);
    }
private:
    SearchService(const SearchService&);
    SearchService& operator=(const SearchService&);

};

#endif

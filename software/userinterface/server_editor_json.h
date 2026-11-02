#ifndef SERVER_EDITOR_JSON_H
#define SERVER_EDITOR_JSON_H
#include "json.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
// Bounded JSON writer: the legacy JSON::render does not escape strings.
class ServerEditorJson {
    char *buf; size_t cap, used;
    bool ok;
    void put(char c) { if (used + 1 >= cap) ok = false; else buf[used++] = c; }
    void text(const char *s) { while (*s) put(*s++); }
    void quoted(const char *s) {
        static const char hex[] = "0123456789abcdef";
        put('"');
        for (; *s; ++s) {
            unsigned char c = *s;
            if (c == '"' || c == '\\') { put('\\'); put(c); }
            else if (c < 32) { text("\\u00"); put(hex[c >> 4]); put(hex[c & 15]); }
            else put(c);
        }
        put('"');
    }
    void value(JSON *j, unsigned depth) {
        if (!j || depth > 16) { ok = false; return; }
        switch(j->type()) {
        case eString: quoted(((JSON_String *)j)->get_string()); break;
        case eInteger: case eBool: text(j->render()); break;
        case eList: {
            JSON_List *l = (JSON_List *)j; put('[');
            for (int i = 0; i < l->get_num_elements(); ++i) {
                if (i) put(','); value((*l)[i], depth + 1);
            }
            put(']'); break;
        }
        case eObject: {
            JSON_Object *o = (JSON_Object *)j;
            IndexedList<const char *> *keys = o->get_keys();
            IndexedList<JSON *> *values = o->get_values();
            put('{');
            for (int i = 0; i < keys->get_elements(); ++i) {
                if (i) put(','); quoted((*keys)[i]); put(':'); value((*values)[i], depth + 1);
            }
            put('}'); break;
        }
        default: ok = false;
        }
    }
public:
    ServerEditorJson(char *b, size_t c) : buf(b), cap(c), used(0), ok(c > 0) {}
    bool write(JSON *j) { value(j, 0); if (cap) buf[used] = 0; return ok; }
};
static inline bool server_editor_host_valid(const char *s) {
    if (!s || !*s) return false;
    for (; *s; ++s) {
        unsigned char c = *s;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
    }
    return true;
}
static inline bool server_editor_path_valid(const char *s) {
    if (!s || *s != '/') return false;
    for (; *s; ++s) if ((unsigned char)*s <= 32 || (unsigned char)*s >= 127) return false;
    return true;
}
static inline bool server_editor_header_valid(const char *s) {
    if (!s || !*s) return false;
    for (; *s; ++s) if ((unsigned char)*s < 32 || (unsigned char)*s >= 127) return false;
    return true;
}
static inline int server_editor_port(const char *s) {
    unsigned value = 0;
    if (!s || !*s) return -1;
    for (; *s; ++s) { if (*s < '0' || *s > '9') return -1; value = value * 10 + *s - '0'; if (value > 65535) return -1; }
    return value ? (int)value : -1;
}
// The project parser leaves JSON string escape sequences encoded. Decode them
// before editing so each save does not add another layer of backslashes.
static inline int server_hex(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}
static inline bool server_json_unescape(char *text) {
    char *out = text;
    for (char *p = text; *p; ++p) {
        if (*p != '\\') { *out++ = *p; continue; }
        ++p; if (!*p) return false;
        switch (*p) {
        case '"': case '\\': case '/': *out++ = *p; break;
        case 'b': *out++ = '\b'; break;
        case 'f': *out++ = '\f'; break;
        case 'n': *out++ = '\n'; break;
        case 'r': *out++ = '\r'; break;
        case 't': *out++ = '\t'; break;
        case 'u': {
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) { if (!p[1] || server_hex(*++p) < 0) return false; code = (code << 4) | server_hex(*p); }
            if (code >= 0xd800 && code <= 0xdbff) {
                if (p[1] != '\\' || p[2] != 'u') return false;
                p += 2; unsigned low = 0;
                for (int i = 0; i < 4; ++i) { if (!p[1] || server_hex(*++p) < 0) return false; low = (low << 4) | server_hex(*p); }
                if (low < 0xdc00 || low > 0xdfff) return false;
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
            } else if (code >= 0xdc00 && code <= 0xdfff) return false;
            if (!code) return false; // C-string-based JSON tree cannot preserve NUL.
            if (code < 0x80) *out++ = code;
            else if (code < 0x800) { *out++ = 0xc0 | (code >> 6); *out++ = 0x80 | (code & 63); }
            else if (code < 0x10000) { *out++ = 0xe0 | (code >> 12); *out++ = 0x80 | ((code >> 6) & 63); *out++ = 0x80 | (code & 63); }
            else { *out++ = 0xf0 | (code >> 18); *out++ = 0x80 | ((code >> 12) & 63); *out++ = 0x80 | ((code >> 6) & 63); *out++ = 0x80 | (code & 63); }
            break;
        }
        default: return false;
        }
    }
    *out = 0; return true;
}
static inline bool server_editor_decode_tree(JSON *j, unsigned depth = 0) {
    if (!j || depth > 16) return false;
    if (j->type() == eString) {
        JSON_String *s = (JSON_String *)j;
        char *copy = strdup(s->get_string()); if (!copy) return false;
        bool ok = server_json_unescape(copy);
        if (ok) s->set_string(copy);
        free(copy); return ok;
    }
    if (j->type() == eList) {
        JSON_List *l = (JSON_List *)j;
        for (int i = 0; i < l->get_num_elements(); ++i) if (!server_editor_decode_tree((*l)[i], depth + 1)) return false;
    } else if (j->type() == eObject) {
        JSON_Object *o = (JSON_Object *)j;
        for (int i = 0; i < o->get_keys()->get_elements(); ++i) {
            // Parsed objects own mutable key storage allocated by json.cc.
            if (!server_json_unescape(const_cast<char *>((*o->get_keys())[i])) ||
                !server_editor_decode_tree((*o->get_values())[i], depth + 1)) return false;
        }
    }
    return true;
}
// Reject JSON primitives that the existing integer/bool/string tree cannot
// preserve (null, floating point or oversized integers), and extra root values.
static inline bool server_editor_input_supported(const char *text, size_t length) {
    unsigned depth = 0; bool quoted = false, escaped = false, started = false, finished = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = text[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
            else if (c < 32) return false;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (finished) return false;
        if (!started) { if (c != '{') return false; started = true; }
        if (c == '"') { quoted = true; continue; }
        if (c == '{' || c == '[') { if (++depth > 16) return false; continue; }
        if (c == '}' || c == ']') { if (!depth) return false; if (!--depth) finished = true; continue; }
        if (c == ':' || c == ',') continue;
        size_t start = i;
        while (i < length && text[i] != ',' && text[i] != '}' && text[i] != ']' && text[i] != ' ' && text[i] != '\r' && text[i] != '\n' && text[i] != '\t') ++i;
        size_t n = i - start; --i;
        if ((n == 4 && !memcmp(text + start, "true", 4)) || (n == 5 && !memcmp(text + start, "false", 5))) continue;
        size_t j = start; bool negative = text[j] == '-'; if (negative) ++j;
        if (j >= start + n || (text[j] == '0' && j + 1 < start + n)) return false;
        uint64_t value = 0;
        for (; j < start + n; ++j) {
            if (text[j] < '0' || text[j] > '9') return false;
            value = value * 10 + text[j] - '0';
            if (value > (negative ? 2147483648ULL : 2147483647ULL)) return false;
        }
    }
    return started && finished && !quoted;
}
static inline bool server_editor_entry_valid(JSON_Object *o) {
    const char *strings[] = {"name", "host", "client-id", "url-search", "url-patterns", "url-entries", "url-download"};
    for (unsigned i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        JSON *v = o->get(strings[i]);
        if (v && v->type() != eString) return false;
        if (!v) continue; // Loader supplies documented defaults for missing fields.
        const char *text = ((JSON_String *)v)->get_string();
        if (i == 1 ? !server_editor_host_valid(text) : i >= 3 ? !server_editor_path_valid(text) : !server_editor_header_valid(text)) return false;
    }
    JSON *p = o->get("port");
    return !p || (p->type() == eInteger && ((JSON_Integer *)p)->get_value() > 0 && ((JSON_Integer *)p)->get_value() <= 65535);
}
#endif

#include <cassert>
#include <string>
#include <cstring>
#include <cstdio>
#include "http_request.h"
extern void attachment_to_buffer(BodyDataBlock_t *);
extern "C" void outbyte(int c) { char b = c; fwrite(&b, 1, 1, stdout); }
int main() {
    t_BufferedBody body = {};
    BodyDataBlock_t block = {eDataStart, NULL, 0, &body};
    attachment_to_buffer(&block);
    std::string text = "[";
    for (int i=0; i<100; ++i) {
        if(i) text += ",";
        text += "{\"name\":\"" + std::string(180, 'a') + "\",\"id\":\"123\",\"category\":0,\"year\":1985}";
    }
    text += "]";
    assert(text.size() > 16384);
    for (size_t off=0; off<text.size(); off+=1000) {
        block = {eDataBlock, text.data()+off, (int)std::min(size_t(1000),text.size()-off), &body};
        attachment_to_buffer(&block);
    }
    JSON *json = convert_buffer_to_json(&body);
    assert(json && json->type() == eList);
    assert(((JSON_List*)json)->get_num_elements() == 100);
    delete json;
    block = {eDataStart, NULL, 0, &body}; attachment_to_buffer(&block);
    std::string oversized(65536, ' ');
    block = {eDataBlock, oversized.data(), (int)oversized.size(), &body};
    attachment_to_buffer(&block);
    assert(body.overflow && convert_buffer_to_json(&body) == NULL);
    block = {eDataStart, NULL, 0, &body}; attachment_to_buffer(&block);
    block = {eDataBlock, "[]", 2, &body}; attachment_to_buffer(&block);
    json = convert_buffer_to_json(&body); assert(json); delete json;
    assert(convert_buffer_to_json(NULL) == NULL);
    puts("http_buffer_test: OK (100 results >16KiB; overflow rejected; reset recovery)");
}

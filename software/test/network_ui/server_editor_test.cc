#include <cassert>
#include <string>
#include <cstdio>
#include "server_editor_json.h"
extern "C" void outbyte(int c) { fputc(c, stdout); }
int main() {
 assert(server_editor_host_valid("192.168.1.100"));assert(server_editor_host_valid("files.local"));
 for (auto bad : {"", "http://files.local", "a/b", "a\r\nX: bad", "a:8000", "a b"}) assert(!server_editor_host_valid(bad));
 for (auto good : {"1","80","8000","65535"}) assert(server_editor_port(good)>0);
 for (auto bad : {"", "0", "65536", "99999999999999999", "80x", "-1"}) assert(server_editor_port(bad)<0);
 assert(server_editor_path_valid("/leet/search/aql/0/100?query="));assert(!server_editor_path_valid("/x\r\nBad: 1"));
 JSON_Object *tree=JSON::Obj()->add("assembly64",JSON::List()->add(JSON::Obj()->add("name","quote\"slash\\")->add("host","files.local")->add("port",8000)))->add("extra",true);
 char output[4096];ServerEditorJson writer(output,sizeof(output));assert(writer.write(tree));
 assert(strstr(output,"quote\\\"slash\\\\"));assert(strstr(output,"\"extra\":true"));
 assert(server_editor_input_supported(output,strlen(output)));
 for (auto bad : {"{}{}", "{\"unknown\":null}", "{\"unknown\":1.5}", "{\"unknown\":2147483648}", "{\"unknown\":01}"}) assert(!server_editor_input_supported(bad,strlen(bad)));
 std::string first=output;JSON *parsed=NULL;
 assert(convert_text_to_json_objects(output,strlen(output),512,&parsed)>0);
 assert(server_editor_decode_tree(parsed));
 ServerEditorJson again(output,sizeof(output));assert(again.write(parsed));assert(first==output);
 char small[10];ServerEditorJson bounded(small,sizeof(small));assert(!bounded.write(tree));assert(small[9]==0);
 char unicode[]="\\u0041\\u00e9\\ud83d\\ude00";assert(server_json_unescape(unicode));assert(std::string(unicode)=="A\xc3\xa9\xf0\x9f\x98\x80");
 for (auto bad : {"\\u0000", "\\ud800", "\\udc00", "\\x", "\\u123"}) {char b[32];strcpy(b,bad);assert(!server_json_unescape(b));}
 delete tree;delete parsed;
 puts("server editor: endpoint validation, bounded JSON, quote/backslash round trip, unknown key preservation and Unicode handling passed");
}

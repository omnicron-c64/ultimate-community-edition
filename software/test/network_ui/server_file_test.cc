#include <cassert>
#include <string>
#include <map>
#include <cstdio>
#include <cstdint>
#include "server_editor_json.h"
extern "C" void outbyte(int c) { fputc(c,stdout); }
enum FRESULT { FR_OK,FR_EXIST,FR_NO_FILE,FR_DISK_ERR,FR_INVALID_PARAMETER };
enum { FA_WRITE=1,FA_CREATE_ALWAYS=2 };
static std::map<std::string,std::string> files;
static int failure;
class File {
public:
 FRESULT write(const void *b,uint32_t n,uint32_t *written) {
  if(failure==1)return FR_DISK_ERR;
  *written=failure==2?n-1:n;files["/Flash/config/server.json.tmp"]=std::string((const char*)b,*written);return FR_OK;
 }
 FRESULT sync() { return failure==3?FR_DISK_ERR:FR_OK; }
};
class FileManager {
public:
 static FileManager *getFileManager() { static FileManager fm;return &fm; }
 FRESULT create_dir(const char*) { return FR_EXIST; }
 FRESULT fopen(const char*,int,File **out) { *out=new File;return FR_OK; }
 void fclose(File *f) { delete f; }
 FRESULT delete_file(const char *p) { return files.erase(p)?FR_OK:FR_NO_FILE; }
 FRESULT rename(const char *a,const char *b) {
  if(failure==4 && std::string(a)=="/Flash/config/server.json.tmp")return FR_DISK_ERR;
  if(!files.count(a))return FR_NO_FILE;
  files[b]=files[a];files.erase(a);return FR_OK;
 }
};
#include "server_editor_file.h"
int main() {
 JSON_Object *doc=JSON::Obj()->add("assembly64",JSON::List()->add(JSON::Obj()->add("host","files.local")));
 for(failure=1;failure<=4;++failure) {
  files.clear();files["/Flash/config/server.json"]="original";
  assert(save_servers(doc)!=FR_OK);assert(files["/Flash/config/server.json"]=="original");
 }
 failure=0;files.clear();files["/Flash/config/server.json"]="original";
 assert(save_servers(doc)==FR_OK && files["/Flash/config/server.json.bak"]=="original");
 assert(files["/Flash/config/server.json"].find("files.local")!=std::string::npos);
 delete doc;puts("server save: write error, short write, sync error, install rollback, backup and success passed");
}

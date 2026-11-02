#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include <algorithm>
enum FRESULT { FR_OK,FR_INT_ERR,FR_DISK_ERR,FR_NOT_READY,FR_EXIST,FR_DISK_FULL,FR_NOT_ENOUGH_CORE };
enum { FA_READ=1,FA_WRITE=2,FA_CREATE_NEW=4,FA_CREATE_ALWAYS=8,FA_CREATE_ANY=12 };
class mstring { std::string s;
public: mstring(const char *v=""):s(v){}const char *c_str()const{return s.c_str();}size_t length()const{return s.size();} };
class FileSystem {public:virtual ~FileSystem(){};};
class File { FileSystem *fs;
public:File(FileSystem*f=NULL):fs(f){}virtual ~File(){};FileSystem *get_file_system(){return fs;}
 virtual FRESULT close(){delete this;return FR_OK;}
 virtual FRESULT sync(){return FR_OK;}virtual FRESULT seek(uint32_t){return FR_OK;}
 virtual FRESULT read(void*,uint32_t,uint32_t*){return FR_OK;}
 virtual FRESULT write(const void*,uint32_t,uint32_t*){return FR_OK;}
 virtual void abort_write(){}
};
static std::map<std::string,std::string> storage;static unsigned seq=0,uploads=0,messages=0;
class TempFile:public File {std::string key;unsigned offset=0;
public:TempFile(std::string k):key(k){}
 FRESULT read(void*out,uint32_t n,uint32_t*got){*got=std::min((size_t)n,storage[key].size()-offset);memcpy(out,storage[key].data()+offset,*got);offset+=*got;return FR_OK;}
 FRESULT write(const void*in,uint32_t n,uint32_t*done){storage[key].append((const char*)in,n);*done=n;return FR_OK;}
 FRESULT seek(uint32_t n){offset=n;return FR_OK;}
};
class FileManager {
public:static FileManager*getFileManager(){static FileManager f;return &f;}
 FRESULT create_temp_file(const char*,const char*,int,File**f,mstring*path){std::string key="temp"+std::to_string(++seq);storage[key]="";*path=mstring(key.c_str());*f=new TempFile(key);return FR_OK;}
 FRESULT fclose(File*f){return f->close();}
 FRESULT delete_file(const char*p){storage.erase(p);return FR_OK;}
};
class FTPClient {
public:bool fail_download=false,fail_upload=false,fail_list=false;std::string payload="data";
 const char*last_error(){return "test failure";}
 int retr(const char*,File*f,int*bytes){uint32_t n;f->write(payload.data(),payload.size(),&n);*bytes=n;return fail_download?-1:0;}
 int stor(const char*,File*){++uploads;return fail_upload?-1:0;}
 int list_lines(const char*,bool(*)(void*,const char*,bool),void*){return fail_list?-1:0;}
};
class UserInterface {public:static void postMessage(const char*){++messages;}};
struct FTPExists {const char*name;bool found,unsupported;};
bool ftp_check_exists(void*,const char*,bool){return true;}
class FileSystemFTP:public FileSystem {
public:FTPClient backend;FTPClient *client=&backend;bool offline=false;
 FTPClient*get_client(){return client;}int connect_if_needed(){return offline?-1:0;}
 void build_ftp_path(const char*,mstring&out){out=mstring("/file.d64");}
 FRESULT file_open(const char*,uint8_t,File**);
};
class FileOnFTP:public File {
public:File*cached;bool needs_upload,write_failed; mstring remote_path,temp_path;
 FileOnFTP(FileSystem*f,File*c,const char*p,bool u,const char*t):File(f),cached(c),needs_upload(u),write_failed(false),remote_path(p),temp_path(t){}
 void abort_write(){write_failed=true;needs_upload=false;}
 FRESULT close();
};
#include "ftp_file_transfer.h"
#include "file_copy_stream.h"
class StreamFile:public File {
public:bool aborted=false,short_write=false,read_failure=false;FRESULT closed=FR_OK;int reads=0;
 FRESULT close(){return closed;} // Test keeps objects alive to inspect abort/close results.
 FRESULT read(void*out,uint32_t,uint32_t*n){if(read_failure)return FR_DISK_ERR;*n=reads++?0:4;memset(out,'X',*n);return FR_OK;}
 FRESULT write(const void*,uint32_t n,uint32_t*done){*done=short_write?n-1:n;return FR_OK;}
 void abort_write(){aborted=true;}
};
int main(){
 FileManager *fm=FileManager::getFileManager();FileSystemFTP a,b;File *f=NULL,*g=NULL;
 a.backend.fail_download=true;assert(a.file_open("same.d64",FA_READ,&f)==FR_DISK_ERR && !f && storage.empty());
 a.backend.fail_download=false;a.backend.payload="server-A";b.backend.payload="server-B";
 assert(a.file_open("same.d64",FA_READ,&f)==FR_OK);assert(b.file_open("same.d64",FA_READ,&g)==FR_OK);
 assert(storage.size()==2);assert(((FileOnFTP*)f)->temp_path.c_str()!=((FileOnFTP*)g)->temp_path.c_str());
 assert(fm->fclose(f)==FR_OK && fm->fclose(g)==FR_OK && storage.empty());
 assert(a.file_open("write.d64",FA_CREATE_ALWAYS,&f)==FR_OK);a.backend.fail_upload=true;
 assert(fm->fclose(f)==FR_DISK_ERR && storage.empty());
 assert(a.file_open("write.d64",FA_CREATE_ALWAYS,&f)==FR_OK);unsigned before=uploads;f->abort_write();
 assert(fm->fclose(f)==FR_DISK_ERR && uploads==before && storage.empty());
 a.backend.fail_list=true;assert(a.file_open("write.d64",FA_CREATE_NEW,&f)==FR_DISK_ERR && storage.empty());
 StreamFile in,out;out.closed=FR_DISK_ERR;assert(copy_stream_and_close(fm,&in,&out)==FR_DISK_ERR);
 StreamFile in2,out2;out2.short_write=true;assert(copy_stream_and_close(fm,&in2,&out2)==FR_DISK_FULL && out2.aborted);
 StreamFile in3,out3;in3.read_failure=true;assert(copy_stream_and_close(fm,&in3,&out3)==FR_DISK_ERR && out3.aborted);
 StreamFile in4,out4;assert(copy_stream_and_close(fm,&in4,&out4)==FR_OK && !out4.aborted);
 puts("FTP lifecycle: isolated fresh files, failed-download cleanup, upload-close propagation, aborts, copy errors and short writes passed");
}

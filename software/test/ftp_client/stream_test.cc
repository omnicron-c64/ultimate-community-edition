#include "ftp_client.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cassert>
class mstring {
    std::string text;
public:
    mstring(const char *s):text(s) {}
    const char *c_str() const { return text.c_str(); }
};
class FTPServer {
public:
    mstring host, user, passw;
    uint16_t port;
    FTPServer(int p):host("127.0.0.1"),user("test"),passw("secret"),port(p) {}
};
static unsigned errors;
class UserInterface { public: static void postMessage(const char *s) { ++errors; printf("%s\n",s); } };
class FileSystemFTP: public FileSystem {
public:
    FTPServer *server;
    FRESULT open_read_stream(const char *, File **);
};
#include "ftp_read_stream.h"
class FileManager { public: FRESULT fclose(File *f) { return f->close(); } };
#include "file_copy_stream.h"
class CheckedDestination : public File {
public:
    uint32_t offset=0; bool aborted=false;
    FRESULT close() { return FR_OK; }
    FRESULT sync() { return FR_OK; }
    FRESULT seek(uint32_t) { return FR_DISK_ERR; }
    FRESULT read(void *,uint32_t,uint32_t *) { return FR_DISK_ERR; }
    FRESULT write(const void *data,uint32_t len,uint32_t *done) {
        const unsigned char *bytes=(const unsigned char *)data;
        for(uint32_t i=0;i<len;++i)assert(bytes[i]==(offset+i)%251);
        offset+=len;*done=len;return FR_OK;
    }
    void abort_write() { aborted=true; }
};
int main(int argc, char **argv) {
    assert(argc == 3);
    std::string mode(argv[2]);
    FTPServer server(atoi(argv[1])); FileSystemFTP fs;fs.server=&server;
    if(mode=="stream-copy" || mode=="stream-copy-fail") {
        File *source=NULL;assert(fs.open_read_stream("/large.reu",&source)==FR_OK && source);
        CheckedDestination out;FileManager fm;
        FRESULT result=copy_stream_and_close(&fm,source,&out);
        if(mode=="stream-copy") assert(result==FR_OK && out.offset==16u*1024*1024 && !out.aborted);
        else assert(result==FR_DISK_ERR && out.aborted);
        printf("PASS %s copied=%u\n",argv[2],out.offset);return 0;
    }
    FTPReadStream stream(&fs,&server,"/large.reu");
    int opened=stream.open();
    if(mode=="stream-size-unsupported") {assert(opened==1);return 0;}
    if(mode=="stream-size-invalid") {assert(opened<0);return 0;}
    assert(opened==0);
    uint32_t size=mode=="stream-32m"?32u*1024*1024:16u*1024*1024;
    assert(stream.get_size()==size);
    uint32_t offset=0; unsigned char buffer[4093];
    if(mode=="stream-seek") {assert(stream.seek(12345)==FR_OK);offset=12345;}
    if(mode=="stream-backseek") {
        uint32_t got=0;assert(stream.read(buffer,1000,&got)==FR_OK && got==1000);
        assert(stream.seek(100)==FR_OK);offset=100;
    }
    bool failed=false;
    for (;;) {
        uint32_t got=0;
        if(stream.read(buffer,sizeof(buffer),&got)!=FR_OK) {failed=true;break;}
        if(!got)break;
        for(uint32_t i=0;i<got;++i)assert(buffer[i]==(offset+i)%251);
        offset+=got;
    }
    bool expected_failure=mode=="stream-truncated"||mode=="stream-completion"||mode=="stream-grown";
    assert(failed==expected_failure);
    if(!failed) {
        assert(offset==size && errors==0);
        assert(stream.seek(size+1)==FR_INVALID_PARAMETER);
    } else assert(errors==1 && stream.sync()==FR_DISK_ERR);
    printf("PASS %s checked=%u\n",argv[2],offset);
}

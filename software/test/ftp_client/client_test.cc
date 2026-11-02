#include "ftp_client.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
class MemoryFile : public File {
public:
 std::string data; unsigned offset=0; bool read_error=false,write_error=false,sync_error=false;
 FRESULT read(void *out,uint32_t length,uint32_t *done) {
  if(read_error && offset) return FR_DISK_ERR;
  *done=data.size()-offset; if(*done>length)*done=length; memcpy(out,data.data()+offset,*done); offset+=*done;return FR_OK;
 }
 FRESULT write(const void *in,uint32_t length,uint32_t *done) {
  *done=write_error?length-1:length;data.append((const char*)in,*done);return FR_OK;
 }
 FRESULT seek(uint32_t n) {offset=n;return FR_OK;}
 FRESULT sync(){return sync_error?FR_DISK_ERR:FR_OK;}
};
static bool line(void *ctx,const char *s,bool machine) {
 unsigned *count=(unsigned*)ctx;
 if(!*s)return false;
 ++*count;
 if(machine && strncmp(s,"type=file;",10))return false;
 return true;
}
static unsigned progress_calls;
static void observe(void *, const char *, uint32_t) {++progress_calls;}
int main(int argc,char **argv) {
 if(argc!=3)return 99;
 std::string mode=argv[2];FTPClient ftp;
 if(!FTPClient::observe_transfers(observe,NULL))return 98;
 int ret=ftp.open("127.0.0.1",atoi(argv[1]));
 if(!ret)ret=ftp.login("test","do-not-log-this");
 if(!ret)ret=ftp.type_binary();
 if(ret){printf("ERROR %s\n",ftp.last_error());return mode=="login-fail"||mode=="greeting-fail"||mode=="greeting-timeout"?0:2;}
 if(mode=="list"||mode=="fallback"||mode=="large-list"||mode=="bad-epsv"||mode=="bad-pasv"||mode=="listing-fail") {
  unsigned count=0;ret=ftp.list_lines("/folder with spaces",line,&count);
  if(mode=="bad-epsv"||mode=="bad-pasv"||mode=="listing-fail") { if(!ret)return 3; }
  else if(ret || count!=(mode=="large-list"?1000u:2u))return 4;
 } else if(mode=="small-buffer") {
  char buf[8];int n;ret=ftp.list("/",buf,sizeof(buf),&n);if(!ret)return 5;
 } else if(mode=="download"||mode=="write-fail"||mode=="transfer-fail"||mode=="sync-fail") {
  MemoryFile f;f.write_error=mode=="write-fail";f.sync_error=mode=="sync-fail";int n=0;ret=ftp.retr("/demo.d64",&f,&n);
  if(mode!="download") {if(!ret)return 6;}
  else {std::string expected;for(int i=0;i<12000;++i)expected+=(char)(i%251);if(ret||f.data!=expected||n!=12000)return 7;}
 } else if(mode=="upload"||mode=="read-fail") {
  MemoryFile f;f.data=std::string(12000,'X');f.read_error=mode=="read-fail";ret=ftp.stor("/new.d64",&f);
  if(mode=="read-fail" ? ret==0 : ret!=0)return 8;
 } else if(mode=="injection") {
  if(ftp.cwd("/bad\r\nDELE something")==0)return 9;
 }
 if(!progress_calls)return 97;
 FTPClient::observe_transfers(NULL,NULL);
 printf("PASS %s bytes=%u error=%s\n",argv[2],(unsigned)ftp.bytes_transferred(),ftp.last_error());return 0;
}

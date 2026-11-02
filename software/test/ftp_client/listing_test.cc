#include <cassert>
#include <cstdio>
#include <string>
struct FTPExists {const char*name;bool found,unsupported;};
#include "ftp_listing_parser.h"
int main(){char name[512];uint32_t size;bool dir;
 assert(ftp_entry("type=file;size=123; file with spaces.d64",true,name,sizeof(name),&size,&dir)&&size==123&&!dir&&std::string(name)=="file with spaces.d64");
 assert(ftp_entry("modify=20260101000000;type=dir; Folder",true,name,sizeof(name),&size,&dir)&&dir);
 assert(ftp_entry("type=cdir; .",true,name,sizeof(name),&size,&dir)&&!name[0]);
 assert(!ftp_entry("type=file;size=4294967296; overflow",true,name,sizeof(name),&size,&dir));
 assert(!ftp_entry("type=file; ../../bad",true,name,sizeof(name),&size,&dir));
 assert(ftp_entry("-rw-r--r-- 1 user group 42 Jan 01 12:00 arrow -> name.d64",false,name,sizeof(name),&size,&dir)&&std::string(name)=="arrow -> name.d64"&&size==42);
 assert(ftp_entry("01-01-24  12:00PM  <DIR>  Folder Name",false,name,sizeof(name),&size,&dir)&&dir&&std::string(name)=="Folder Name");
 assert(ftp_entry("total 24",false,name,sizeof(name),&size,&dir)&&!name[0]);
 std::string longname="-rw-r--r-- 1 u g 1 Jan 01 12:00 "+std::string(600,'a');
 assert(!ftp_entry(longname.c_str(),false,name,sizeof(name),&size,&dir));
 puts("FTP listing parser: MLSD facts, LIST fallback, spaces, long names, bounds and traversal-like names passed");
}

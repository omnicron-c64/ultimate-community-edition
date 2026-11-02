#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include "wifi_saved.h"
typedef int esp_err_t;
typedef int nvs_handle_t;
enum {ESP_OK=0, ESP_ERR_NVS_NOT_FOUND=1, ESP_ERR_INVALID_STATE=2, NVS_READWRITE=1};
struct command_buf_t { unsigned char data[2048]; size_t size; };
struct ConnectCommand_t { char pw[68]; uint8_t auth_mode; char ssid[33]; int list_index, command; command_buf_t *buf; };
std::map<std::string,std::string> db;
static int commits, closes, opened, fail_commit;
int nvs_open(const char *, int, int *h) { *h=1; ++opened; return 0; }
void nvs_close(int) { ++closes; }
int nvs_commit(int) { ++commits; return fail_commit; }
int nvs_get_str(int, const char *k, char *s, size_t *n) {
 if (!db.count(k)) return ESP_ERR_NVS_NOT_FOUND;
 if (*n <= db[k].size()) return ESP_ERR_INVALID_STATE;
 strcpy(s,db[k].c_str()); *n=db[k].size()+1; return 0;
}
int nvs_get_i8(int, const char *k, int8_t *v) { if(!db.count(k))return 1;*v=std::stoi(db[k]);return 0; }
int nvs_get_u8(int, const char *k, uint8_t *v) { int8_t t;int e=nvs_get_i8(0,k,&t);if(!e)*v=t;return e; }
int nvs_erase_key(int, const char *k) { if(!db.erase(k))return 1;return 0; }
int nvs_set_str(int, const char *k, const char *v) { db[k]=v;return 0; }
int nvs_set_u8(int, const char *k, uint8_t v) { db[k]=std::to_string(v);return 0; }
int nvs_set_i8(int, const char *k, int8_t v) { db[k]=std::to_string(v);return 0; }
#define ESP_LOGE(...)
#include "wifi_ap_credentials.h"
#include "wifi_saved_store.h"
static void entry(int n,const char *ssid) {
 std::string key="sa0";key[1]='a'+n;db[key]=ssid;key[0]='p';db[key]="secret-never-send";key[0]='m';db[key]="3";
}
int main() {
 static_assert(sizeof(wifi_saved_record)==36,"wire size");
 command_buf_t b={}; ConnectCommand_t c={};c.buf=&b;c.command=CMD_WIFI_SAVED_LIST;
 wifi_saved_record records[16];unsigned count=99;int32_t err=0;
 assert(saved_ap_command(&c)==0 && b.size==9);memcpy(b.data+4,&err,4);
 assert(wifi_saved_decode(b.data,b.size,records,&count)==0 && count==0);
 entry(0,"first");entry(15,"last");db["last"]="15";
 assert(saved_ap_command(&c)==0);assert(wifi_saved_decode(b.data,b.size,records,&count)==0 && count==2);
 assert(records[1].slot==15 && records[1].preferred && records[1].auth==3);
 assert(!memmem(b.data,b.size,"secret",6));
 for (size_t size=0;size<b.size;++size) assert(wifi_saved_decode(b.data,size,records,&count)!=0);
 b.data[8]=17;assert(wifi_saved_decode(b.data,b.size,records,&count)!=0);
 c.command=CMD_WIFI_SAVED_FORGET;c.list_index=15;strcpy(c.ssid,"stale");
 assert(saved_ap_command(&c)==ESP_ERR_INVALID_STATE && db.count("sp0"));
 strcpy(c.ssid,"last");assert(saved_ap_command(&c)==0);
 assert(!db.count("sp0")&&!db.count("pp0")&&!db.count("mp0")&&!db.count("last"));
 assert(db["sa0"]=="first" && commits==1);
 c.command=CMD_WIFI_SAVED_LIST;assert(saved_ap_command(&c)==0 && b.data[8]==1);
 entry(15,"replacement");c.command=CMD_WIFI_SAVED_FORGET;strcpy(c.ssid,"last");
 assert(saved_ap_command(&c)==ESP_ERR_INVALID_STATE && db["sp0"]=="replacement");
 strcpy(c.ssid,"replacement");fail_commit=77;assert(saved_ap_command(&c)==77);
 fail_commit=0;
 db.clear();entry(7,"already-saved");strcpy(c.ssid,"already-saved");strcpy(c.pw,"new-password");c.auth_mode=3;
 assert(wifi_store_ap(&c)==0 && c.list_index==7 && !db.count("sa0") && db["ph0"]=="new-password");
 strcpy(c.ssid,"new-ap");assert(wifi_store_ap(&c)==0 && c.list_index==0);
 int previous=commits;assert(wifi_set_last_ap(7)==0 && commits==previous+1 && db["last"]=="7");
 assert(opened==closes);
 puts("saved AP: empty/list/last marker, bounds, truncation, credential exclusion, stale slot, single delete and NVS commit errors passed");
}

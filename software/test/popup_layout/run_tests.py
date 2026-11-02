#!/usr/bin/env python3
"""Exercise production popup drawing and FTP preview code with bounded screen adapters."""
from pathlib import Path
import subprocess, tempfile, os
ROOT=Path(__file__).resolve().parents[3]
s=(ROOT/'software/userinterface/ui_elements.cc').read_text()
ctor=s[s.index('UIPopup :: UIPopup('):s.index('// Consume one bounded')]
layout=s[s.index('// Consume one bounded'):s.index('int UIPopup :: poll(')]
deinit=s[s.index('void UIPopup :: deinit()'):s.index('UIStringBox :: UIStringBox(')]
header=(ROOT/'software/userinterface/ui_elements.h').read_text()
cls=header[header.index('class UIPopup :'):header.index('// Opt-in input policy')]
pre=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
#include <stdint.h>
using mstring=std::string;
struct Keyboard {void wait_free(){} void clear_buffer(){}};
struct Screen {
 int w,h; std::vector<char> cells,saved;
 Screen(int width):w(width),h(25),cells(w*h,'x'){}
 int get_size_x(){return w;} int get_size_y(){return h;}
 void backup(){saved=cells;} void restore(){cells=saved;}
};
struct Window {
 Screen *s;int ox,oy,w,h,x=0,y=0;
 Window(Screen *p,int a,int b,int c,int d):s(p),ox(a),oy(b),w(c),h(d){assert(a>=0&&b>=0&&a+c<=p->w&&b+d<=p->h);}
 void clear(){for(int j=0;j<h;j++)for(int i=0;i<w;i++)s->cells[(oy+j)*s->w+ox+i]=' ';}
 void draw_border(){for(int j=0;j<h;j++)for(int i=0;i<w;i++)if(j==0||i==0||j==h-1||i==w-1)s->cells[(oy+j)*s->w+ox+i]='|';++ox;++oy;w-=2;h-=2;}
 void no_scroll(){} void set_color(int){} void set_background(int){} void reverse_mode(int){}
 void move_cursor(int a,int b){assert(a>=0&&b>=0&&a<w&&b<h);x=a;y=b;}
 void output(char c){assert(x>=0&&x<w&&y>=0&&y<h);s->cells[(oy+y)*s->w+ox+x++]=c;}
 void output(const char *p){while(*p)output(*p++);}
 Screen *getScreen(){return s;}
};
struct UserInterface {Screen *s;Keyboard k;int color_fg=1;UserInterface(Screen *p):s(p){} Screen *get_screen(){return s;} Keyboard *get_keyboard(){return &k;}};
struct UIObject {UserInterface *ui;UIObject(UserInterface *p):ui(p){} UserInterface *get_ui(){return ui;}};
'''
post=r'''
int main(){
 const char *names[]={" Ok "," Yes "," No "," All "," Cancel "};const char *keys="oynac";
 std::vector<std::string> messages={"", "FTP test failed.\n192.168.200.193:21\nControl connection: connection failed / timed out (FTP 0)","one\n\ntwo","one\r\ntwo",std::string(4096,'A'),"\x1b" "Acontrol bytes\t\x01"};
 for(int width:{40,80})for(int n=1;n<=160;++n)messages.push_back(std::string(n,'b'));
 for(int width:{40,80})for(auto &text:messages)for(int flags:{1,6,31}){
  Screen screen(width);UserInterface ui(&screen);UIPopup pop(&ui,text.c_str(),flags,5,names,keys);
  pop.init();pop.deinit();for(char c:screen.cells)assert(c=='x');
 }
 puts("Popup: 40/80-column bounds, long failures, blank/CRLF lines, oversized messages, buttons and restoration passed");
}
'''
# Extract the real preview loop; exercise it with the firmware formatter, not libc printf.
f=(ROOT/'software/filesystem/filesystem_ftp.cc').read_text()
preview=f[f.index('        char labels[8][72]'):f.index('        choices[6] = "Save server"')]
ftp=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
extern "C" int uce_target_snprintf(char*,size_t,const char*,...);
extern "C" void outbyte(int){}
struct Fields {std::string value;const char *string_or(const char*,const char*){return value.c_str();}};
int main(){const char *keys[]={"Alias","Host","Port","User","Password","Path"};
for(int n:{0,1,48,49,255}) {Fields values;values.value=std::string(n,'z');Fields *fields=&values;
'''+preview+r'''
for(int i=0;i<6;i++){std::string wanted=std::string(keys[i])+": "+(i==4?(n?"[set]":"[empty]"):std::string(n>48?48:n,'z'));assert(labels[i]==wanted);}
}puts("FTP previews: actual firmware formatter, empty/long fields and password summaries passed");}
'''
ftp=ftp.replace('snprintf(labels','uce_target_snprintf(labels')
product=(ROOT/'software/system/product.cc').read_text()
product=product[product.index('char *getProductVersionString('):product.index('char *getProductTitleString(')]
product=product.replace('snprintf(', 'uce_target_snprintf(')
product_test=r'''#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdint.h>
#define APPL_VERSION_ASCII "3.15a-spiffy-u64.7"
#define APPL_VERSION APPL_VERSION_ASCII
#define C64_CORE_VERSION 0x50
enum {PRODUCT_ULTIMATE_II,PRODUCT_ULTIMATE_II_PLUS,PRODUCT_ULTIMATE_II_PLUS_L,PRODUCT_ULTIMATE_64,PRODUCT_ULTIMATE_64_ELITE,PRODUCT_ULTIMATE_64_ELITE_II};
int getProductId(){return PRODUCT_ULTIMATE_64_ELITE;}
uint8_t getFpgaVersion(){return 0x50;}
const char *getProductString(){return "Ultimate 64 Elite";}
extern "C" int uce_target_snprintf(char*,size_t,const char*,...);
extern "C" void outbyte(int){}
'''+product+r'''
int main(){char guarded[49];memset(guarded,'!',sizeof guarded);assert(getProductVersionString(guarded,41,true)==guarded);assert(guarded[40]==0);for(int i=41;i<49;i++)assert(guarded[i]=='!');puts("Product heading: long legacy version bounded to caller buffer, canary intact");}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'popup.cc').write_text(pre+cls+ctor+layout+deinit+post);(d/'ftp.cc').write_text(ftp);(d/'product.cc').write_text(product_test)
 flags=['g++','-std=c++11','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
 subprocess.run(flags+[str(d/'popup.cc'),'-o',str(d/'popup')],check=True)
 # Rename only the firmware formatter's public entry points to avoid host libc interposition.
 subprocess.run(['g++','-std=c++11','-fpermissive','-w','-DRUNS_ON_PC','-ffunction-sections','-fdata-sections','-Dsnprintf=uce_target_snprintf','-Dprintf=uce_target_printf','-Dsprintf=uce_target_sprintf','-Dvsnprintf=uce_target_vsnprintf','-Dvsprintf=uce_target_vsprintf','-Dputs=uce_target_puts','-Dsscanf=uce_target_sscanf','-I'+str(ROOT/'software/system'),'-I'+str(ROOT/'software/components'),'-c',str(ROOT/'software/system/small_printf.cc'),'-o',str(d/'printf.o')],check=True)
 subprocess.run(flags+['-Wl,--gc-sections',str(d/'ftp.cc'),str(d/'printf.o'),'-o',str(d/'ftp')],check=True)
 subprocess.run(flags+['-Wl,--gc-sections',str(d/'product.cc'),str(d/'printf.o'),'-o',str(d/'product')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 for binary in ['popup','ftp','product']:subprocess.run([str(d/binary)],env=env,check=True)

#!/usr/bin/env python3
"""Compile the production picker/menu/search function bodies with native UI adapters.
No copied policy implementation: extraction fails if function signatures disappear.
"""
from pathlib import Path
import subprocess,tempfile,os
ROOT=Path(__file__).resolve().parents[3]
def body(text,signature):
 start=text.index('{',text.index(signature));depth=0;quoted=False;escape=False
 for i in range(start,len(text)):
  c=text[i]
  if quoted:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c=='"':quoted=False
   continue
  if c=='"':quoted=True
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if depth==0:return text[start:i+1]
 raise AssertionError(signature)
def main():
 tree=(ROOT/'software/userinterface/tree_browser.cc').read_text()
 context=(ROOT/'software/userinterface/context_menu.cc').read_text()
 search=(ROOT/'software/userinterface/assembly_search.h').read_text()
 searchcc=(ROOT/'software/userinterface/assembly_search.cc').read_text()
 full=body(tree,'int TreeBrowser :: handle_key(int c)')
 guard=body(full,'if (pick_mode != PICK_NONE)')
 picker_code=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include "keyboard.h"
#define MENU_CLOSE -1
#define ET_PICKER -2
#define AM_DIR 16
#define AM_VOL 8
struct FileInfo {int attrib=0;};
class Browsable {public:FileInfo info;int order=0,fetched=0;bool selectable=true;
 int getSortOrder(){return order;}bool isSelectable(){return selectable;}
 template<class T>void fetch_context_items(T &){++fetched;}
};
class BrowsableDirEntry:public Browsable {public:FileInfo*getFileInfo(){return &info;}};
class TreeBrowser;
struct State {TreeBrowser *browser=NULL;Browsable *under_cursor=NULL;State*previous=NULL;int entered=0,up=0;
 void into2(){++entered;}void level_up(){++up;}
};
class TreeBrowser {public:
 enum PickMode{PICK_NONE,PICK_LOAD,PICK_SAVE,PICK_PATH};PickMode pick_mode=PICK_PATH;
 State*state;bool allow_exit=true,picked=false;int contexts=0;
 void context(int){++contexts;}void reset_quick_seek(){}
 bool can_pick(Browsable*);
 int pick_current(){picked=true;return MENU_CLOSE;}
 int key(int c){if(pick_mode!=PICK_NONE) PICKER_GUARD
 return -999;}
};
CAN_PICK
using actionFunction_t=void*;
struct Action {Action(const char*,actionFunction_t,int){}};
struct Actions {std::vector<Action*> a;~Actions(){for(auto*p:a)delete p;}
 void prepend(Action*p){a.insert(a.begin(),p);}int get_elements(){return a.size();}};
class ContextMenu{public:Browsable*contextable;State*state;Actions actions;Action*pickAction=NULL;
 int get_items();};
CONTEXT_ITEMS
int main(){
 BrowsableDirEntry folder,file;folder.info.attrib=AM_DIR;BrowsableDirEntry header;header.order=ET_PICKER;
 State state;TreeBrowser picker;picker.state=&state;state.browser=&picker;state.under_cursor=&folder;
 for(int key:{KEY_SEARCH,KEY_F6,KEY_TASKS,KEY_CONFIG,KEY_CTRL_O,KEY_CTRL_F,KEY_CTRL_R,KEY_CTRL_C,KEY_CTRL_V,KEY_INSERT})assert(picker.key(key)==0);
 assert(picker.contexts==0);
 assert(picker.key(KEY_RETURN)==0 && state.entered==1 && picker.contexts==0);
 state.under_cursor=&header;assert(picker.key(KEY_RETURN)==MENU_CLOSE && picker.picked);
 picker.picked=false;assert(picker.key(KEY_ESCAPE)==MENU_CLOSE && !picker.picked);
 assert(picker.key(KEY_UP)==-999 && picker.key(KEY_DOWN)==-999 && picker.key('A')==-999);
 assert(picker.can_pick(&header) && !picker.can_pick(&file) && !picker.can_pick(&folder));
 {ContextMenu c;c.state=&state;c.contextable=&file;assert(c.get_items()==0 && file.fetched==0);}
 {ContextMenu c;c.state=&state;c.contextable=&header;assert(c.get_items()==1 && header.fetched==0);}
 picker.pick_mode=TreeBrowser::PICK_LOAD;assert(picker.can_pick(&file));
 picker.pick_mode=TreeBrowser::PICK_NONE;
 {ContextMenu c;c.state=&state;c.contextable=&file;assert(c.get_items()==0 && file.fetched==1);}
 puts("picker: F6/global actions blocked; Return navigation; explicit folder confirmation; cancel; file selection and context restrictions passed");
}
'''.replace('PICKER_GUARD',guard).replace('CAN_PICK','bool TreeBrowser::can_pick(Browsable *entry) '+body(tree,'bool TreeBrowser :: can_pick(')).replace('CONTEXT_ITEMS','int ContextMenu::get_items() '+body(context,'int ContextMenu :: get_items('))
 search_code=r'''
#include <cassert>
#include <cstdio>
#define BUTTON_OK 1
class AssemblySearch;
struct Screen {void set_status(const char*,int){}};
struct UserInterface {Screen *screen;void*keyboard=NULL;int color_status=0,depth=0,popups=0;
 void popup(const char*,int){++popups;}int choice(const char*,const char**,int){return 0;}
 int activate_uiobject(AssemblySearch*);
};
struct NetworkInterface{static bool DoWeHaveLink(){return true;}};
struct BrowsableAssemblyRoot{bool isInitialized(){return true;}};
struct Gui {AssemblySearch*active_search=NULL;BrowsableAssemblyRoot root;int roots=0;
 BrowsableAssemblyRoot*getRoot(int){++roots;return &root;}
} assembly_gui;
static int live=0,draws=0;
struct AssemblySearch {UserInterface *ui;AssemblySearch(UserInterface*u,BrowsableAssemblyRoot*):ui(u){++live;}
 void setCleanup(){}void init(Screen*,void*){assert(ui->depth>0);++draws;}
 ~AssemblySearch();
};
int UserInterface::activate_uiobject(AssemblySearch*){if(depth>=7)return -1;++depth;return 0;}
int get_servers(){return 1;}const char **ServerNameList=NULL;
void open_search(UserInterface *cmd_ui) OPEN_SEARCH
AssemblySearch::~AssemblySearch() DESTRUCTOR
int main(){
 Screen screen;UserInterface ui;ui.screen=&screen;
 open_search(NULL);assert(live==0);
 // Repeated copy/picker return followed by open/close must not accumulate windows.
 for(int i=0;i<200;++i){
  open_search(&ui);assert(live==1 && ui.depth==1 && assembly_gui.active_search);
  int roots=assembly_gui.roots;open_search(&ui);assert(live==1 && ui.depth==1 && assembly_gui.roots==roots);
  delete assembly_gui.active_search;--ui.depth;assert(!assembly_gui.active_search && live==0);
 }
 int before=draws;ui.depth=7;open_search(&ui);
 assert(live==0 && !assembly_gui.active_search && draws==before && ui.depth==7);
 ui.depth=0;open_search(&ui);assert(live==1);delete assembly_gui.active_search;--ui.depth;
 puts("search: 200 open/close/reopen cycles, overlapping-window rejection, full-stack rejection before drawing and recovery passed");
}
'''.replace('OPEN_SEARCH',body(search,'static void S_OpenSearch(UserInterface *cmd_ui)')).replace('DESTRUCTOR','{ --live; '+body(searchcc,'AssemblySearch :: ~AssemblySearch()')+' }')
 with tempfile.TemporaryDirectory() as temp:
  for name,source in [('picker',picker_code),('search',search_code)]:
   p=Path(temp)/(name+'.cc');p.write_text(source);exe=Path(temp)/name
   subprocess.run(['g++','-std=c++11','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'software/io/c64'),str(p),'-o',str(exe)],check=True)
   env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
   result=subprocess.run([str(exe)],env=env,check=True,capture_output=True,text=True)
   # Production search destructor has a noisy diagnostic with no newline.
   print(result.stdout.replace('And there goes our Assembly browser!','').strip())
if __name__=='__main__':main()

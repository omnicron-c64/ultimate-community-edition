#ifndef ASSEMBLY_SEARCH_H
#define ASSEMBLY_SEARCH_H

#include "tree_browser.h"
#include "tree_browser_state.h"
#include "browsable_field.h"
#include "context_menu.h"
#include "subsys.h"
#include "json.h"
#include "menu.h"
#include "action.h"
#include "network_interface.h"
#include "assembly_server.h"

class BrowsableQueryResults;

class AssemblySearchForm: public TreeBrowserState
{
    // The results object built for the last query. The results view holds it as
    // its node but never owns it, and TreeBrowserState does not delete nodes, so
    // the form outlives the view and frees it.
    BrowsableQueryResults *results;

    void send_query(void);
public:
    AssemblySearchForm(Browsable *node, TreeBrowser *tb, int level);
    ~AssemblySearchForm();
    bool reload_on_action(void) { return false; }

    void into(void) { printf("Search Form Into\n"); }
    void level_up(void) { printf("Search Form Up\n"); };
    void change(void);
    void increase(void);
    void decrease(void);
    void clear_entry(void);
};

class AssemblyResultsView: public TreeBrowserState
{
    void get_entries(void);
public:
    AssemblyResultsView(Browsable *node, TreeBrowser *tb, int level);
    ~AssemblyResultsView();

    void into(void);
    bool into2(void) { into(); return true; }
//    void level_up(void) { printf("Results Up\n"); };

    //virtual void draw_item(Browsable *t, int line, bool selected);
    void show_status();
    virtual void move_to_index(int idx);
    virtual void draw();
};

class AssemblySearch: public TreeBrowser
{
public:
    AssemblySearch(UserInterface *ui, Browsable *);
    virtual ~AssemblySearch();

    void init(Screen *screen, Keyboard *k);
    int handle_key(int);
    void checkFileManagerEvent(void)
    {
    } // we are not listening to file manager events
};

class BrowsableAssemblyRoot: public Browsable
{
    JSON *presets;
    SearchService* server_data;

    static SubsysResultCode_e new_search(SubsysCommand *cmd);
public:
    BrowsableAssemblyRoot(SearchService* _server_data)
    {
        presets = NULL;
        server_data = _server_data;
    }

    ~BrowsableAssemblyRoot()
    {
    }

    void fetchPresets(void);
    bool isInitialized(void)
    {
        return (presets != NULL);
    }

    IndexedList<Browsable *> *getSubItems(int &error);

    void getDisplayString(char *buffer, int width, UserInterface *ui) {
        sprintf(buffer, "A64     Assembly 64 Database");
    }

	void fetch_context_items(IndexedList<Action *>&items) {
        items.append(new Action("New Search..", new_search, (int)this, 0));
    }

    const char *getName()
    {
        return (server_data) ? server_data->title.c_str() : "?";
    }
};

class BrowsableQueryResult: public Browsable
{
    mstring summary;
    mstring summary2;
    //mstring name;
    //mstring group;
    //mstring year;
    mstring id;
    mstring updated;
    int category;
    int year;
    Path path;
public:
    BrowsableQueryResult(JSON_Object *result, int window_width);

    ~BrowsableQueryResult()
    {
    }

    const char *getId() { return id.c_str(); }
    const char *getUpdated() { return updated.c_str(); }
    int getCategory() { return category; }
    int getYear() { return year; }

    const char *getName()
    {
        return summary.c_str();
    }

    Path *getPath()
    {
        return &path;
    }

    void getDisplayString(char *buffer, int width, UserInterface *ui);

    IndexedList<Browsable *> *getSubItems(int &error);

};

class BrowsableQueryResults : public Browsable // Root of results screen
{
    IndexedList<Browsable *>items; // Override from Browsable
public:
    BrowsableQueryResults(JSON_List *results, int window_width) : items(16, NULL)
    {
        for(int i=0; i<results->get_num_elements(); i++) {
            JSON *j = (*results)[i];
            if (j && j->type() == eObject) {
                JSON_Object *obj = (JSON_Object *)j;
                items.append(new BrowsableQueryResult(obj, window_width));
            }
        }
    }
    ~BrowsableQueryResults()
    {
        for (int i = 0; i < items.get_elements(); i++) {
            delete items[i];
        }
        items.clear_list();
    }

    IndexedList<Browsable *> *getSubItems(int &error)
    {
        return &items;
    }

};

class AssemblyInGui;
extern AssemblyInGui assembly_gui;

class AssemblyInGui : public ObjectWithMenu
{
    TaskCategory *taskItemCategory;
    BrowsableAssemblyRoot * root;
    int dbselection;
    static const char** ServerNameList;
    static SearchService** ServerList;
    static int server_count;

    static SubsysResultCode_e S_OpenSearch(SubsysCommand *cmd) {
        UserInterface *cmd_ui = cmd->user_interface;
        S_OpenSearch(cmd_ui);
        return SSRET_OK;
    }

    static SubsysResultCode_e S_EditServers(SubsysCommand *cmd);
    static int load_custom();
    static int get_servers();
   
public:
    AssemblySearch *active_search;
    AssemblyInGui();

    ~AssemblyInGui() {
        // unregister taskItemCategory
    }

    static void S_OpenSearch(UserInterface *cmd_ui) {
        if (!cmd_ui) return;
        if (assembly_gui.active_search) {
            cmd_ui->popup("A search is already open.\nClose it before starting another.", BUTTON_OK);
            return;
        }

        if (!NetworkInterface :: DoWeHaveLink()) {
            if (cmd_ui)
                cmd_ui->popup("No Valid Network Link", BUTTON_OK);
            return;
        }

        Screen *scr = cmd_ui->screen;


        int selection = 0;
        if (cmd_ui) {
            int count = get_servers();
            if (count < 1)
                return;
            if (count > 1)
                selection = cmd_ui->choice("Internet File Search", ServerNameList, count);
            if (selection < 0)
                return;
        }

        scr->set_status("Connecting...", cmd_ui->color_status);

        BrowsableAssemblyRoot *root = assembly_gui.getRoot(selection);
        if ((!root)||(!root->isInitialized())) {
            if (cmd_ui)
                cmd_ui->popup("Could not connect.", BUTTON_OK);
            return;
        }

        AssemblySearch *search_window = new AssemblySearch(cmd_ui, assembly_gui.getRoot(selection));
        search_window->setCleanup();
        // Claim focus before drawing, and delete rejected windows immediately.
        if (cmd_ui->activate_uiobject(search_window) < 0) {
            delete search_window;
            cmd_ui->popup("Cannot open search window.\nClose other windows and retry.", BUTTON_OK);
            return;
        }
        assembly_gui.active_search = search_window;
        search_window->init(cmd_ui->screen, cmd_ui->keyboard);
    }

    BrowsableAssemblyRoot *getRoot(int selection)
    {
        if (dbselection != selection) {
            dbselection = selection;
            delete root;
            root = NULL;
        }

        if (!root) {
            root = new BrowsableAssemblyRoot(ServerList[selection]);
        }
        if (!root->isInitialized()) {
            root->fetchPresets();
        }
        return root;
    }

    void create_task_items(void) {
        taskItemCategory->append(new Action("New Search", S_OpenSearch, 0, 0));
#if U64 == 1 && !defined(RECOVERYAPP)
        taskItemCategory->append(new Action("Edit search servers", S_EditServers, 0, 0));
#endif
    }
};

#endif

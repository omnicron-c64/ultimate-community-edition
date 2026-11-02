#include "userinterface.h"
#include <stdlib.h>
#include <string.h>
#include "mystring.h" // my string class
#include "assembly_search.h"
#include "assembly_entry.h"
#include "assembly.h"
#include "network_config.h"


#define DEFAULT_HOSTNAME_ASS  "hackerswithstyle.se"
#define DEFAULT_HOSTPORT      80
#define DEFAULT_URL_SEARCH_ASS "/leet/search/aql/0/100?query="
#define DEFAULT_URL_PATTERNS  "/leet/search/aql/presets"
#define DEFAULT_URL_ENTRIES   "/leet/search/entries"
#define DEFAULT_URL_DOWNLOAD  "/leet/search/bin"
// Keep the existing server-facing identity for compatibility.
#define DEFAULT_CLIENTID_ASS  "Spiffy"

/****************************/
/* AssemblySearch UI Object */
/****************************/
/*
 * The first screen consists of a series of query fields.
 * Each of these fields are single line, thus a short description on the left
 * with a string on the right. There are two types of entries; the string
 * edit fields, and the drop down fields. For the drop down fields, the
 * context menus are used, just like in the config browser.
 * Some unselectable lines can be used for spacing.
 * A special third type of entry exits the screen to perform the search.
 * - BrowsableQueryField, with subtype: string entry, and select (drop down)
 * 
 * The second screen shows the results of the search. This screen will be 
 * populated with the 'entries'. These entries show the name and group,
 * maybe year of the release. These entries are a special type of
 * Browsable; which holds the reference to the ID and Category number
 * as listed on the Assembly server. When entering on such entry, the
 * downloadable items are fetched and shown on the third screen.
 * 
 * - BrowsableAssemblyEntry (second screen)
 * - BrowsableAssemblyItem (third screen), which may be a derivative of
 *      BrowsableDirEntry, such that the filetypes would work. However,
 *      the file must still be downloaded to the cache, and the path
 *      reference must be set to the cache for the commands to work. 
 */


AssemblySearch :: AssemblySearch(UserInterface *ui, Browsable *root) : TreeBrowser(ui, root)
{
    setCleanup();
    replace_root_state(new AssemblySearchForm(root, this, 0));
    state->reload();
}

AssemblySearch :: ~AssemblySearch()
{
    if (assembly_gui.active_search == this) assembly_gui.active_search = NULL;
    printf("And there goes our Assembly browser!"); 
}

void AssemblySearch :: init(Screen *screen, Keyboard *k) // call on root!
{
	this->screen = screen;
	window = new Window(screen, (screen->get_size_x() - 40) >> 1, 2, 40, screen->get_size_y()-3);
	window->draw_border();
	keyb = k;
	state->do_refresh();
}

// Using the base class function deinit, which destroys the window.

static const char *queryhelp = 
        "1. Query Screen:\n"
        "\n"
		"CRSR UP/DN: Select field\n"
		"CLEAR:      Clear all fields\n"
        "DEL:        Clear selected field\n"
        "RETURN:     Field: Edit\n"
        "            Search: Send Query\n"
        "+/-:        Change preset option.\n"
        "RUN/STOP:   Close Search\n"
        "CRSR LEFT:  Close Search\n"
        "\n"
		"Quick type: Use the keyboard to type\n"
		"            directly in current\n"
        "            field.\n"
        "\n"
        "2. Query Result Screen:\n"
		"CRSR UP/DN: Select title\n"
        "RETURN/->:  View Result entries\n"
        "CRSR LEFT:  Return to Query screen\n"
        "\n"
        "3. Result Entries:\n"
        "\nWorks like any directory. Please\n"
        "note that accessing disks or files\n"
        "introduces a delay as the data needs\n"
        "to be downloaded from the internet.";

int AssemblySearch :: handle_key(int c)
{
    int ret = 0;
    
    if ((c == KEY_BREAK) || (c == KEY_ESCAPE) || (c == '`')) {
        return MENU_CLOSE; // independent of level, it closes the search.
        // if we'd have this handled by the tree browser, it would cause a HIDE instead
    }
    // For level 1 it's just like tree browser, with the exception of the return key / space
    // This could also be handled by the tree browser, if we check for context menus and do 'into' when none exist.
    if (((c == KEY_RETURN) || (c == KEY_SPACE)) && (state->level == 1)) {
        state->into();
        return 0;
    }
    if (state->level >= 1) {
        return TreeBrowser :: handle_key(c);
    }
    switch(c) {
        case KEY_F8: // exit
            ret = MENU_EXIT;
            break;
        case KEY_DOWN: // down
            state->down(1);
            break;
        case KEY_UP: // up
            state->up(1);
            break;
        case KEY_PAGEUP:
            state->up(window->get_size_y()/2);
            break;
        case KEY_PAGEDOWN:
            state->down(window->get_size_y()/2);
            break;
        case KEY_TASKS:
            ret = MENU_CLOSE; // do nothing in the non-commercial mode
            break;
        case KEY_HELP: 
            state->refresh = true;
            user_interface->run_editor(queryhelp, strlen(queryhelp));
            break;
        case KEY_CLEAR: //
            if(state->level == 0) {
                state->reload();
                state->do_refresh();
            }
            break;
        case KEY_HOME: // clear entry
            ((AssemblySearchForm *)state)->clear_entry();
            break;
        case KEY_SPACE: // space = select
        	state->select_one();
            break;
        case KEY_RETURN: // CR = select
            switch (state->level) {
            case 0:
                state->change();
                break;
            case 1:
                state->into();
                break;
            case 2:
                context(0);
                break;
            default:
                break;
            }
            break;
        case KEY_RIGHT: // right
            if(state->level!=0)
                state->into();
            break;
        case '+':
            if(state->level==0)
                state->increase();
            break;
        case '-':
            if(state->level==0)
                state->decrease();
            break;
        case KEY_LEFT: // left
		case KEY_BACK: // del
            if(state->level==0) {
                ret = MENU_CLOSE; // leave
            } else {
                state->level_up();
            }
            break;

        default:
            if ((state->level == 0) && (
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9'))) {
                keyb->push_head(c);
                state->change();
            } else {
                printf("Unhandled key: %b\n", c);
            }
    }    
    return ret;
}


AssemblySearchForm :: AssemblySearchForm(Browsable *node, TreeBrowser *tb, int level) : TreeBrowserState(node, tb, level)
{
    //default_color = 7;
    results = NULL;
}

AssemblySearchForm :: ~AssemblySearchForm()
{
    // Runs after ~TreeBrowserState has torn the results view down, because the
    // view deletes its 'previous' (this form) last.
    if (results) {
        delete results;
    }
}


void AssemblySearchForm :: send_query(void)
{
    mstring query;
    for(int i=0;i<children->get_elements();i++) {
        Browsable *b = (*children)[i];
        if (b->isSelectable()) {
            BrowsableQueryField *field = (BrowsableQueryField *)b;
            const char *name = field->getName();
            const char *value = field->getAqlString();
            if (strlen(value) == 0) {
                continue;
            }
            if (name[0] == '$') {
                continue;
            }
            if (query.length() > 0) {
                query += " & ";
            }
            query += "(";
            query += field->getName();
            query += ":";
            if (!field->isDropDown()) {
                query += "\"";
            }
            if ((strcasecmp(name, "rating") == 0) && (value[0] != '>')) {
                query += ">=";
            }
            query += value;
            if (!field->isDropDown()) {
                query += "\"";
            }
            query += ")";
        }
    }
    if (!query.length()) {
        browser->user_interface->popup("Queries cannot be empty!", BUTTON_OK);
        return;
    }
    printf("Query:\n%s\n", query.c_str());

    // Let the user know we are busy
    browser->window->getScreen()->set_status("Sending query...", browser->user_interface->color_status);

    JSON *response = assembly.send_query(query.c_str());
    // if (response) {
    //     puts(response->render());
    // }
    if (response) {
        if (response->type() == eList) {
            printf("Creating results view...\n");
            // The previous query's results view is already gone by the time the
            // form can be used again, so its results object is ours to drop.
            if (results) {
                delete results;
            }
            BrowsableQueryResults *rb = new BrowsableQueryResults((JSON_List *)response, browser->window->get_size_x());
            results = rb;
            deeper = new AssemblyResultsView(rb, browser, 1);
            deeper->previous = this;
            int error;
        	deeper->children = rb->getSubItems(error);
        	int child_count = deeper->children->get_elements();
            printf("Number of results: %d.\n", child_count);
            browser->state = deeper;
        }
        delete response;
    } else {
        browser->window->getScreen()->set_status("** Connection FAILED **", 10);
    }
}

// The form mixes unselectable spacers in with the query fields, and the cast is
// only valid for a field. send_query() guards the identical cast this way.
static BrowsableQueryField *as_query_field(Browsable *under_cursor)
{
    if (!under_cursor || !under_cursor->isSelectable()) {
        return NULL;
    }
    return (BrowsableQueryField *)under_cursor;
}

void AssemblySearchForm :: change(void)
{
    BrowsableQueryField *field = as_query_field(under_cursor);
    if (!field)
        return;

    char buffer[32];
    if ((field->getName())[0] == '$') {  // The dirtiest trick ever!
        send_query();
    } else if (field->isDropDown()) {
        browser->context(0);
        // refresh will take place, because the context menu disappears and refresh flag is set
    } else {
        // The value comes from the server, so its length is not ours to assume.
        // max_chars must be explicit too: it otherwise defaults to the window
        // width, which is unrelated to this buffer.
        const int edit_len = 26;
        strncpy(buffer, field->getStringValue(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = 0;
        browser->window->set_color(1);
        int edited = browser->user_interface->string_edit(buffer, edit_len, browser->window,
                                                          10, this->cursor_pos, edit_len);
        if (edited > 0) { // an aborted edit must not overwrite the field
            field->setStringValue(buffer);
        }
        // explicit refresh
        refresh = true;
        down(1);
    }
}

void AssemblySearchForm :: increase(void)
{
    BrowsableQueryField *field = as_query_field(under_cursor);
    if (!field)
        return;
    field->updown(1);
    update_selected();
}

void AssemblySearchForm :: decrease(void)
{
    BrowsableQueryField *field = as_query_field(under_cursor);
    if (!field)
        return;
    field->updown(-1);
    update_selected();
}

void AssemblySearchForm :: clear_entry(void)
{
    BrowsableQueryField *field = as_query_field(under_cursor);
    if (!field)
        return;
    field->reset();
    update_selected();
}

AssemblyResultsView :: AssemblyResultsView(Browsable *node, TreeBrowser *tb, int level) : TreeBrowserState(node, tb, level)
{
    //default_color = 7;
}

AssemblyResultsView :: ~AssemblyResultsView()
{
}

void AssemblyResultsView :: into()
{
    BrowsableQueryResult *item = (BrowsableQueryResult *)under_cursor;
    if (!item)
        return;

    deeper = new TreeBrowserState(item, browser, 2);
    deeper->previous = this;
    int error;
    deeper->children = item->getSubItems(error);
    browser->state = deeper;
    char cat[16];
    browser->path->cd("/a64");
    browser->path->cd(item->getId());
    sprintf(cat, "%d", item->getCategory());
    browser->path->cd(cat);
    int child_count = deeper->children->get_elements();
    printf("Number of entries: %d. Path = %s\n", child_count, browser->getPath());
}

void AssemblyResultsView :: get_entries()
{

}

void AssemblyResultsView :: move_to_index(int idx)
{
    TreeBrowserState::move_to_index(idx);
    show_status();
}

void AssemblyResultsView :: draw()
{
    TreeBrowserState::draw();
    show_status();
}

void AssemblyResultsView :: show_status()
{
    BrowsableQueryResult *item = (BrowsableQueryResult *)under_cursor;
    if (!item)
        return;
    mstring infostr;
    infostr += item->getUpdated();
    infostr += " ";
    int y = item->getYear();
    if ((y>=1000)&&(y<=9999))
        infostr += y;
    else
        infostr += "    ";
    infostr += " cat:";
    infostr += item->getCategory();
    browser->window->getScreen()->set_status(infostr.c_str(), browser->user_interface->color_status);
}

void BrowsableAssemblyRoot :: fetchPresets(void)
{
    presets = assembly.get_presets(server_data);
}

IndexedList<Browsable *> *BrowsableAssemblyRoot::getSubItems(int &error)
{
    // name, group, handle, event, date*, category*, subcat*, rating*, type*, repo*, latest, sort, order
    if (children.get_elements() == 0) {
            children.append(new BrowsableStatic(server_data->title.c_str()));
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableQueryField("name", NULL));
            children.append(new BrowsableQueryField("group", NULL));
            children.append(new BrowsableQueryField("handle", NULL));
            children.append(new BrowsableQueryField("event", NULL));
            if (presets && (presets->type() == eList)) {
                JSON_List *list = (JSON_List *)presets;
                for (int i = 0; i < list->get_num_elements(); i++) {
                    JSON_Object *obj = (JSON_Object *)(*list)[i];
                    JSON *type = obj->get("type");
                    JSON *values = obj->get("values");
                    if (type && type->type() == eString && values && values->type() == eList) {
                        children.append(new BrowsableQueryField(((JSON_String *)type)->get_string(), (JSON_List *)values));
                    }
                }
            }
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableQueryField("$", NULL));

          if (children.get_elements() < 16) {
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableStatic("You agree you have a necessary license"));
            children.append(new BrowsableStatic("or rights to download any software."));
          }
    }
    return &children;
}

#include "browsable_root.h"
IndexedList<Browsable *> *BrowsableQueryResult :: getSubItems(int &error)
{
    // name, group, handle, event, date*, category*, subcat*, rating*, type*, repo*, latest, sort, order
    error = 0;
    if (children.get_elements() == 0) {
        JSON *j = assembly.request_entries(id.c_str(), category);
        if (j && j->type() == eObject) {
            JSON *content = ((JSON_Object *)j)->get("contentEntry");
            if (content && content->type() == eList) {
                JSON_List *list = (JSON_List *)content;
                for(int i=0; i < list->get_num_elements(); i++) {
                    JSON *el = (*list)[i];
                    if (el->type() == eObject) {
                        children.append(new BrowsableDirEntryAssembly(this, (JSON_Object *)el, id.c_str(), category));
                    }
                }
            }
        } else {
            error = 1;
        }
        // Each entry copied what it needs out of the tree, so nothing below
        // points into it once the loop is done.
        if (j) {
            delete j;
        }
    }
    return &children;
}

BrowsableQueryResult :: BrowsableQueryResult(JSON_Object *result, int window_width) : path("/a64")
{
    JSON *j;
    year = 0;

    j = result->get("name");
    if (j && j->type() == eString) {
        summary = ((JSON_String *)j)->get_string();
    }
    j = result->get("year");
    if (j && j->type() == eInteger) {
        year = ((JSON_Integer *)j)->get_value();
    }
    j = result->get("group");
    if (j && j->type() == eString) {
        summary += " ";
        summary2 += "\e\x0d";
        summary2 += ((JSON_String *)j)->get_string();
        int l = summary.length()+summary2.length()-2; // subtract 2 characters for escape codes
        if (year && (l+5<=window_width)) {
            summary2 += ",";
            summary2 += year;
        }
        l = summary.length()+summary2.length()-2; // subtract 2 characters for escape codes
        while (l++ < window_width)
            summary += " ";
    }
    category = 0;
    j = result->get("category");
    if (j && j->type() == eInteger) {
        category = ((JSON_Integer *)j)->get_value();
    }
    j = result->get("id");
    if (j && j->type() == eString) {
        id = ((JSON_String *)j)->get_string();
    }
    j = result->get("updated");
    if (j && j->type() == eString) {
        updated = ((JSON_String *)j)->get_string();
    }
    path.cd(id.c_str());
    char catstr[8];
    sprintf(catstr, "%d", category);
    path.cd(catstr);
}

void BrowsableQueryResult :: getDisplayString(char *buffer, int width, UserInterface *ui)
{
    bool extra = (summary.length()<width);
    if (extra)
        width += 2;
    memset(buffer, ' ', width);
    buffer[width] = '\0';

    strncpy(buffer, summary.c_str(), width);
    if (extra)
    {
        strncpy(&buffer[summary.length()], summary2.c_str(), width-summary.length());
    }
}

AssemblyInGui::AssemblyInGui() {
    active_search = NULL;
    root = NULL;
    dbselection = 0;
    server_count = 0;

    taskItemCategory = TasksCollection :: getCategory("Assembly 64", SORT_ORDER_ASSEMBLY);
}

#define JSON_GET(j, name) \
    ((j && (j->type() == eObject)) ? ((JSON_Object*) j)->get(name) : NULL)

#define JSON_GET_STRING(obj, name, s) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eString)) \
        s = ((JSON_String*) j)->get_string();

#define JSON_GET_INTEGER(obj, name, v) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eInteger)) \
        v = ((JSON_Integer*) j)->get_value();

int AssemblyInGui::load_custom()
{
#ifndef RECOVERYAPP
    const char* branding_directory = "/Flash/config";

    ConfigManager *cm = ConfigManager :: getConfigManager();
    if (cm->get_safe_mode())
        return 0;

    FileManager *fm = FileManager::getFileManager();
    size_t bufferSize = 4096;
    char* jsonText = (char*) malloc(bufferSize);
    if (!jsonText)
        return 0;

    uint32_t jsonTextSize=0;
    JSON *obj = NULL;
    FRESULT fres = fm->load_file(branding_directory, "server.json", (uint8_t*) jsonText, bufferSize, &jsonTextSize);

    if ((fres != 0)||(jsonTextSize == 0)) {
        free(jsonText);
        return 0;
    }

    server_count = 0;
    printf("Loading server.json...\n");
    convert_text_to_json_objects(jsonText, (size_t) jsonTextSize, 512, &obj);
    do {
        if (!obj) {
            printf("Invalid JSON data!\n");
            break;
        }

        printf("Evaluating server.json file...\n");
        JSON* j = JSON_GET(obj, "assembly64");
        JSON_List* l = (j && (j->type() == eList)) ? (JSON_List*) j : NULL;
        if (!l)
            break;

        int max_count = l->get_num_elements();
        if (max_count < 1)
            break;
        if (max_count > 8)
            max_count = 8;

        ServerList = new SearchService*[max_count];
        if (!ServerList)
            break;

        for (int i=0;i < max_count;i++) {
            JSON* e = (*l)[i];
            if (e && (e->type() == eObject)) {
                // set useful defaults
                const char* name      = "no name";
                const char* host      = DEFAULT_HOSTNAME_ASS;
                int         port      = DEFAULT_HOSTPORT;
                const char* client_id = DEFAULT_CLIENTID_ASS;
                const char* usearch   = DEFAULT_URL_SEARCH_ASS;
                const char* upatterns = DEFAULT_URL_PATTERNS;
                const char* uentries  = DEFAULT_URL_ENTRIES;
                const char* udownload = DEFAULT_URL_DOWNLOAD;

                JSON_GET_STRING(e, "name",         name);
                JSON_GET_STRING(e, "host",         host);
                JSON_GET_INTEGER(e,"port",         port);
                JSON_GET_STRING(e, "client-id",    client_id);
                JSON_GET_STRING(e, "url-search",   usearch);
                JSON_GET_STRING(e, "url-patterns", upatterns);
                JSON_GET_STRING(e, "url-entries",  uentries);
                JSON_GET_STRING(e, "url-download", udownload);

                if (port < 1 || port > 65535 || !host[0] || strchr(host, '\r') || strchr(host, '\n')) continue;
                ServerList[server_count++] = new SearchService(
                    name, host, port, client_id,
                    usearch, upatterns, uentries, udownload);
            }
        }
    } while (0);

    delete obj;
    free(jsonText);
#endif
    return server_count;
}

int AssemblyInGui::get_servers()
{
    if ((!ServerList)||(!server_count))
        server_count = load_custom();

    if ((!ServerList)||(!server_count)) {
        delete [] ServerList;
        ServerList = new SearchService*[1];
        server_count = 0;
        ServerList[server_count++] =
            new SearchService("Assembly 64",
                              DEFAULT_HOSTNAME_ASS, DEFAULT_HOSTPORT,
                              DEFAULT_CLIENTID_ASS,
                              DEFAULT_URL_SEARCH_ASS,
                              DEFAULT_URL_PATTERNS,
                              DEFAULT_URL_ENTRIES,
                              DEFAULT_URL_DOWNLOAD);
    }

    if (!ServerNameList) {
        printf("search service server:\n");
        ServerNameList = new const char*[server_count+1];
        for (int i=0;i<server_count;i++) {
            ServerNameList[i] = ServerList[i]->name;
            printf("server %i: %s\n", i, ServerNameList[i]);
        }
    }

    return server_count;
}

const char** AssemblyInGui::ServerNameList = NULL;
SearchService** AssemblyInGui::ServerList = NULL;
int AssemblyInGui::server_count = 0;

AssemblyInGui assembly_gui;

#if U64 == 1 && !defined(RECOVERYAPP)
#include "server_editor_json.h"

static JSON_Object *server_defaults(const char *name, const char *host, int port)
{
    return JSON::Obj()->add("name", name)->add("host", host)->add("port", port)
        ->add("client-id", DEFAULT_CLIENTID_ASS)->add("url-search", DEFAULT_URL_SEARCH_ASS)
        ->add("url-patterns", DEFAULT_URL_PATTERNS)->add("url-entries", DEFAULT_URL_ENTRIES)
        ->add("url-download", DEFAULT_URL_DOWNLOAD);
}
static bool edit_server(UserInterface *ui, JSON_Object *server)
{
    const char *labels[] = {"Name", "Host (DNS / IPv4, no http://)", "Port", "Client ID", "Search URL", "Presets URL", "Entries URL", "Download URL", "Done"};
    const char *keys[] = {"name", "host", "port", "client-id", "url-search", "url-patterns", "url-entries", "url-download"};
    const char *defaults[] = {"Local files", "192.168.1.100", "8000", DEFAULT_CLIENTID_ASS, DEFAULT_URL_SEARCH_ASS, DEFAULT_URL_PATTERNS, DEFAULT_URL_ENTRIES, DEFAULT_URL_DOWNLOAD};
    for (;;) {
        int field = ui->choice("Edit search server", labels, 9);
        if (field < 0 || field == 8) return true;
        char value[257];
        if (field == 2) snprintf(value, sizeof(value), "%d", server->int_or("port", 80));
        else {
            const char *current = server->string_or(keys[field], defaults[field]);
            if (strlen(current) > 256) { ui->popup("Field exceeds editor limit.\nEdit this field in server.json.", BUTTON_OK); continue; }
            strcpy(value, current);
        }
        if (ui->string_box(labels[field], value, field == 2 ? 5 : field == 0 ? 32 : field == 1 ? 127 : field == 3 ? 32 : 256) < 0) continue;
        bool valid = field == 1 ? server_editor_host_valid(value) :
            field == 2 ? server_editor_port(value) > 0 :
            field >= 4 ? server_editor_path_valid(value) : server_editor_header_valid(value);
        if (!valid) { ui->popup("Invalid value.\nHost: DNS name or IPv4 only.\nPort: 1-65535. URLs start /.", BUTTON_OK); continue; }
        if (field == 2) server->set(keys[field], server_editor_port(value));
        else server->set(keys[field], value);
    }
}
#include "server_editor_file.h"

SubsysResultCode_e AssemblyInGui::S_EditServers(SubsysCommand *cmd)
{
    UserInterface *ui = cmd->user_interface;
    if (!ui) return SSRET_NO_USER_INTERFACE;
    if (ConfigManager::getConfigManager()->get_safe_mode()) {
        ui->popup("Server editing is disabled\nin safe mode.", BUTTON_OK); return SSRET_OK;
    }
    FileManager *fm = FileManager::getFileManager();
    File *file = NULL; JSON *document = NULL;
    FRESULT result = fm->fopen("/Flash/config/server.json", FA_READ, &file);
    if (result == FR_OK) {
        uint32_t length = file->get_size(), read = 0;
        char *text = new char[4097];
        bool valid = length > 0 && length < 4096;
        if (valid) valid = file->read(text, length, &read) == FR_OK && read == length;
        fm->fclose(file);
        if (valid) { text[length] = 0; valid = server_editor_input_supported(text, length) && convert_text_to_json_objects(text, length, 512, &document) > 0; }
        if (valid) valid = server_editor_decode_tree(document);
        delete[] text;
        if (!valid || !document || document->type() != eObject) {
            delete document; ui->popup("Cannot edit server.json.\nInvalid JSON or file too large.\nExisting file was kept.", BUTTON_OK); return SSRET_OK;
        }
    } else if (result == FR_NO_FILE || result == FR_NO_PATH) {
        document = JSON::Obj()->add("assembly64", JSON::List()->add(server_defaults("Assembly64", DEFAULT_HOSTNAME_ASS, 80)));
    } else {
        ui->popup("Cannot read server.json.\nExisting file was kept.", BUTTON_OK); return SSRET_OK;
    }
    JSON *list = ((JSON_Object *)document)->get("assembly64");
    if (!list || list->type() != eList || ((JSON_List *)list)->get_num_elements() > 8) {
        delete document; ui->popup("Expected assembly64 list\nwith at most 8 servers.\nExisting file was kept.", BUTTON_OK); return SSRET_OK;
    }
    JSON_List *servers = (JSON_List *)list;
    bool valid = true;
    for (int i = 0; i < servers->get_num_elements(); ++i) if ((*servers)[i]->type() != eObject) valid = false;
    if (!valid) { delete document; ui->popup("Invalid server entry.\nExisting file was kept.", BUTTON_OK); return SSRET_OK; }
    const char *actions[] = {"Add local server", "Edit server", "Delete server", "Save and exit", "Cancel"};
    for (;;) {
        int action = ui->choice("Assembly64 server editor", actions, 5);
        if (action < 0 || action == 4) {
            if (ui->popup("Discard editor changes?", BUTTON_OK | BUTTON_CANCEL) == BUTTON_OK) break;
            continue;
        }
        int count = servers->get_num_elements();
        if (action == 0) {
            if (count == 8) { ui->popup("Maximum 8 servers.", BUTTON_OK); continue; }
            JSON_Object *entry = server_defaults("Local files", "192.168.1.100", 8000);
            edit_server(ui, entry); servers->add(entry);
        } else if (action == 1 || action == 2) {
            if (!count) { ui->popup("No servers. Add one first.", BUTTON_OK); continue; }
            const char *names[8]; char display[8][33];
            for (int i = 0; i < count; ++i) {
                const char *name = ((JSON_Object *)(*servers)[i])->string_or("name", "Unnamed server");
                int j = 0;
                for (; j < 32 && name[j]; ++j) { unsigned char c = name[j]; display[i][j] = c < 32 || c >= 127 ? '?' : c; }
                display[i][j] = 0; names[i] = display[i];
            }
            int selected = ui->choice("Choose server", names, count);
            if (selected < 0) continue;
            JSON_Object *entry = (JSON_Object *)(*servers)[selected];
            if (action == 1) edit_server(ui, entry);
            else if (ui->popup("Delete this server?", BUTTON_OK | BUTTON_CANCEL) == BUTTON_OK) { servers->remove(entry); delete entry; }
        } else if (action == 3) {
            if (!count) { ui->popup("Keep at least one server.", BUTTON_OK); continue; }
            bool all_valid = true;
            for (int i = 0; i < count; ++i) if (!server_editor_entry_valid((JSON_Object *)(*servers)[i])) all_valid = false;
            if (!all_valid) { ui->popup("Invalid server settings.\nEdit host, port and URL fields\nbefore saving.", BUTTON_OK); continue; }
            result = save_servers(document);
            ui->popup(result == FR_OK ? "Servers saved.\nReboot to load the changes.\nPrevious file: server.json.bak" : "Could not save servers.\nCheck Flash space / permissions.\nInspect server.json and .bak.", BUTTON_OK);
            if (result == FR_OK) break;
        }
    }
    delete document;
    return SSRET_OK;
}
#endif

SubsysResultCode_e BrowsableAssemblyRoot::new_search(SubsysCommand *cmd)
{
    if (!cmd->user_interface) return SSRET_NO_USER_INTERFACE;
    AssemblyInGui::S_OpenSearch(cmd->user_interface);
    return SSRET_OK;
}

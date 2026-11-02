/*
 * filesystem_ftp.cc
 *
 * FTP remote filesystem — mounts an FTP server as a browsable
 * directory in the file tree. Files are downloaded to /Temp/ on
 * each open into an isolated temporary file.
 */

#include "filesystem_ftp.h"
#include "filemanager.h"
#include "node_directfs.h"
#include "network_config.h"
#include "init_function.h"
#include "json.h"
#include "server_editor_json.h"
#include "userinterface.h"
extern "C" {
void url_decode(char *src, char *dest, int max);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************************************************************/
/* FileSystemFTP                                             */
/*************************************************************/
struct FTPExists { const char *name; bool found; bool unsupported; };
uint32_t FileOnFTP::node_count = 0;

FileSystemFTP::FileSystemFTP(FTPServer *s) : FileSystem(0), client(NULL), connected(false), server(s)
{
    client = NULL;
}

FileSystemFTP::~FileSystemFTP()
{
    if (client) {
        client->disconnect();
        delete client;
    }
}

int FileSystemFTP::connect_if_needed()
{
    if (!client) {
        client = new FTPClient();
    }
    if (connected && client->is_connected()) {
        return 0;
    }
    connected = false;



    if (client->open(server->host.c_str(), server->port) < 0) {
        printf("[FTP-FS] Connection failed\n");
        return -1;
    }

    if (client->login(server->user.c_str(), server->passw.c_str()) < 0) {
        printf("[FTP-FS] Login failed\n");
        client->disconnect();
        return -1;
    }

    if (client->type_binary() < 0) { client->disconnect(); return -1; }

    connected = true;
    printf("[FTP-FS] Connected OK\n");
    return 0;
}

void FileSystemFTP::drop_connection()
{
    if (client) {
        client->disconnect();
    }
    connected = false;
}

#include "ftp_read_stream.h"
#include "ftp_file_transfer.h"

FRESULT FileSystemFTP::dir_open(const char *path, Directory **dir)
{
    printf("[FTP-FS] dir_open('%s')\n", path ? path : "(null)");

	DirectoryOnFTP *newdir = new DirectoryOnFTP(this);
	if (!path) {
	    path = "/";
	}
    mstring full_path;
    build_ftp_path(path, full_path);
	FRESULT res = newdir->list(full_path.c_str());

	if (res == FR_OK) {
		*dir = newdir;
	} else {
		delete newdir;
	}
	return res;
}

/*************************************************************/
/* DirectoryOnFTP                                            */
/*************************************************************/

FRESULT DirectoryOnFTP::get_entry(FileInfo &info)
{
    if (index >= children.get_elements()) {
        return FR_NO_FILE;
    }

    FileInfo *child = children[index++];
    info.copyfrom(child);
    return FR_OK;
}

/********************/
/* FTP LIST Parse   */
/********************/

#include "ftp_listing_parser.h"

bool DirectoryOnFTP::accept_line(void *context, const char *line, bool machine)
{
    DirectoryOnFTP *self = (DirectoryOnFTP *)context;
    char name[512]; uint32_t size; bool dir;
    if (!ftp_entry(line, machine, name, sizeof(name), &size, &dir)) return false;
    if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return true;
    if (self->children.get_elements() >= 4096) return false;
    FileInfo *info = new FileInfo(name);
    info->attrib = dir ? AM_DIR : 0; info->size = size; info->fs = self->ftpfs;
    get_extension(name, info->extension, true); self->children.append(info); return true;
}
FRESULT DirectoryOnFTP::list(const char *path)
{
    if (ftpfs->connect_if_needed() < 0) return FR_NOT_READY;
    if (ftpfs->get_client()->list_lines(path, accept_line, this) < 0) {
        UserInterface::postMessage(ftpfs->get_client()->last_error());
        return FR_DISK_ERR;
    }
    return FR_OK;
}

/*************************************************************/
/* Writable filesystem operations                            */
/*************************************************************/

// Build the full FTP server path from a local filesystem path.
// Local paths look like "subdir/file.d64"; we prepend the root's base path.
void FileSystemFTP::build_ftp_path(const char *local_path, mstring &out)
{
    out = server->folder.c_str();
    if (out.length() > 0 && out.c_str()[out.length() - 1] != '/') {
        out += "/";
    }
    if (local_path && local_path[0] == '/') {
        local_path++;
    }
    if (local_path) {
        out += local_path;
    }
}

FRESULT FileSystemFTP::dir_create(const char *path)
{
    printf("[FTP-FS] dir_create('%s')\n", path);

    if (connect_if_needed() < 0) {
        return FR_DISK_ERR;
    }

    mstring ftp_path;
    build_ftp_path(path, ftp_path);

    if (client->mkd(ftp_path.c_str()) < 0) {
        printf("[FTP-FS] MKD '%s' failed\n", ftp_path.c_str());
        return FR_DENIED;
    }

    return FR_OK;
}

FRESULT FileSystemFTP::file_delete(const char *path)
{
    printf("[FTP-FS] file_delete('%s')\n", path);

    if (connect_if_needed() < 0) {
        return FR_DISK_ERR;
    }

    mstring ftp_path;
    build_ftp_path(path, ftp_path);

    // Try file delete first, then directory delete
    if (client->dele(ftp_path.c_str()) < 0) {
        if (client->rmd(ftp_path.c_str()) < 0) {
            printf("[FTP-FS] DELE/RMD '%s' failed\n", ftp_path.c_str());
            return FR_DENIED;
        }
    }

    return FR_OK;
}

FRESULT FileSystemFTP::file_rename(const char *old_name, const char *new_name)
{
    printf("[FTP-FS] file_rename('%s' -> '%s')\n", old_name, new_name);

    if (connect_if_needed() < 0) {
        return FR_DISK_ERR;
    }

    mstring old_ftp, new_ftp;
    build_ftp_path(old_name, old_ftp);
    build_ftp_path(new_name, new_ftp);

    if (client->rnfr_rnto(old_ftp.c_str(), new_ftp.c_str()) < 0) {
        printf("[FTP-FS] RNFR/RNTO failed\n");
        return FR_DENIED;
    }
    return FR_OK;
}

/*************************************************************/
/* Registration                                              */
/*************************************************************/

FTPRootNode *ftp_root = NULL;

void add_ftp_to_root(void *_context, void *_param)
{
    ftp_root = new FTPRootNode();
    ftp_root->load_servers();
    FileManager::getFileManager()->add_root_entry(ftp_root);
    printf("[FTP-FS] FTP root node added.\n");
}

InitFunction init_ftp_fs("FTP Filesystem", add_ftp_to_root, NULL, NULL, 31);

void FTPRootNode :: load_servers()
{
    File *fi;
    FileManager *fm = FileManager :: getFileManager();
    FRESULT fres = fm->fopen(CFG_FILEPATH, FTP_SERVERS, FA_READ, &fi);
    if (fres == FR_OK) {
        load_servers_impl(fi);
        fm->fclose(fi);
    }
}

FRESULT FTPRootNode :: save_servers()
{
    FileManager *fm = FileManager::getFileManager();
    JSON_Object *root = JSON::Obj(); JSON_List *servers = JSON::List();
    root->add("version", 1); root->add("servers", servers);
    for (int i = 0; i < children.get_elements(); ++i) {
        FTPServer *s = (FTPServer *)children[i]; mstring encoded;
        url_encode(s->passw.c_str(), encoded);
        servers->add(JSON::Obj()->add("alias", s->alias.c_str())->add("host", s->host.c_str())
            ->add("port", (int)s->port)->add("user", s->user.c_str())
            ->add("password", encoded.c_str())->add("folder", s->folder.c_str()));
    }
    char *text = new char[12288]; ServerEditorJson writer(text, 12288);
    if (!writer.write(root)) { delete root; delete[] text; return FR_INVALID_PARAMETER; }
    delete root;
    FRESULT result = fm->create_dir(CFG_FILEPATH);
    if (result != FR_OK && result != FR_EXIST) { delete[] text; return result; }
    File *file = NULL;
    result = fm->fopen(CFG_FILEPATH, "ftp_servers.tmp", FA_WRITE | FA_CREATE_ALWAYS, &file);
    uint32_t written = 0; size_t size = strlen(text);
    if (result == FR_OK) {
        result = file->write(text, size, &written);
        if (result == FR_OK && written != size) result = FR_DISK_ERR;
        if (result == FR_OK) result = file->sync();
        FRESULT closed = fm->fclose(file); if (result == FR_OK) result = closed;
    }
    delete[] text;
    if (result != FR_OK) return result;
    mstring original(CFG_FILEPATH "/" FTP_SERVERS), backup(CFG_FILEPATH "/ftp_servers.bak");
    result = fm->delete_file(backup.c_str());
    if (result != FR_OK && result != FR_NO_FILE) return result;
    result = fm->rename(original.c_str(), backup.c_str());
    bool existed = result == FR_OK;
    if (result != FR_OK && result != FR_NO_FILE) return result;
    result = fm->rename(CFG_FILEPATH "/ftp_servers.tmp", original.c_str());
    if (result != FR_OK && existed) fm->rename(backup.c_str(), original.c_str());
    return result;
}

void FTPRootNode :: load_servers_impl(File *f)
{
    uint32_t size = f->get_size();
    if ((size > 12288) || (size < 2)) { // max 12K FTP server config file
        return;
    }
    char *buffer = new char[size+1];

    uint32_t transferred;
    if (f->read(buffer, size, &transferred) != FR_OK || transferred != size) { delete[] buffer; return; }
    buffer[transferred] = 0;
    if (!server_editor_input_supported(buffer, transferred)) { delete[] buffer; return; }

    JSON *root_json = NULL;
    int tokens = convert_text_to_json_objects(buffer, transferred, 1024, &root_json);
    if ((tokens > 0) && root_json && (root_json->type() == eObject) && server_editor_decode_tree(root_json)) {
        JSON *servers_json = ((JSON_Object *)root_json)->get("servers");
        if (servers_json && (servers_json->type() == eList)) {
            JSON_List *servers = (JSON_List *)servers_json;
            for (int i=0; i < servers->get_num_elements(); i++) {
                JSON *entry_json = (*servers)[i];
                if (!entry_json || (entry_json->type() != eObject)) {
                    continue;
                }

                JSON_Object *entry = (JSON_Object *)entry_json;
                const char *alias = entry->string_or("alias", "");
                const char *host = entry->string_or("host", "");
                if (!alias[0] || !host[0]) {
                    continue;
                }

                int port = entry->int_or("port", 21);
                if (port < 1 || port > 65535 || !server_editor_host_valid(host) || strpbrk(alias, "/*?\\:\r\n")) continue;
                char portstr[8];
                sprintf(portstr, "%d", port);

                const char *user = entry->string_or("user", "anonymous");
                const char *encoded_password = entry->string_or("password", "");
                const char *folder = entry->string_or("folder", "/");
                if (folder[0] != '/' || strchr(folder, '\r') || strchr(folder, '\n') ||
                    strchr(user, '\r') || strchr(user, '\n')) continue;
                char *password = new char[1 + strlen(encoded_password)];
                strcpy(password, encoded_password);
                url_decode(password, password, 0);

                children.append(new FTPServer(this, alias, host, portstr, user, password, folder));
                delete[] password;
            }
        }
    }
    delete root_json;
    delete[] buffer;
}

IndexedList<Browsable *> *BrowsableFTPRoot :: getSubItems(int &error)
{
    for(int i=0; i < node->children.get_elements(); i++) {
        FTPServer *serv = (FTPServer *)node->children[i];
        FileInfo *inf = new FileInfo(serv->alias.c_str());
        inf->attrib = AM_DIR;
        children.append(new BrowsableFTPServer(serv, path, this, inf));
    }
    error = 0;
    return &children;
}

static bool ftp_settings_valid(UserInterface *ui, JSON_Object *fields, FTPServer *except = NULL)
{
    const char *alias = fields->string_or("Alias", "");
    const char *host = fields->string_or("Host", "");
    const char *port = fields->string_or("Port", "");
    const char *path = fields->string_or("Path", "");
    bool valid = *alias && strlen(alias) <= 63 && !strpbrk(alias, "/*?\\:\r\n") &&
        server_editor_host_valid(host) && server_editor_port(port) > 0 &&
        path[0] == '/' && !strchr(path, '\r') && !strchr(path, '\n');
    for (const char *p = alias; *p; ++p) if ((unsigned char)*p < 32 || (unsigned char)*p >= 127) valid = false;
    const char *user = fields->string_or("User", ""); const char *pass = fields->string_or("Password", "");
    if (!*user || strchr(user, '\r') || strchr(user, '\n') || strchr(pass, '\r') || strchr(pass, '\n')) valid = false;
    for (int i = 0; i < ftp_root->children.get_elements(); ++i) {
        FTPServer *s = (FTPServer *)ftp_root->children[i];
        if (s != except && !strcasecmp(s->alias.c_str(), alias)) valid = false;
    }
    if (!valid) ui->popup("Check FTP settings.\nUnique alias; DNS/IPv4 host;\nport 1-65535; path starts /.\nPlain FTP only (not SFTP/FTPS).", BUTTON_OK);
    return valid;
}
static void ftp_save_result(UserInterface *ui)
{
    if (ftp_root->save_servers() != FR_OK)
        ui->popup("FTP settings active but NOT saved.\nCheck Flash space / permissions.\nPrevious file or .bak retained.", BUTTON_OK);
}
static void ftp_progress(void *context, const char *stage, uint32_t bytes)
{
    char message[80]; snprintf(message, sizeof(message), "%s: %u bytes", stage, (unsigned)bytes);
    ((UserInterface *)context)->update_progress(message, 0);
}
static bool ftp_test_line(void *, const char *line, bool machine) {
    char name[512]; uint32_t size; bool dir;
    return ftp_entry(line, machine, name, sizeof(name), &size, &dir);
}
SubsysResultCode_e BrowsableFTPServer::S_connection(SubsysCommand *cmd)
{
    UserInterface *ui = cmd->user_interface; if (!ui) return SSRET_NO_USER_INTERFACE;
    BrowsableFTPServer *bs = (BrowsableFTPServer *)cmd->direct_obj;
    FTPServer *server = (FTPServer *)ftp_root->find_child(bs->getName());
    if (!server) return SSRET_INTERNAL_ERROR;
    FileSystemFTP *fs = (FileSystemFTP *)server->get_file_info()->fs;
    if (!fs) return SSRET_INTERNAL_ERROR;
    if (cmd->functionID == 2 || cmd->functionID == 4) {
        if (cmd->functionID == 2) fs->drop_connection();
        mstring refresh_path;
        char full_path[128]; snprintf(full_path, sizeof(full_path), "/ftp/%s", server->alias.c_str());
        FileManager::getFileManager()->sendEventToObservers(eRefreshDirectory, full_path, "");
        FileManager::getFileManager()->sendEventToObservers(eRefreshDirectory, "/ftp", "");
        ui->popup(cmd->functionID == 2 ? "FTP disconnected.\nEnter or Reconnect to connect." : "Directory refresh requested.\nRe-enter the server to reload.", BUTTON_OK);
        return SSRET_OK;
    }
    FTPClient *client = fs->get_client();
    if (cmd->functionID == 3) {
        char message[256];
        snprintf(message, sizeof(message), "%s:%u\n%s\nBytes: %u\n%s", server->host.c_str(), server->port,
            client ? client->last_stage() : "Not connected", client ? (unsigned)client->bytes_transferred() : 0,
            client && client->last_error()[0] ? client->last_error() : "No recorded error");
        ui->popup(message, BUTTON_OK); return SSRET_OK;
    }
    // Test always establishes a fresh control/login/data sequence.
    fs->drop_connection(); ui->show_progress("Testing FTP...", 1);
    // connect_if_needed creates and owns the client. Attach observer before open.
    if (client) client->set_progress(ftp_progress, ui);
    int result = fs->connect_if_needed();
    if (!result) result = client->list_lines(server->folder.c_str(), ftp_test_line, NULL);
    char message[256];
    if (!result) snprintf(message, sizeof(message), "FTP test passed.\n%s:%u\nLogin and directory data OK.\nListing: %u bytes.", server->host.c_str(), server->port, (unsigned)client->bytes_transferred());
    else snprintf(message, sizeof(message), "FTP test failed.\n%s:%u\n%s", server->host.c_str(), server->port, client ? client->last_error() : "No client available");
    if (client) client->set_progress(NULL, NULL);
    ui->hide_progress(); ui->popup(message, BUTTON_OK); return SSRET_OK;
}

static bool ftp_edit_settings(UserInterface *ui, JSON_Object *fields, FTPServer *except)
{
    const char *keys[] = {"Alias", "Host", "Port", "User", "Password", "Path"};
    const char *prompts[] = {"Server name", "Host (DNS / IPv4, no ftp://)", "Port (usually 21)", "User (anonymous if public)", "New password (blank clears)", "Starting directory (usually /)"};
    for (;;) {
        char labels[8][72]; const char *choices[8];
        for (int i = 0; i < 6; ++i) {
            const char *value = fields->string_or(keys[i], "");
            // The firmware formatter has no printf string-precision support.
            char preview[49];
            const char *display = i == 4 ? (*value ? "[set]" : "[empty]") : value;
            strncpy(preview, display, sizeof(preview)-1);
            preview[sizeof(preview)-1] = 0;
            snprintf(labels[i], sizeof(labels[i]), "%s: %s", keys[i], preview);
            choices[i] = labels[i];
        }
        choices[6] = "Save server"; choices[7] = "Cancel";
        int selected = ui->choice("FTP server (plain FTP)", choices, 8);
        if (selected < 0 || selected == 7) return false;
        if (selected == 6) { if (ftp_settings_valid(ui, fields, except)) return true; continue; }
        char buffer[256];
        const char *old = fields->string_or(keys[selected], "");
        if (strlen(old) > 255) { ui->popup("Field too long for editor.", BUTTON_OK); continue; }
        strcpy(buffer, selected == 4 ? "" : old);
        int limit = selected == 2 ? 5 : selected == 0 ? 63 : 255;
        if (ui->string_box(prompts[selected], buffer, limit) >= 0) fields->set(keys[selected], buffer);
    }
}
SubsysResultCode_e BrowsableFTPRoot::S_new_host(SubsysCommand *cmd)
{
    UserInterface *ui = cmd->user_interface;
    if (!ui) return SSRET_NO_USER_INTERFACE;
    JSON_Object *fields = JSON::Obj()->add("Alias", "")->add("Host", "")->add("Port", "21")
        ->add("User", "anonymous")->add("Password", "")->add("Path", "/");
    if (ftp_edit_settings(ui, fields, NULL)) {
        FTPServer *server = new FTPServer(ftp_root, fields->string_or("Alias", ""), fields->string_or("Host", ""),
            fields->string_or("Port", "21"), fields->string_or("User", "anonymous"),
            fields->string_or("Password", ""), fields->string_or("Path", "/"));
        ftp_root->children.append(server);
        ftp_save_result(ui);
        FileManager::getFileManager()->sendEventToObservers(eRefreshDirectory, "/ftp", "");
    }
    delete fields; return SSRET_OK;
}
SubsysResultCode_e BrowsableFTPServer::S_edit(SubsysCommand *cmd)
{
    UserInterface *ui = cmd->user_interface;
    if (!ui) return SSRET_NO_USER_INTERFACE;
    BrowsableFTPServer *bs = (BrowsableFTPServer *)cmd->direct_obj;
    FTPServer *server = (FTPServer *)ftp_root->find_child(bs->getName());
    if (!server) return SSRET_INTERNAL_ERROR;
    char port[8]; snprintf(port, sizeof(port), "%u", server->port);
    JSON_Object *fields = JSON::Obj()->add("Alias", server->alias.c_str())->add("Host", server->host.c_str())
        ->add("Port", port)->add("User", server->user.c_str())->add("Password", server->passw.c_str())->add("Path", server->folder.c_str());
    if (ftp_edit_settings(ui, fields, server)) {
        FileManager::getFileManager()->invalidate(server); server->drop_connection();
        server->alias = fields->string_or("Alias", ""); server->host = fields->string_or("Host", "");
        server->port = server_editor_port(fields->string_or("Port", "21")); server->user = fields->string_or("User", "");
        server->passw = fields->string_or("Password", ""); server->folder = fields->string_or("Path", "/");
        // Keep the cached node name and its displayed alias consistent.
        FileInfo *info = server->get_file_info();
        delete[] info->lfname; info->lfsize = server->alias.length() + 1;
        info->lfname = new char[info->lfsize]; strcpy(info->lfname, server->alias.c_str());
        ftp_save_result(ui);
        FileManager::getFileManager()->sendEventToObservers(eRefreshDirectory, "/ftp", "");
    }
    delete fields; return SSRET_OK;
}

SubsysResultCode_e BrowsableFTPServer :: S_remove(SubsysCommand *cmd)
{ 
    UserInterface *cmd_ui = cmd->user_interface;
    BrowsableFTPServer *bs = (BrowsableFTPServer *)cmd->direct_obj;

    FTPServer *serv = (FTPServer *)ftp_root->find_child(bs->getName());
    if (!serv) {
        return SSRET_INTERNAL_ERROR;
    }
    if (cmd_ui->popup("Remove this FTP server?", BUTTON_OK | BUTTON_CANCEL) != BUTTON_OK) return SSRET_ABORTED_BY_USER;
    FileManager :: getFileManager()->invalidate(serv);
    // Close the live FTP control connection before dropping the node. Without
    // this, removing a server that had been entered leaks its open control
    // socket (a lwip netconn). Since the netconn pool is small and shared by
    // the http/ftp/telnet servers too, repeated add/enter/remove cycles would
    // eventually exhaust it and wedge the whole network stack.
    serv->drop_connection();
    ftp_root->children.remove(serv);
    // The list only unlinks; free the node and its filesystem (which owns the
    // FTP client) so repeated removals don't leak memory either.
    FileSystemFTP *fs = (FileSystemFTP *)serv->get_file_info()->fs;
    serv->get_file_info()->fs = NULL;
    delete fs;
    delete serv;
    ftp_save_result(cmd_ui);
    FileManager :: getFileManager() -> sendEventToObservers(eRefreshDirectory, "/ftp", "");
    return SSRET_OK;
}

FTPServer :: FTPServer(CachedTreeNode *par, const char *alias, const char *host, const char *port_str,
            const char *user, const char *passw, const char *folder) :
            alias(alias), host(host), user(user), passw(passw), folder(folder), CachedTreeNode(par, alias)
{
    port = (uint16_t)strtol(port_str ? port_str : "21", NULL, 10);
    if (port == 0) {
        port = 21;
    }
    info.fs = new FileSystemFTP(this);
    info.attrib = AM_DIR; // ;)
    info.cluster = 0;
}

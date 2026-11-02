#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H
#include <stdint.h>
#include "filemanager.h"
#define FTP_MAX_RESPONSE 512
class FTPClient {
    int ctrl_fd, response_code;
    char response[FTP_MAX_RESPONSE], error_text[160];
    uint32_t peer_ip, transferred;
    const char *stage;
    void (*progress)(void *, const char *, uint32_t);
    void *progress_context;
    void report(const char *what, uint32_t bytes = 0);
    int fail(const char *what);
    int connect_socket(int fd, uint32_t ip, uint16_t port);
    int read_response();
    int send_cmd(const char *cmd);
    int send_cmd(const char *cmd, const char *arg);
    int send_all(int fd, const void *buf, int len);
    int open_data_connection();
    int begin_listing(const char *path, bool &machine);
    int finish_transfer(int fd, bool ok);
public:
    static bool observe_transfers(void (*fn)(void *, const char *, uint32_t), void *context);
    FTPClient();
    ~FTPClient();
    int open(const char *host, uint16_t port);
    int login(const char *user, const char *pass);
    void disconnect();
    bool is_connected();
    int cwd(const char *path);
    int pwd(char *buf, int bufsize);
    int list(const char *path, char *buf, int bufsize, int *bytes_read);
    // Streaming list parser: no fixed-size whole-directory buffer.
    int list_lines(const char *path, bool (*line)(void *, const char *, bool), void *context);
    // Incremental RETR. The caller owns fd until read_end (or disconnect).
    int size(const char *path, uint32_t *bytes);
    int read_begin(const char *path);
    int read_data(int fd, void *buffer, uint32_t length);
    int read_end(int fd, bool complete);
    int retr(const char *remote_path, File *file, int *bytes_read);
    int stor(const char *remote_path, File *file);
    int dele(const char *path);
    int mkd(const char *path);
    int rmd(const char *path);
    int rnfr_rnto(const char *from, const char *to);
    int type_binary();
    const char *last_response() { return response; }
    int last_code() { return response_code; }
    const char *last_error() { return error_text; }
    const char *last_stage() { return stage; }
    uint32_t bytes_transferred() { return transferred; }
    void set_progress(void (*fn)(void *, const char *, uint32_t), void *context) { progress = fn; progress_context = context; }
};
#endif

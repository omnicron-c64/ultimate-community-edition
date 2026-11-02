#include "ftp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "netdb.h"
#if OS
#include "FreeRTOS.h"
#include "task.h"
#endif

static const int ftp_timeout_ms = 10000;
static void (*transfer_observer)(void *, const char *, uint32_t);
static void *observer_context, *observer_owner;
static void *ftp_task() {
#if OS
    return (void *)xTaskGetCurrentTaskHandle();
#else
    return (void *)1;
#endif
}
bool FTPClient::observe_transfers(void (*fn)(void *, const char *, uint32_t), void *context) {
#if OS
    ENTER_SAFE_SECTION;
#endif
    bool available = !observer_owner || observer_owner == ftp_task();
    if (available) { transfer_observer = fn; observer_context = context; observer_owner = fn ? ftp_task() : NULL; }
#if OS
    LEAVE_SAFE_SECTION;
#endif
    return available;
}
static bool safe_argument(const char *s) {
    return s && !strchr(s, '\r') && !strchr(s, '\n');
}
static void socket_timeouts(int fd) {
    struct timeval tv = {ftp_timeout_ms / 1000, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
}
FTPClient::FTPClient() : ctrl_fd(-1), response_code(0), peer_ip(0), transferred(0),
    stage("Idle"), progress(NULL), progress_context(NULL) { response[0] = error_text[0] = 0; }
FTPClient::~FTPClient() { disconnect(); }
void FTPClient::report(const char *what, uint32_t bytes) {
    stage = what; transferred = bytes;
    if (progress) progress(progress_context, what, bytes);
    else {
        void (*fn)(void *, const char *, uint32_t) = NULL; void *context = NULL;
#if OS
        ENTER_SAFE_SECTION;
#endif
        if (observer_owner == ftp_task()) { fn = transfer_observer; context = observer_context; }
#if OS
        LEAVE_SAFE_SECTION;
#endif
        if (fn) fn(context, what, bytes);
    }
}
int FTPClient::fail(const char *what) {
    if (ctrl_fd < 0 && error_text[0]) return -1;
    snprintf(error_text, sizeof(error_text), "%s: %s (FTP %d)", stage, what, response_code);
    return -1;
}
void FTPClient::disconnect() {
    // A stale socket must not add another blocking QUIT round trip.
    if (ctrl_fd >= 0) lwip_close(ctrl_fd);
    ctrl_fd = -1;
}
int FTPClient::connect_socket(int fd, uint32_t ip, uint16_t port) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons(port); address.sin_addr.s_addr = ip;
    unsigned long nonblock = 1;
    if (lwip_ioctl(fd, FIONBIO, &nonblock) < 0) return -1;
    int result = lwip_connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (result < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
        fd_set write_set; FD_ZERO(&write_set); FD_SET(fd, &write_set);
        struct timeval tv = {ftp_timeout_ms / 1000, 0};
        result = lwip_select(fd + 1, NULL, &write_set, NULL, &tv);
        int error = 0; socklen_t size = sizeof(error);
        if (result > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && !error) result = 0;
        else result = -1;
    }
    nonblock = 0;
    if (lwip_ioctl(fd, FIONBIO, &nonblock) < 0) result = -1;
    if (!result) socket_timeouts(fd);
    return result;
}
int FTPClient::read_response() {
    response_code = 0; response[0] = 0;
    int pos = 0, multiline = 0;
    uint32_t started = sys_now();
    for (unsigned total = 0; total < 65536; ++total) {
        char c;
        if ((uint32_t)(sys_now() - started) >= (uint32_t)ftp_timeout_ms || recv(ctrl_fd, &c, 1, 0) != 1) {
            fail("reply timed out / connection closed"); disconnect(); return -1;
        }
        if (pos >= FTP_MAX_RESPONSE - 1) { fail("reply line too long"); disconnect(); return -1; }
        response[pos++] = c; response[pos] = 0;
        if (c != '\n') continue;
        bool numeric = pos >= 4 && response[0] >= '1' && response[0] <= '5' &&
            response[1] >= '0' && response[1] <= '9' && response[2] >= '0' && response[2] <= '9';
        int code = numeric ? (response[0]-'0')*100 + (response[1]-'0')*10 + response[2]-'0' : 0;
        if (!multiline && numeric && response[3] == '-') multiline = code;
        else if (numeric && response[3] == ' ' && (!multiline || multiline == code)) { response_code = code; return code; }
        pos = 0;
    }
    fail("reply too long"); disconnect(); return -1;
}
int FTPClient::send_all(int fd, const void *data, int length) {
    const char *p = (const char *)data;
    while (length > 0) {
        int n = send(fd, p, length, 0);
        if (n <= 0) return -1;
        p += n; length -= n;
    }
    return 0;
}
int FTPClient::send_cmd(const char *cmd) { return send_cmd(cmd, NULL); }
int FTPClient::send_cmd(const char *cmd, const char *arg) {
    if (ctrl_fd < 0) return fail("not connected");
    response_code = 0;
    if (!safe_argument(cmd) || (arg && !safe_argument(arg))) return fail("invalid command argument");
    char text[768];
    int n = arg ? snprintf(text, sizeof(text), "%s %s\r\n", cmd, arg) : snprintf(text, sizeof(text), "%s\r\n", cmd);
    if (n < 0 || n >= (int)sizeof(text)) return fail("command too long");
    if (send_all(ctrl_fd, text, n) < 0) {
        // Never log PASS or its argument.
        fail("command send failed"); disconnect(); return -1;
    }
    return read_response();
}
int FTPClient::open(const char *host, uint16_t port) {
    disconnect(); response_code = 0; error_text[0] = 0; report("DNS lookup");
    struct hostent entry, *resolved = NULL; char buffer[1024]; int error;
    if (!safe_argument(host) || !*host || !port ||
        lwip_gethostbyname_r(host, &entry, buffer, sizeof(buffer), &resolved, &error) != 0 ||
        !resolved || resolved->h_addrtype != AF_INET || resolved->h_length != 4) return fail("hostname lookup failed");
    memcpy(&peer_ip, resolved->h_addr, 4);
    report("Control connection");
    ctrl_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctrl_fd < 0) return fail("no socket available");
    if (connect_socket(ctrl_fd, peer_ip, port) < 0) { fail("connection failed / timed out"); disconnect(); return -1; }
    report("Server greeting");
    int code = read_response();
    if (code == 120) code = read_response();
    if (code != 220) { fail("server did not accept connection"); disconnect(); return -1; }
    return 0;
}
int FTPClient::login(const char *user, const char *pass) {
    report("Login");
    int code = send_cmd("USER", user);
    if (code == 331) code = send_cmd("PASS", pass);
    if (code != 230) return fail("login rejected / failed");
    return 0;
}
bool FTPClient::is_connected() { return ctrl_fd >= 0 && pwd(NULL, 0) == 0; }
int FTPClient::cwd(const char *path) { return send_cmd("CWD", path) == 250 ? 0 : fail("directory unavailable"); }
int FTPClient::pwd(char *out, int size) {
    if (send_cmd("PWD") != 257) return fail("cannot read working directory");
    if (!size) return 0;
    const char *start = strchr(response, '"'); if (!start) return fail("invalid PWD reply");
    ++start; int used = 0;
    while (*start && used + 1 < size) {
        if (*start == '"') { if (start[1] == '"') ++start; else { out[used] = 0; return 0; } }
        out[used++] = *start++;
    }
    out[used] = 0; return fail("PWD path too long / malformed");
}
int FTPClient::type_binary() { report("Binary mode"); return send_cmd("TYPE", "I") == 200 ? 0 : fail("binary mode rejected"); }
int FTPClient::open_data_connection() {
    report("Passive data connection");
    int code = send_cmd("EPSV"); unsigned port = 0;
    if (code == 229) {
        const char *p = strchr(response, '(');
        if (!p || strlen(p) < 7 || !p[1] || p[2] != p[1] || p[3] != p[1]) return fail("invalid EPSV reply");
        char delimiter = p[1]; p += 4;
        if (*p < '0' || *p > '9') return fail("invalid EPSV port");
        while (*p >= '0' && *p <= '9') { port = port * 10 + *p++ - '0'; if (port > 65535) return fail("invalid EPSV port"); }
        if (*p != delimiter || p[1] != ')' || !port) return fail("invalid EPSV reply");
    } else {
        if (code != 500 && code != 501 && code != 502 && code != 504 && code != 522) return fail("EPSV failed");
        if (send_cmd("PASV") != 227) return fail("PASV rejected");
        const char *p = strchr(response, '('); int values[6];
        if (!p) return fail("invalid PASV reply");
        ++p;
        for (int i = 0; i < 6; ++i) {
            if (*p < '0' || *p > '9') return fail("invalid PASV reply");
            unsigned value = 0;
            while (*p >= '0' && *p <= '9') { value = value * 10 + *p++ - '0'; if (value > 255) return fail("invalid PASV reply"); }
            values[i] = value;
            if (*p++ != (i == 5 ? ')' : ',')) return fail("invalid PASV reply");
        }
        port = values[4] * 256 + values[5]; if (!port) return fail("invalid PASV port");
        // Use the established control peer, avoiding private/wrong PASV IPs and
        // server-directed connections to unrelated hosts. No third-party FTP.
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return fail("no data socket available");
    if (connect_socket(fd, peer_ip, port) < 0) { lwip_close(fd); return fail("data connection failed / timed out"); }
    return fd;
}
int FTPClient::finish_transfer(int fd, bool ok) {
    lwip_close(fd);
    if (!ok) { disconnect(); return -1; } // unread completion replies must not poison the next command
    int code = read_response();
    if (code != 226 && code != 250) { fail("transfer completion rejected"); disconnect(); return -1; }
    error_text[0] = 0;
    report("Transfer complete", transferred); return 0;
}
int FTPClient::begin_listing(const char *path, bool &machine) {
    report("Directory selection");
    // All filesystem paths are absolute; do not depend on restoring a previous CWD.
    if (path && *path && cwd(path) < 0) return -1;
    int fd = open_data_connection(); if (fd < 0) return -1;
    report("Directory listing");
    int code = send_cmd("MLSD"); machine = true;
    if (code == 500 || code == 501 || code == 502 || code == 504) {
        lwip_close(fd); fd = open_data_connection(); if (fd < 0) return -1;
        report("Directory listing"); code = send_cmd("LIST"); machine = false;
    }
    if (code != 125 && code != 150) { lwip_close(fd); return fail("listing rejected"); }
    return fd;
}
int FTPClient::list_lines(const char *path, bool (*on_line)(void *, const char *, bool), void *context) {
    bool machine; int fd = begin_listing(path, machine); if (fd < 0) return -1;
    char block[1024], line[1024]; int used = 0; bool ok = true;
    transferred = 0;
    for (;;) {
        int n = recv(fd, block, sizeof(block), 0);
        if (n == 0) break;
        if (n < 0) { fail("listing receive failed / timed out"); ok = false; break; }
        transferred += n;
        for (int i = 0; i < n; ++i) {
            if (block[i] == '\n') {
                if (used && line[used - 1] == '\r') --used;
                line[used] = 0;
                if (!on_line(context, line, machine)) { fail("listing unsupported / too large"); ok = false; break; }
                used = 0;
            } else {
                if (used >= (int)sizeof(line) - 1 || !block[i]) { fail("listing line too long / invalid"); ok = false; break; }
                line[used++] = block[i];
            }
        }
        if (!ok) break;
        report("Directory listing", transferred);
    }
    if (ok && used) { line[used] = 0; if (!on_line(context, line, machine)) { fail("listing unsupported"); ok = false; } }
    return finish_transfer(fd, ok);
}
struct ListingBuffer { char *buf; int size, used; };
static bool collect_listing(void *context, const char *line, bool) {
    ListingBuffer *b = (ListingBuffer *)context; int n = strlen(line);
    if (b->used + n + 2 >= b->size) return false;
    memcpy(b->buf + b->used, line, n); b->used += n; b->buf[b->used++] = '\n'; b->buf[b->used] = 0; return true;
}
int FTPClient::list(const char *path, char *buf, int size, int *bytes) {
    ListingBuffer b = {buf, size, 0}; if (size <= 0) return fail("invalid listing buffer"); buf[0] = 0;
    int result = list_lines(path, collect_listing, &b); *bytes = b.used; return result;
}
int FTPClient::retr(const char *path, File *file, int *bytes) {
    *bytes = 0; int fd = open_data_connection(); if (fd < 0) return -1;
    report("Downloading"); int code = send_cmd("RETR", path);
    if (code != 125 && code != 150) { lwip_close(fd); return fail("download rejected"); }
    uint8_t *buffer = new uint8_t[4096]; bool ok = true;
    if (!buffer) { fail("no transfer buffer"); return finish_transfer(fd, false); }
    for (;;) {
        int n = recv(fd, buffer, 4096, 0);
        if (!n) break;
        if (n < 0) { fail("download receive failed / timed out"); ok = false; break; }
        uint32_t written = 0;
        if (file->write(buffer, n, &written) != FR_OK || written != (uint32_t)n) { fail("local write failed / storage full"); ok = false; break; }
        *bytes += n; report("Downloading", *bytes);
    }
    delete[] buffer;
    if (ok && file->sync() != FR_OK) { fail("local flush failed"); ok = false; }
    return finish_transfer(fd, ok);
}
int FTPClient::stor(const char *path, File *file) {
    report("Preparing upload");
    if (file->sync() != FR_OK || file->seek(0) != FR_OK) return fail("local flush / rewind failed");
    int fd = open_data_connection(); if (fd < 0) return -1;
    report("Uploading"); int code = send_cmd("STOR", path);
    if (code != 125 && code != 150) { lwip_close(fd); return fail("upload rejected"); }
    uint8_t *buffer = new uint8_t[4096]; uint32_t total = 0; bool ok = true;
    if (!buffer) { fail("no transfer buffer"); return finish_transfer(fd, false); }
    for (;;) {
        uint32_t read = 0;
        if (file->read(buffer, 4096, &read) != FR_OK) { fail("local read failed"); ok = false; break; }
        if (!read) break;
        if (send_all(fd, buffer, read) < 0) { fail("upload send failed / timed out"); ok = false; break; }
        total += read; report("Uploading", total);
    }
    delete[] buffer;
    return finish_transfer(fd, ok);
}
int FTPClient::dele(const char *path) { report("Delete file"); return send_cmd("DELE", path) == 250 ? 0 : fail("delete rejected"); }
int FTPClient::mkd(const char *path) { report("Create directory"); return send_cmd("MKD", path) == 257 ? 0 : fail("mkdir rejected"); }
int FTPClient::rmd(const char *path) { report("Remove directory"); return send_cmd("RMD", path) == 250 ? 0 : fail("rmdir rejected"); }
int FTPClient::rnfr_rnto(const char *from, const char *to) {
    report("Rename"); if (send_cmd("RNFR", from) != 350) return fail("rename source rejected");
    return send_cmd("RNTO", to) == 250 ? 0 : fail("rename destination rejected");
}

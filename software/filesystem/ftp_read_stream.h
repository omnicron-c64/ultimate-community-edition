#ifndef FTP_READ_STREAM_H
#define FTP_READ_STREAM_H
// A private control/data connection per open file. Browsing, reconnect actions,
// and a second FTP source cannot consume this file's completion reply.
class FTPReadStream : public File
{
    FTPClient client;
    static uint32_t next_inode;
    uint32_t inode;
    mstring host, user, password, remote;
    uint16_t port;
    int data_fd;
    uint32_t size_bytes, position, wire_position;
    bool failed, complete, ready;

    void stop() {
        if (data_fd >= 0) client.read_end(data_fd, false);
        data_fd = -1;
        client.disconnect(); ready = false;
    }
    int connect() {
        if (client.open(host.c_str(), port) < 0 ||
            client.login(user.c_str(), password.c_str()) < 0 || client.type_binary() < 0) return -1;
        ready = true;
        return 0;
    }
    FRESULT error(const char *message = NULL) {
        UserInterface::postMessage(message ? message : client.last_error());
        failed = true;
        stop();
        return FR_DISK_ERR;
    }
public:
    FTPReadStream(FileSystem *fs, FTPServer *server, const char *path)
        : File(fs), inode(++next_inode), host(server->host.c_str()), user(server->user.c_str()),
          password(server->passw.c_str()), remote(path), port(server->port),
          data_fd(-1), size_bytes(0), position(0), wire_position(0), failed(false), complete(false), ready(false) {}
    ~FTPReadStream() { stop(); }
    // 1 means SIZE is unsupported; the caller may use the legacy cache.
    int open() {
        if (connect() < 0) { error(); return -1; }
        int result = client.size(remote.c_str(), &size_bytes);
        if (result < 0) error();
        return result;
    }
    FRESULT write(const void *, uint32_t, uint32_t *transferred) { *transferred = 0; return FR_DENIED; }
    FRESULT sync() { return failed ? FR_DISK_ERR : FR_OK; }
    FRESULT close() { FRESULT result = sync(); delete this; return result; }
    uint32_t get_inode() { return inode; }
    uint32_t get_size() { return size_bytes; }
    FRESULT seek(uint32_t offset) {
        if (failed) return FR_DISK_ERR;
        if (offset > size_bytes) return FR_INVALID_PARAMETER;
        position = offset;
        return FR_OK;
    }
    FRESULT read(void *buffer, uint32_t length, uint32_t *transferred) {
        *transferred = 0;
        if (failed) return FR_DISK_ERR;
        if (!length) return FR_OK;
        if (complete && position == size_bytes) return FR_OK;
        if (position < wire_position || complete) {
            stop(); complete = false; wire_position = 0;
        }
        if (data_fd < 0) {
            // Reconnect also makes backward seek independent of REST support.
            if (!ready && connect() < 0) return error();
            data_fd = client.read_begin(remote.c_str());
            if (data_fd < 0) return error();
        }
        // Forward seek skips through the stream with bounded memory. Backward
        // seek replays it from zero; sequential loads require only one RETR.
        uint8_t discard[512];
        while (wire_position < position) {
            uint32_t n = position - wire_position;
            if (n > sizeof(discard)) n = sizeof(discard);
            int got = client.read_data(data_fd, discard, n);
            if (got <= 0) return error(got == 0 ? "FTP download ended before the advertised file size" : NULL);
            wire_position += got;
        }
        uint32_t wanted = size_bytes - position;
        if (wanted > length) wanted = length;
        while (*transferred < wanted) {
            int got = client.read_data(data_fd, (uint8_t *)buffer + *transferred, wanted - *transferred);
            if (got <= 0) return error(got == 0 ? "FTP download ended before the advertised file size" : NULL);
            *transferred += got; position += got; wire_position += got;
        }
        if (position == size_bytes) {
            // Check EOF and the final 226/250 before reporting the last read as
            // successful. Some loaders do not propagate fclose errors.
            uint8_t extra;
            int got = client.read_data(data_fd, &extra, 1);
            if (got != 0) return error(got > 0 ? "FTP file grew during download; retry the transfer" : NULL);
            int fd = data_fd; data_fd = -1;
            if (client.read_end(fd, true) < 0) return error();
            complete = true;
        }
        return FR_OK;
    }
};

uint32_t FTPReadStream::next_inode = 0x80000000;

FRESULT FileSystemFTP::open_read_stream(const char *remote, File **file)
{
    FTPReadStream *stream = new FTPReadStream(this, server, remote);
    int result = stream->open();
    if (result != 0) {
        delete stream;
        return result > 0 ? FR_OK : FR_DISK_ERR;
    }
    *file = stream;
    return FR_OK;
}
#endif

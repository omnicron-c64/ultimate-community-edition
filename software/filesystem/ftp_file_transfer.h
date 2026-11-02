#ifndef FTP_FILE_TRANSFER_H
#define FTP_FILE_TRANSFER_H
// Included after the filesystem and file-manager declarations.
FRESULT FileOnFTP::close(void)
{
    FileManager *fm = FileManager::getFileManager();
    FRESULT ret = write_failed ? FR_DISK_ERR : FR_OK;
    if (!cached) ret = FR_INT_ERR;
    if (cached && needs_upload && !write_failed) {
        FileSystemFTP *fs = static_cast<FileSystemFTP *>(get_file_system());
        if (!fs || fs->connect_if_needed() < 0 || fs->get_client()->stor(remote_path.c_str(), cached) < 0) {
            ret = FR_DISK_ERR;
            UserInterface::postMessage(fs && fs->get_client() ? fs->get_client()->last_error() : "FTP upload failed");
        }
    }
    if (cached) {
        FRESULT closed = fm->fclose(cached);
        if (ret == FR_OK) ret = closed;
    }
    if (temp_path.length()) fm->delete_file(temp_path.c_str());
    delete this;
    return ret;
}

FRESULT FileSystemFTP::file_open(const char *filename, uint8_t flags, File **file)
{
    *file = NULL;
    if (connect_if_needed() < 0) return FR_NOT_READY;
    mstring remote;
    build_ftp_path(filename, remote);
    if (!(flags & (FA_WRITE | FA_CREATE_ANY))) {
        FRESULT streamed = open_read_stream(remote.c_str(), file);
        if (streamed != FR_OK || *file) return streamed;
        // Servers without SIZE retain the existing seekable cache behavior.
    }
    FileManager *fm = FileManager::getFileManager();
    File *temp = NULL; mstring canonical;
    // A unique fresh temporary file per open: never reuse an incomplete or stale
    // download, and never share cached bytes between servers or paths.
    FRESULT result = fm->create_temp_file("ftp", "ftp-transfer.tmp", FA_READ | FA_WRITE | FA_CREATE_NEW, &temp, &canonical);
    if (result != FR_OK || !temp) return result != FR_OK ? result : FR_INT_ERR;
    bool creating = (flags & FA_CREATE_ANY) != 0;
    if (creating && (flags & FA_CREATE_NEW)) {
        // Preserve copy-without-overwrite semantics. Use a parent listing rather
        // than treating a RETR failure as proof that a remote file is absent.
        mstring parent(remote.c_str());
        const char *slash = strrchr(remote.c_str(), '/');
        const char *leaf = slash ? slash + 1 : remote.c_str();
        int prefix = slash ? slash - remote.c_str() : 0;
        char *dir = new char[prefix + 2]; memcpy(dir, remote.c_str(), prefix); dir[prefix] = 0;
        if (!prefix) strcpy(dir, "/");
        FTPExists check = {leaf, false, false};
        // Defined below with the listing parser, shared by browser and existence check.
        extern bool ftp_check_exists(void *, const char *, bool);
        int listed = client->list_lines(dir, ftp_check_exists, &check);
        delete[] dir;
        if (listed < 0 || check.unsupported || check.found) {
            fm->fclose(temp); fm->delete_file(canonical.c_str());
            return check.found ? FR_EXIST : FR_DISK_ERR;
        }
    }
    if (!creating) {
        int bytes = 0;
        if (client->retr(remote.c_str(), temp, &bytes) < 0 || temp->seek(0) != FR_OK) {
            UserInterface::postMessage(client->last_error());
            fm->fclose(temp); fm->delete_file(canonical.c_str()); return FR_DISK_ERR;
        }
    }
    *file = new FileOnFTP(this, temp, remote.c_str(), creating, canonical.c_str());
    return FR_OK;
}

#endif

#ifndef SERVER_EDITOR_FILE_H
#define SERVER_EDITOR_FILE_H
// Write a complete temporary file first. Preserve the old file as .bak, and
// restore it if installing the replacement fails. No claim of power-loss atomicity.
static FRESULT save_servers(JSON *document)
{
    char *text = new char[4096];
    ServerEditorJson writer(text, 4096);
    if (!writer.write(document)) { delete[] text; return FR_INVALID_PARAMETER; }
    FileManager *fm = FileManager::getFileManager();
    FRESULT result = fm->create_dir("/Flash/config");
    if (result != FR_OK && result != FR_EXIST) { delete[] text; return result; }
    File *file = NULL;
    result = fm->fopen("/Flash/config/server.json.tmp", FA_WRITE | FA_CREATE_ALWAYS, &file);
    uint32_t written = 0; size_t length = strlen(text);
    if (result == FR_OK) {
        result = file->write(text, length, &written);
        if (result == FR_OK && written != length) result = FR_DISK_ERR;
        if (result == FR_OK) result = file->sync();
        fm->fclose(file);
    }
    delete[] text;
    if (result != FR_OK) return result;
    result = fm->delete_file("/Flash/config/server.json.bak");
    if (result != FR_OK && result != FR_NO_FILE) return result;
    result = fm->rename("/Flash/config/server.json", "/Flash/config/server.json.bak");
    bool had_old = result == FR_OK;
    if (result != FR_OK && result != FR_NO_FILE) return result;
    result = fm->rename("/Flash/config/server.json.tmp", "/Flash/config/server.json");
    if (result != FR_OK && had_old) fm->rename("/Flash/config/server.json.bak", "/Flash/config/server.json");
    return result;
}
#endif

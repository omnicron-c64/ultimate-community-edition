#ifndef FILE_COPY_STREAM_H
#define FILE_COPY_STREAM_H
// Both handles belong to the caller's FileManager; this helper closes both.
static FRESULT copy_stream_and_close(FileManager *fm, File *source, File *destination)
{
    FRESULT result = FR_OK;
    uint8_t *buffer = new uint8_t[32768];
    if (!buffer) result = FR_NOT_ENOUGH_CORE;
    while (result == FR_OK) {
        uint32_t read = 0, written = 0;
        result = source->read(buffer, 32768, &read);
#if OS
        vTaskDelay(2);
#endif
        if (result != FR_OK || !read) break;
        result = destination->write(buffer, read, &written);
#if OS
        vTaskDelay(2);
#endif
        if (result == FR_OK && written != read) result = FR_DISK_FULL;
    }
    delete[] buffer;
    // In particular, do not STOR a partial local copy to an FTP destination.
    if (result != FR_OK) destination->abort_write();
    FRESULT closed = fm->fclose(destination);
    FRESULT source_closed = fm->fclose(source);
    if (result == FR_OK) result = closed;
    if (result == FR_OK) result = source_closed;
    return result;
}
#endif

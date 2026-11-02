#ifndef FTP_NATIVE_FILEMANAGER_H
#define FTP_NATIVE_FILEMANAGER_H
#include <stdint.h>
enum FRESULT { FR_OK, FR_DISK_ERR, FR_INVALID_PARAMETER, FR_DENIED, FR_NOT_ENOUGH_CORE, FR_DISK_FULL };
class FileSystem {};
class File {
public:
 File(FileSystem * = 0) {}
 virtual ~File() {}
 virtual FRESULT close() { delete this; return FR_OK; }
 virtual void abort_write() {}
 virtual FRESULT read(void *, uint32_t, uint32_t *) = 0;
 virtual FRESULT write(const void *, uint32_t, uint32_t *) = 0;
 virtual FRESULT seek(uint32_t) = 0;
 virtual FRESULT sync() = 0;
};
#endif

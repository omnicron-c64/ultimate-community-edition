#ifndef FTP_NATIVE_FILEMANAGER_H
#define FTP_NATIVE_FILEMANAGER_H
#include <stdint.h>
enum FRESULT { FR_OK, FR_DISK_ERR };
class File {
public:
 virtual ~File() {}
 virtual FRESULT read(void *, uint32_t, uint32_t *) = 0;
 virtual FRESULT write(const void *, uint32_t, uint32_t *) = 0;
 virtual FRESULT seek(uint32_t) = 0;
 virtual FRESULT sync() = 0;
};
#endif

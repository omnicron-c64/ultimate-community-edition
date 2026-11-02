#ifndef FTP_LISTING_PARSER_H
#define FTP_LISTING_PARSER_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static bool parse_ftp_list_line(const char *line, char *name, int namesize, uint32_t *size, bool *is_dir)
{
    *is_dir = false;
    *size = 0;
    name[0] = '\0';

    if (!line || !line[0]) {
        return false;
    }

    // Unix-style: "drwxr-xr-x  2 user group  4096 Jan 01 12:00 dirname"
    if (line[0] == 'd' || line[0] == '-' || line[0] == 'l') {
        *is_dir = (line[0] == 'd');

        // Find the filename — it's after the date/time field.
        // Skip permissions, links, owner, group, size, month, day, time/year
        // Strategy: scan from the right for the name after the last time/year field
        const char *p = line;
        int fields = 0;
        while (*p && fields < 8) {
            // skip whitespace
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (!*p) {
                break;
            }
            // Parse size from field 4 (0-indexed)
            if (fields == 4) {
                *size = (uint32_t)strtoul(p, NULL, 10);
            }
            // skip field content
            while (*p && *p != ' ' && *p != '\t') {
                p++;
            }
            fields++;
        }
        // skip whitespace before filename
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p) {
            // For symlinks, strip " -> target"
            const char *arrow = line[0] == 'l' ? strstr(p, " -> ") : NULL;
            int len = arrow ? (int)(arrow - p) : (int)strlen(p);
            // Trim trailing \r\n
            while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n')) {
                len--;
            }
            if (len >= namesize) {
                return false;
            }
            memcpy(name, p, len);
            name[len] = '\0';
            return len > 0;
        }
        return false;
    }

    // Windows-style: "01-01-24  12:00PM  <DIR>  dirname" or "01-01-24  12:00PM  12345 filename"
    if (line[0] >= '0' && line[0] <= '9') {
        const char *p = line;
        // Skip date
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        // Skip time
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        // <DIR> or size
        if (strncmp(p, "<DIR>", 5) == 0) {
            *is_dir = true;
            p += 5;
        } else {
            *size = (uint32_t)strtoul(p, NULL, 10);
        }
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        // Remainder is filename
        int len = (int)strlen(p);
        while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n')) {
            len--;
        }
        if (len >= namesize) {
            return false;
        }
        memcpy(name, p, len);
        name[len] = '\0';
        return len > 0;
    }

    return false;
}

// MLSD facts are structured; LIST fallback supports ordinary Unix/Windows listings.
static bool parse_mlsd(const char *line, char *name, int capacity, uint32_t *size, bool *dir)
{
    const char *space = strchr(line, ' ');
    if (!space || !space[1] || (int)strlen(space + 1) >= capacity) return false;
    *dir = false; *size = 0; bool typed = false;
    for (const char *p = line; p < space;) {
        const char *end = strchr(p, ';'); if (!end || end > space) return false;
        if (!strncasecmp(p, "type=", 5)) {
            int n = end - p - 5;
            if ((n == 4 && !strncasecmp(p + 5, "cdir", 4)) || (n == 4 && !strncasecmp(p + 5, "pdir", 4))) { name[0] = 0; return true; }
            if (n == 3 && !strncasecmp(p + 5, "dir", 3)) { *dir = true; typed = true; }
            else if (n == 4 && !strncasecmp(p + 5, "file", 4)) typed = true;
            else return false;
        } else if (!strncasecmp(p, "size=", 5)) {
            uint64_t value = 0;
            if (p + 5 == end) return false;
            for (const char *q = p + 5; q < end; ++q) {
                if (*q < '0' || *q > '9') return false;
                value = value * 10 + *q - '0'; if (value > 0xffffffffULL) return false;
            }
            *size = value;
        }
        p = end + 1;
    }
    if (!typed) return false;
    strcpy(name, space + 1); return true;
}
static bool ftp_entry(const char *line, bool machine, char *name, int length, uint32_t *size, bool *dir)
{
    if (!*line || (!machine && !strncmp(line, "total ", 6))) { name[0] = 0; return true; }
    bool ok = machine ? parse_mlsd(line, name, length, size, dir) : parse_ftp_list_line(line, name, length, size, dir);
    // Never silently truncate a remote filename into a different path.
    if (ok) for (const char *p = name; *p; ++p) if ((unsigned char)*p < 32 || *p == '/' || *p == '\\') return false;
    return ok;
}

bool ftp_check_exists(void *context, const char *line, bool machine)
{
    FTPExists *check = (FTPExists *)context;
    char name[512]; uint32_t size; bool dir;
    if (!ftp_entry(line, machine, name, sizeof(name), &size, &dir)) { check->unsupported = true; return false; }
    if (!strcmp(check->name, name)) check->found = true;
    return true;
}
#endif

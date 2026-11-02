#ifndef WIFI_SAVED_H
#define WIFI_SAVED_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define CMD_WIFI_SAVED_LIST 0x30
#define CMD_WIFI_SAVED_CONNECT 0x31
#define CMD_WIFI_SAVED_FORGET 0x32
#define WIFI_SAVED_MAX 16
// Following rpc_header_t: int32 error, uint8 count, then count records.
// Wire records are byte-only: slot, preferred, auth, SSID[33].
typedef struct { uint8_t slot, preferred, auth; char ssid[33]; } wifi_saved_record;
static inline int wifi_saved_decode(const void *packet, size_t size,
                                    wifi_saved_record *out, unsigned *count) {
    *count = 0;
    if (!packet || size < 9) return -1;
    const uint8_t *p = (const uint8_t *)packet;
    int32_t err; memcpy(&err, p + 4, 4);
    if (err) return err;
    unsigned n = p[8];
    if (n > WIFI_SAVED_MAX || size != 9 + n * sizeof(wifi_saved_record)) return -1;
    unsigned slots = 0;
    for (unsigned i = 0; i < n; ++i) {
        wifi_saved_record r; memcpy(&r, p + 9 + i * sizeof(r), sizeof(r));
        if (r.slot >= WIFI_SAVED_MAX || r.preferred > 1 || !r.ssid[0] ||
            !memchr(r.ssid, 0, sizeof(r.ssid)) || (slots & (1u << r.slot))) return -1;
        slots |= 1u << r.slot; out[i] = r;
    }
    *count = n; return 0;
}
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint16_t u16_t;
#include "wifi_scan_result.h"

static void empty(const ultimate_ap_records_t &aps) {
    const unsigned char *p = (const unsigned char *)&aps;
    for (size_t i = 0; i < sizeof(aps); ++i) assert(p[i] == 0);
}
int main() {
    static_assert(sizeof(event_pkt_disconnected) == 6, "disconnect reason wire extension");
    static_assert(sizeof(rpc_header_t) == 4, "wire header");
    static_assert(sizeof(ultimate_ap_record_t) == 42, "wire record");
    rpc_scan_resp reply = {};
    ultimate_ap_records_t out;
    reply.hdr.command = CMD_WIFI_SCAN;
    reply.rec.num_records = 1;
    memset(reply.rec.aps[0].ssid, 'A', 33);
    const size_t prefix = offsetof(rpc_scan_resp, rec) + offsetof(ultimate_ap_records_t, aps);
    assert(wifi_decode_scan(&reply, prefix + 42, &out) == 0);
    assert(out.num_records == 1 && strlen((char *)out.aps[0].ssid) == 32);
    assert(wifi_decode_scan(&reply, prefix + 41, &out) == -1); empty(out);
    reply.esp_err = 0x101;
    assert(wifi_decode_scan(&reply, sizeof(rpc_espcmd_resp), &out) == 0x101); empty(out);
    reply.esp_err = 0;
    char http[] = "HTTP/1.1 200 OK\r\nContent-Length: 123\r\n";
    assert(wifi_decode_scan(http, sizeof(http), &out) == -1); empty(out);
    assert(wifi_decode_scan(NULL, 0, &out) == -1); empty(out);
    unsigned cases = 0;
    for (unsigned count = 0; count < 256; ++count) {
        reply.rec.num_records = count;
        for (size_t size = 0; size <= sizeof(reply); ++size) {
            // Allocate exactly the received bytes so ASan catches any overread.
            unsigned char *packet = new unsigned char[size];
            memcpy(packet, &reply, size);
            int rc = wifi_decode_scan(packet, size, &out);
            bool valid = count <= DEFAULT_SCAN_LIST_SIZE && size >= prefix + count * 42;
            assert((rc == 0) == valid);
            if (!valid) empty(out);
            else assert(out.num_records == count);
            delete[] packet;
            ++cases;
        }
    }
    printf("wifi_scan: %u count/length cases passed, plus HTTP/error/SSID cases\n", cases);
}

#ifndef WIFI_SCAN_RESULT_H
#define WIFI_SCAN_RESULT_H
#include <stddef.h>
#include <string.h>
#include "rpc_calls.h"

// Decode the compact wire reply. Never interpret a failed/truncated reply as APs.
static inline int wifi_decode_scan(const void *data, size_t size, ultimate_ap_records_t *aps)
{
    memset(aps, 0, sizeof(*aps));
    const size_t prefix = offsetof(rpc_scan_resp, rec);
    if (!data || size < prefix) return -1;
    rpc_header_t hdr;
    int status;
    memcpy(&hdr, data, sizeof(hdr));
    memcpy(&status, (const unsigned char *)data + offsetof(rpc_scan_resp, esp_err), sizeof(status));
    if (hdr.command != CMD_WIFI_SCAN) return -1;
    if (status) return status;
    const size_t records = prefix + offsetof(ultimate_ap_records_t, aps);
    if (size < records) return -1;
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned count = bytes[prefix + offsetof(ultimate_ap_records_t, num_records)];
    if (count > DEFAULT_SCAN_LIST_SIZE || size < records + count * sizeof(ultimate_ap_record_t)) return -1;
    memcpy(aps->aps, bytes + records, count * sizeof(ultimate_ap_record_t));
    for (unsigned i = 0; i < count; ++i) aps->aps[i].ssid[32] = 0;
    aps->num_records = count;
    return 0;
}
#endif

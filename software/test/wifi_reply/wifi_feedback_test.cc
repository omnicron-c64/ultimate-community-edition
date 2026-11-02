#include <assert.h>
#include <stdio.h>
#include "wifi_connection_feedback.h"

int main()
{
    WifiConnectionFeedback f;
    assert(f.phase == WifiConnectionFeedback::Idle);
    f.disconnected(202);
    assert(f.phase == WifiConnectionFeedback::Idle);
    f.begin("Target AP");
    assert(f.phase == WifiConnectionFeedback::Connecting);
    f.disconnected(8); // deliberate departure from previous AP
    assert(f.phase == WifiConnectionFeedback::Connecting);
    f.connected("Other AP");
    assert(f.phase == WifiConnectionFeedback::Connecting);
    f.disconnected(202);
    assert(f.phase == WifiConnectionFeedback::Failed && f.reason == 202);
    f.connected("Target AP");
    f.disconnected(8);
    assert(f.phase == WifiConnectionFeedback::Failed && f.reason == 202);
    f.finish();
    f.begin("Target AP");
    f.connected("Target AP");
    assert(f.phase == WifiConnectionFeedback::WaitingForIP);
    f.disconnected(8); // genuine disconnect after association is a failure
    assert(f.phase == WifiConnectionFeedback::Failed);
    char full[33]; memset(full, 'X', 32); full[32] = 0;
    f.finish(); f.begin(full); f.connected(full);
    assert(f.phase == WifiConnectionFeedback::WaitingForIP && strlen(f.target) == 32);
    unsigned char packet[] = {0x42, 0xFF, 0, 0, 202, 0};
    for (size_t n = 0; n < 6; ++n) {
        unsigned char *short_packet = new unsigned char[n];
        memcpy(short_packet, packet, n);
        assert(wifi_disconnect_reason(short_packet, n) == 0);
        delete[] short_packet;
    }
    assert(wifi_disconnect_reason(packet, 6) == 202);
    assert(wifi_disconnect_reason(NULL, 6) == 0);
    assert(strstr(wifi_connection_failure(202), "Authentication failed"));
    assert(strstr(wifi_connection_failure(201), "not found"));
    assert(strstr(wifi_connection_failure(204), "handshake timed out"));
    assert(strstr(wifi_connection_failure(15), "handshake timed out"));
    assert(strstr(wifi_connection_failure(210), "matching security"));
    assert(strstr(wifi_connection_failure(212), "signal"));
    assert(strstr(wifi_connection_failure(0), "connection failed"));
    assert(strstr(wifi_connection_failure(65535), "connection failed"));
    assert(strstr(wifi_connection_request_failure(0x300B), "Invalid password format"));
    assert(strstr(wifi_connection_request_failure(0x300A), "Invalid access point"));
    assert(strstr(wifi_connection_request_failure(-1), "No valid reply"));
    assert(strstr(wifi_connection_request_failure(0x101), "busy"));
    // Messages fit the original 40-column screen and 128-byte result buffer.
    for (unsigned code = 0; code < 65536; ++code) {
        const char *s = wifi_connection_failure(code);
        assert(strlen(s) + 20 < 128);
        unsigned column = 0;
        for (; *s; ++s) { if (*s == '\n') column = 0; else assert(++column <= 34); }
    }
    puts("wifi_feedback: phases, retry, target matching, legacy/truncated packets and 65536 reason mappings passed");
}

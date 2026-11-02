#ifndef WIFI_CONNECTION_FEEDBACK_H
#define WIFI_CONNECTION_FEEDBACK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Reason numbers follow ESP-IDF v5.3.1 esp_wifi_types_generic.h.
static inline const char *wifi_connection_failure(unsigned reason)
{
    switch (reason) {
    case 2: case 15: case 204:
        return "WiFi handshake timed out.\nCheck password or signal.";
    case 202: case 23:
        return "Authentication failed.\nCheck password / AP settings.";
    case 201:
        return "Access point not found.\nCheck AP power and range.";
    case 210: case 211:
        return "No AP with matching security.\nCheck AP security settings.";
    case 212: case 200:
        return "WiFi signal lost or too weak.\nCheck range and signal.";
    case 203: case 5:
        return "Association failed.\nCheck AP settings / capacity.";
    default:
        return "WiFi connection failed.\nCheck AP, password and signal.";
    }
}

// ESP-IDF esp_wifi.h request errors (ESP_ERR_WIFI_BASE = 0x3000).
static inline const char *wifi_connection_request_failure(int error)
{
    switch (error) {
    case 0x300B: return "Invalid password format.\nCheck password length / format.";
    case 0x300A: return "Invalid access point name.\nRescan and select the AP again.";
    case 0x102: return "Invalid connection settings.\nCheck AP name / security.";
    case 0x101: return "WiFi module is busy.\nWait, then retry Connect.";
    case -1: return "No valid reply from WiFi module.\nWait, then retry Connect.";
    default: return "Connection request failed.\nCheck WiFi / network settings.";
    }
}

static inline unsigned wifi_disconnect_reason(const void *packet, size_t size)
{
    // Legacy modules send only the 4-byte RPC header: reason is unavailable.
    if (!packet || size < 6) return 0;
    uint16_t reason;
    memcpy(&reason, (const unsigned char *)packet + 4, sizeof(reason));
    return reason;
}

struct WifiConnectionFeedback {
    enum Phase { Idle, Connecting, WaitingForIP, Failed };
    volatile Phase phase;
    volatile unsigned reason;
    char target[33];
    WifiConnectionFeedback() : phase(Idle), reason(0) { target[0] = 0; }
    void begin(const char *ssid) {
        strncpy(target, ssid, 32); target[32] = 0;
        reason = 0; phase = Connecting;
    }
    void connected(const char *ssid) {
        if (phase == Connecting && strncmp(target, ssid, 32) == 0) phase = WaitingForIP;
    }
    void disconnected(unsigned code) {
        if (phase == Idle || phase == Failed) return;
        // Replacing an existing connection deliberately disconnects it first.
        if (phase == Connecting && code == 8) return;
        reason = code; phase = Failed;
    }
    void finish() { phase = Idle; }
};
#endif

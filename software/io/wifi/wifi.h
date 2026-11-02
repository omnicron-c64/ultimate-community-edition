/*
 * wifi.h
 *
 *  Created on: Sep 2, 2018
 *      Author: gideon
 */

#ifndef WIFI_H_
#define WIFI_H_

#include "dma_uart.h"
#include "esp32.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "browsable.h"
#include "size_str.h"
//#include "network_esp32.h"
#include "wifi_cmd.h"
#include "wifi_connection_feedback.h"

// This class provides an interface to the WiFi module, to manage and program it
typedef enum {
    eWifi_Off,
    eWifi_Download,
    eWifi_NotDetected,
    eWifi_ModuleDetected,
    eWifi_AppDetected,
    eWifi_Scanning,
    eWifi_NotConnected,
    eWifi_Connected,
    eWifi_Disabled,
} WifiState_t;

class NetworkLWIP_WiFi;

class WiFi : public Esp32Application
{
    TaskHandle_t runModeTask;
    command_buf_context_t *packets;
    DmaUART *uart;
    NetworkLWIP_WiFi *netstack;

    WifiState_t state;
    volatile unsigned lastDisconnectReason;
    WifiConnectionFeedback connectionFeedback;
    WifiState_t scanReturnState;
    bool scanAttempted;
    int scanResult;
    char    last_ap[33];
    char    moduleName[34];
    char    moduleType[24];
    uint8_t my_mac[6];
    uint32_t my_ip;
    uint32_t my_gateway;
    uint32_t my_netmask;

    bool RequestEcho(void);
    void RxPacket(command_buf_t *);

    static void RunModeTaskStart(void *context);
    void RunModeThread();
    void RefreshRoot();
    void RefreshAPs();
public:
    WiFi();
    static void Init(void *obj, void *param);

    BaseType_t doRequestEcho(void);

    WifiState_t getState(void) { return state; }
    bool canTransmit(void) { return state == eWifi_Connected ||
        (state == eWifi_Scanning && scanReturnState == eWifi_Connected); }
    const char *getLastAP(void) { return canTransmit() ? last_ap : "-"; }
    const char *getModuleName(void) { return moduleName; }
    const char *getModuleType(void) { return moduleType; }
    void  getMacAddr(uint8_t *target) { memcpy(target, my_mac, 6); }
    char *getIpAddrString(char *buf, int max) { sprintf(buf, "%d.%d.%d.%d", (my_ip >> 0) & 0xff, (my_ip >> 8) & 0xff, (my_ip >> 16) & 0xff, (my_ip >> 24) & 0xff); return buf; }

    void sendEvent(uint8_t code);
    int connectWithFeedback(UserInterface *ui, const char *ssid, const char *password, uint8_t auth, int savedSlot = -1);
    unsigned getDisconnectReason() const { return lastDisconnectReason; }
    void sendConnectEvent(const char *ssid, const char *pass, uint8_t auth);
    void freeBuffer(command_buf_t *buf);
    int  getAccessPointItems(Browsable *parent, IndexedList<Browsable *> &list);

    // Debug
    void PrintUartStatus() { uart->PrintStatus(); }
    // User Interface functions
    void Disable(void);
    void Enable(void);
    void RadioOn(void);
    void RadioOff(void);

    // From Esp32Application
    void Init(DmaUART *uart, command_buf_context_t *packets);
    void Start();
    void Terminate();
};

err_t wifi_tx_packet(void *driver, void *buffer, int length);
void wifi_free(void *driver, void *buffer);

extern WiFi wifi;

class BrowsableWifiAP : public Browsable
{
    Browsable *parent;
    char ssid[33];
    int8_t dbm;
    uint8_t auth;
public:
    BrowsableWifiAP(Browsable *parent, char *ssid, int8_t dbm, uint8_t auth) {
        this->parent = parent;
        strncpy(this->ssid, ssid, 32);
        this->ssid[32] = 0;
        this->dbm = dbm;
        this->auth = auth;
    }

    void getDisplayString(char *buffer, int width, UserInterface *ui) {
        char display[33];
        for (int i = 0; i < 33; ++i) {
            unsigned char c = ssid[i];
            display[i] = (c && (c < 32 || c == 127)) ? '?' : c;
        }
        sprintf(buffer, "%#s%5d", 31, display, dbm);
    }

    void fetch_context_items(IndexedList<Action *>&items);
	IndexedList<Browsable *> *getSubItems(int &error) { error = -1; return &children; }
    static SubsysResultCode_e connect_ap(SubsysCommand *cmd);
};

#endif /* WIFI_H_ */

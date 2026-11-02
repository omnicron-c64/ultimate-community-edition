/*
 * network_esp32.c
 *
 *  Created on: Nov 11, 2021
 *      Author: gideon
 */

#include "FreeRTOS.h"
#include "network_esp32.h"
#include "rpc_calls.h"
#include <string.h>
#include "dump_hex.h"
#include "subsys.h"
#include "userinterface.h"
#include "tree_browser.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
// Concept:
// 1) Take buffer from cmd buffer free queue (may block)
// 2) Fill buffer with command and task and sequence code
// 3) Fill buffer with params
// 4) Send buffer by putting it in the transmit queue
// 5) Block task with 'wait notify'
// 6) Use notification value as pointer to receive packet
// 7) Take response from receive packet, like retval and errno, and/or data
// 8) Return buffer to cmd buffer free queue

//#define ENTRY(t) cmd_buffer_get

#include "wifi.h"
#include "itu.h"

const char *authmodes[] = { "Open", "WEP", "WPA PSK", "WPA2 PSK", "WPA/WPA2 PSK", "WPA2 Enterprise", "WPA3 PSK", "WPA2/WPA3 PSK", "WAPI PSK" };

//-----------------------------------
struct t_cfg_definition wifi_config[] = {
#if U64 == 1
    { CFG_WIFI_PANEL, CFG_TYPE_FUNC, "WiFi status / reconnect", "", (const char **)NetworkLWIP_WiFi::cfg_status_screen, 0, 0, 0 },
    { CFG_WIFI_SAVED, CFG_TYPE_FUNC, "Saved access points", "", (const char **)NetworkLWIP_WiFi::cfg_saved_aps, 0, 0, 0 },
#endif
#if U64 == 2
    { CFG_WIFI_ENA,    CFG_TYPE_FUNC,   "Enable",                        "",(const char **)NetworkLWIP_WiFi :: cfg_enable, 0, 0, 0 },
    { CFG_WIFI_DIS,    CFG_TYPE_FUNC,   "Disable",                       "",(const char **)NetworkLWIP_WiFi :: cfg_disable, 0, 0, 0 },
#else
    { CFG_WIFI_ENABLE, CFG_TYPE_ENUM,   "WiFi Enabled",                  "%s", en_dis,     0,  1, 1 },
#endif
    { CFG_WIFI_DISCONN,CFG_TYPE_FUNC,   "Disconnect",                    "",(const char **)NetworkLWIP_WiFi :: cfg_disconn, 0, 0, 0 },
    { CFG_WIFI_CONN,   CFG_TYPE_FUNC,   "Connect to last AP",            "",(const char **)NetworkLWIP_WiFi :: cfg_conn_last, 0, 0, 0 },
    { CFG_WIFI_SEL_AP, CFG_TYPE_FUNC,   "Select AP from list",           "",(const char **)NetworkLWIP_WiFi :: cfg_show_aps, 0, 0, 0 },
    { CFG_WIFI_ENT_AP, CFG_TYPE_FUNC,   "Enter AP manually",             "",(const char **)NetworkLWIP_WiFi :: cfg_enter_ap, 0, 0, 0 },
    { CFG_WIFI_FORGET, CFG_TYPE_FUNC,   "Forget APs",                    "",(const char **)NetworkLWIP_WiFi :: cfg_forget, 0, 0, 0 },
    { CFG_SEPARATOR,   CFG_TYPE_SEP,    "",                               "",  NULL,       0,  0, 0 },
    { CFG_NET_DHCP_EN, CFG_TYPE_ENUM,   "Use DHCP",                      "%s", en_dis,     0,  1, 1 },
	{ CFG_NET_IP,      CFG_TYPE_STRING, "Static IP",					 "%s", NULL,       7, 16, (int)"192.168.2.64" },
	{ CFG_NET_NETMASK, CFG_TYPE_STRING, "Static Netmask",				 "%s", NULL,       7, 16, (int)"255.255.255.0" },
	{ CFG_NET_GATEWAY, CFG_TYPE_STRING, "Static Gateway",				 "%s", NULL,       7, 16, (int)"192.168.2.1" },
	{ CFG_NET_DNS,     CFG_TYPE_STRING, "Static DNS",		      		 "%s", NULL,       7, 16, (int)"8.8.8.8" },
    { CFG_SEPARATOR,   CFG_TYPE_SEP,    "",                               "",  NULL,       0,  0, 0 },
    { CFG_WIFI_STATUS, CFG_TYPE_INFO,   "Status",                        "%s", NULL,       0, 32, (int)"" },
    { CFG_WIFI_CUR_AP, CFG_TYPE_INFO,   "Connected to",                  "%s", NULL,       0, 32, (int)"" },
    { CFG_WIFI_CUR_IP, CFG_TYPE_INFO,   "Active IP address",             "%s", NULL,       0, 32, (int)"" },
    { CFG_WIFI_MAC,    CFG_TYPE_INFO,   "Interface MAC",                 "%s", NULL,       0, 32, (int)"" },
	{ CFG_TYPE_END,    CFG_TYPE_END,    "", "", NULL, 0, 0, 0 }
};

NetworkLWIP_WiFi :: NetworkLWIP_WiFi(void *driver, driver_output_function_t out, driver_free_function_t free)
    : NetworkInterface(driver, out, free)
{
}

NetworkLWIP_WiFi :: ~NetworkLWIP_WiFi()
{

}

void NetworkLWIP_WiFi :: getDisplayString(int index, char *buffer, int width)
{
    char ip[16];

    WifiState_t state = wifi.getState();

    switch (state) {
    case eWifi_Off:
        sprintf(buffer, "WiFi    %#s\eLDisabled", width - 17, "");
        break;
    case eWifi_Download:
        sprintf(buffer, "WiFi    %#s\eGBusy", width - 17, "Programming ESP32");
        break;
    case eWifi_NotDetected:
        sprintf(buffer, "WiFi    %#s\eJNo Module", width - 17, "");
        break;
    case eWifi_ModuleDetected:
        sprintf(buffer, "WiFi    %#s\eGFirmware?", width - 17, wifi.getModuleType());
        break;
    case eWifi_AppDetected:               
    case eWifi_Scanning:
        sprintf(buffer, "WiFi    %#s\eGScanning", width - 17, wifi.getModuleName());
        break;
    case eWifi_NotConnected:
        sprintf(buffer, "WiFi    MAC %b:%b:%b:%b:%b:%b%#s\eJLink Down", mac_address[0], mac_address[1], mac_address[2],
                mac_address[3], mac_address[4], mac_address[5], width - 38, "");
        break;
    case eWifi_Connected:
        sprintf(buffer, "WiFi    IP: %#s\eMLink Up", width - 21, getIpAddrString(ip, 16));
        break;
    case eWifi_Disabled:
        sprintf(buffer, "WiFi    %#s\eJDisabled", width - 17, wifi.getModuleName());
        break;
    default:
        sprintf(buffer, "WiFi%#s\eJUnknown", width - 17, "");
        break;
    }
}

void NetworkLWIP_WiFi :: getSubItems(Browsable *parent, IndexedList<Browsable *> &list, int &error)
{
    if (wifi.getAccessPointItems(parent, list) == 0) {
        wifi.sendEvent(EVENT_RESCAN);
    }
    error = 0;
}

void NetworkLWIP_WiFi :: attach_config()
{
	register_store(0x57494649, "WiFi settings", wifi_config);
    cfg->set_sort_order(SORT_ORDER_CFG_WIFI);
    cfg->set_change_hook(CFG_NET_DHCP_EN, dhcp_change);
    dhcp_change(cfg->find_item(CFG_NET_DHCP_EN));
}

// from ConfigurableObject
void NetworkLWIP_WiFi :: effectuate_settings(void)
{
    NetworkInterface :: effectuate_settings();

	my_net_if.name[0] = 'W';
	my_net_if.name[1] = 'I';

#if U64 == 2
    // if (cfg->get_value(CFG_WIFI_ENABLE) && wifi.getState() == eWifi_Disabled) {
    //     wifi.RadioOn();
    // } else if (!(cfg->get_value(CFG_WIFI_ENABLE)) && wifi.getState() != eWifi_Disabled) {
    //     wifi.RadioOff();
    // }
#else
    if (wifi.getState() == eWifi_Off && cfg->get_value(CFG_WIFI_ENABLE)) {
        wifi.Enable();
    }
//  For now, we don't disable the Wifi here, because effectuate_settings is also called when the init callback of the network stack occurs, OWWW
//    else if (wifi.getState() != eWifi_Off && !cfg->get_value(CFG_WIFI_ENABLE)) {
//        wifi.Disable();
//    }
#endif
}

void NetworkLWIP_WiFi :: on_edit()
{
    ConfigItem *it = cfg->find_item(CFG_WIFI_CUR_AP);
    it->setEnabled(false);
    it->setString(wifi.getLastAP());

    it = cfg->find_item(CFG_WIFI_STATUS);
    it->setEnabled(false);

    bool ok_to_connect = false;
    WifiState_t state = wifi.getState();
    switch (state) {
    case eWifi_Off:
        it->setString("OFF");
        break;
    case eWifi_Download:
        it->setString("Programming");
        break;
    case eWifi_NotDetected:
        it->setString("No Module Detected");
        break;
    case eWifi_ModuleDetected:
        it->setString("Detected, ...");
        break;
    case eWifi_AppDetected:               
    case eWifi_Scanning:
        it->setString("Scanning...");
        break;
    case eWifi_NotConnected:
        it->setString("Link Down");
        ok_to_connect = true;
        break;
    case eWifi_Connected:
        it->setString("Link Up");
        ok_to_connect = true;
        break;
    case eWifi_Disabled:
        it->setString("Disabled");
        break;
    default:
        it->setString("???");
        break;
    }

    // cfg->find_item(CFG_WIFI_ENT_AP)->setEnabled(ok_to_connect);
    // cfg->find_item(CFG_WIFI_SEL_AP)->setEnabled(ok_to_connect);
    // cfg->find_item(CFG_WIFI_CONN)->setEnabled(ok_to_connect);

    uint8_t mac[8];
    char buf[32];
    wifi.getMacAddr(mac);
    sprintf(buf, "%b:%b:%b:%b:%b:%b", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    it = cfg->find_item(CFG_WIFI_MAC);
    it->setEnabled(false);
    it->setString(buf);

    it = cfg->find_item(CFG_WIFI_CUR_IP);
    it->setEnabled(false);
    it->setString(getIpAddrString(buf, 16));
}

void NetworkLWIP_WiFi :: fetch_context_items(IndexedList<Action *>&items)
{
#if U64 == 1
    items.append(new Action("WiFi status / reconnect", status_screen, 0, 0));
#endif
    if (wifi.getState() == eWifi_Connected) {
        items.append(new Action("Forget APs", NetworkLWIP_WiFi :: clear_aps, 0, 0));
        items.append(new Action("Disconnect", NetworkLWIP_WiFi :: disconnect, 0, 0));
        items.append(new Action("Disable", NetworkLWIP_WiFi :: disable, 0, 0));
    } else if (wifi.getState() == eWifi_NotConnected) {
        items.append(new Action("Show APs..", NetworkLWIP_WiFi :: list_aps, 0, 0));
        items.append(new Action("Connect", NetworkLWIP_WiFi :: auto_connect, 0, 0));
        items.append(new Action("Connect to..", NetworkLWIP_WiFi :: manual_connect, 0, 0));
        // items.append(new Action("Rescan APs", NetworkLWIP_WiFi :: rescan, 0, 0));
        items.append(new Action("Forget APs", NetworkLWIP_WiFi :: clear_aps, 0, 0));
        items.append(new Action("Disable", NetworkLWIP_WiFi :: disable, 0, 0));
    } else if ((wifi.getState() == eWifi_Off) || (wifi.getState() == eWifi_Disabled)) {
        items.append(new Action("Enable", NetworkLWIP_WiFi :: enable, 0, 0));
    } else if (wifi.getState() == eWifi_NotDetected) {
        items.append(new Action("Disable", NetworkLWIP_WiFi :: disable, 0, 0));
    }
}

SubsysResultCode_e NetworkLWIP_WiFi :: disconnect(SubsysCommand *cmd)
{
    wifi_wifi_disconnect();
    return SSRET_OK;
}

SubsysResultCode_e NetworkLWIP_WiFi :: clear_aps(SubsysCommand *cmd)
{
    if (cmd->user_interface) {
        if (cmd->user_interface->popup("Are you sure? Erase all stored APs?", BUTTON_OK|BUTTON_CANCEL) != BUTTON_OK) {
            return SSRET_ABORTED_BY_USER;
        }
    }

    if (wifi_forget_aps() != 0)
        return SSRET_GENERIC_ERROR;
    if (cmd->user_interface) {
        cmd->user_interface->popup("APs cleared and forgotten!", BUTTON_OK);
    }
    return SSRET_OK;
}

SubsysResultCode_e NetworkLWIP_WiFi :: auto_connect(SubsysCommand *cmd)
{
#if U64 == 1
    if (!cmd->user_interface) return SSRET_NO_USER_INTERFACE;
    reconnect(cmd->user_interface);
#else
    wifi_wifi_autoconnect();
#endif
    return SSRET_OK;
}

SubsysResultCode_e NetworkLWIP_WiFi :: manual_connect(SubsysCommand *cmd)
{
    if (!cmd->user_interface)
        return SSRET_NO_USER_INTERFACE;
    
    char ssid[36];
    char password[65];
    int authmode = 0;
    int ret;
    ssid[0] = 0;
    password[0] = 0;
    ret = cmd->user_interface->string_box("Name of Access Point (SSID)", ssid, 32);
    if ((ret < 1) || !(*ssid)) {
        return SSRET_ABORTED_BY_USER;
    }
    ret = cmd->user_interface->choice("Authentication", authmodes, 9);
    if (ret >= 0) {
        authmode = ret;
    } else {
        return SSRET_ABORTED_BY_USER;
    }
    if (authmode > 0) { // for anything except "Open", ask for password
        ret = cmd->user_interface->string_box("Password", password, 64);
        if (ret < 0) { // allow empty string (ret == 1)
            return SSRET_ABORTED_BY_USER;
        }
    }
    wifi.connectWithFeedback(cmd->user_interface, ssid, password, authmode);
    return SSRET_OK;
}

void NetworkLWIP_WiFi :: cfg_show_aps(UserInterface *intf, ConfigItem *it)
{
    WifiState_t state = wifi.getState();
    if ((state != eWifi_NotConnected) && (state != eWifi_Connected)) {
        intf->popup("Can only show APs when active", BUTTON_OK);
        return;
    }
    wifi.sendEvent(EVENT_RESCAN);
    BrowsableWifiAPList *broot = new BrowsableWifiAPList(); // new root!
    TreeBrowser *tb = new TreeBrowser(intf, broot);
    tb->has_border = true;
    tb->allow_exit = true;
    tb->has_path = false;
    tb->init();
    tb->setCleanup();

    intf->activate_uiobject(tb);
    // There is a new treebrowser now, which will call the fetch items from the BrowsableWifiAPList!!
}

void NetworkLWIP_WiFi :: cfg_enter_ap(UserInterface *intf, ConfigItem *it)
{
    WifiState_t state = wifi.getState();
    if ((state != eWifi_NotConnected) && (state != eWifi_Connected)) {
        intf->popup("Can only connect when active", BUTTON_OK);
        return;
    }
    SubsysCommand cmd(intf, 0, 0, 0, "", "");
    manual_connect(&cmd);
}

void NetworkLWIP_WiFi :: cfg_conn_last(UserInterface *intf, ConfigItem *it)
{
    WifiState_t state = wifi.getState();
    if ((state != eWifi_NotConnected) && (state != eWifi_Connected)) {
        intf->popup("Can only connect when active", BUTTON_OK);
        return;
    }
#if U64 == 1
    reconnect(intf);
#else
    wifi_wifi_autoconnect();
#endif
}

void NetworkLWIP_WiFi :: cfg_disconn(UserInterface *intf, ConfigItem *it)
{
    WifiState_t state = wifi.getState();
    if ((state != eWifi_NotConnected) && (state != eWifi_Connected)) {
        intf->popup("Can only disconnect when active", BUTTON_OK);
        return;
    }
    wifi_wifi_disconnect();    
}

void NetworkLWIP_WiFi :: cfg_enable(UserInterface *intf, ConfigItem *it)
{
    wifi.RadioOn();
}

void NetworkLWIP_WiFi :: cfg_disable(UserInterface *intf, ConfigItem *it)
{
    wifi.RadioOff();
}

void NetworkLWIP_WiFi :: cfg_forget(UserInterface *intf, ConfigItem *it)
{
    SubsysCommand cmd(intf, 0, 0, 0, "", "");
    clear_aps(&cmd);
}

SubsysResultCode_e NetworkLWIP_WiFi :: list_aps(SubsysCommand *cmd)
{
    wifi.sendEvent(EVENT_RESCAN);
    if(cmd->user_interface) {
        cmd->user_interface->send_keystroke(KEY_RIGHT);
        return SSRET_OK;
    }
    return SSRET_NO_USER_INTERFACE;
}

SubsysResultCode_e NetworkLWIP_WiFi :: rescan(SubsysCommand *cmd)
{
    wifi.sendEvent(EVENT_RESCAN);
    return SSRET_OK;
}

SubsysResultCode_e NetworkLWIP_WiFi :: disable(SubsysCommand *cmd)
{
#if U64 == 2
    wifi.RadioOff();
#else
    wifi.Disable();
#endif
    return SSRET_OK;
}

SubsysResultCode_e NetworkLWIP_WiFi :: enable(SubsysCommand *cmd)
{
#if U64 == 2
    wifi.RadioOn();
#else
    wifi.Enable();
#endif
    return SSRET_OK;
}

// Browsable APs
IndexedList<Browsable *> *BrowsableWifiAPList :: getSubItems(int &error)
{
    error = 0;
    wifi.getAccessPointItems(this, children);
    return &children;
}


// Do not report an old cached DHCP address as a newly acquired lease.
bool NetworkLWIP_WiFi :: hasUsableAddress()
{
    return if_up && !ip_addr_isany(&my_net_if.ip_addr) &&
        (!dhcp_enable || dhcp_supplied_address(&my_net_if));
}

#if U64 == 1
// SSIDs are untrusted display text; strip terminal/menu control bytes.
static void wifi_display_name(char *out, const char *ssid)
{
    for (int i = 0; i < 32; ++i) {
        unsigned char c = ssid[i];
        out[i] = c && (c < 32 || c >= 127) ? '?' : c;
        if (!c) return;
    }
    out[32] = 0;
}
static bool wifi_load_saved(UserInterface *ui, wifi_saved_record *records, unsigned *count)
{
    if (wifi.getState() != eWifi_Connected && wifi.getState() != eWifi_NotConnected) {
        ui->popup("WiFi is disabled or busy.\nEnable WiFi and retry when idle.", BUTTON_OK); return false;
    }
    ui->show_progress("Reading saved APs...", 1);
    int err = wifi_saved_list(records, count);
    ui->hide_progress();
    if (err) {
        ui->popup("Cannot read saved APs.\nCheck module firmware V1.9\nand retry when WiFi is idle.", BUTTON_OK);
        return false;
    }
    return true;
}
void NetworkLWIP_WiFi::reconnect(UserInterface *ui)
{
    wifi_saved_record records[WIFI_SAVED_MAX]; unsigned count;
    if (!wifi_load_saved(ui, records, &count)) return;
    if (!count) { ui->popup("No saved APs.\nScan or enter an AP first.", BUTTON_OK); return; }
    unsigned selected = 0;
    for (unsigned i = 0; i < count; ++i) if (records[i].preferred) selected = i;
    wifi_saved_record &r = records[selected];
    wifi.connectWithFeedback(ui, r.ssid, "", r.auth, r.slot);
}
void NetworkLWIP_WiFi::saved_aps(UserInterface *ui)
{
    for (;;) {
        wifi_saved_record records[WIFI_SAVED_MAX]; unsigned count;
        if (!wifi_load_saved(ui, records, &count)) return;
        if (!count) { ui->popup("No saved access points.", BUTTON_OK); return; }
        char names[WIFI_SAVED_MAX][36]; const char *choices[WIFI_SAVED_MAX];
        for (unsigned i = 0; i < count; ++i) {
            names[i][0] = records[i].preferred ? '*' : ' ';
            names[i][1] = ' ';
            wifi_display_name(names[i] + 2, records[i].ssid);
            choices[i] = names[i];
        }
        int selected = ui->choice("Saved APs (* last successful)", choices, count);
        if (selected < 0) return;
        const char *actions[] = {"Connect", "Forget this AP", "Back"};
        int action = ui->choice(names[selected], actions, 3);
        wifi_saved_record &record = records[selected];
        if (action == 0) wifi.connectWithFeedback(ui, record.ssid, "", record.auth, record.slot);
        else if (action == 1 && ui->popup("Forget this saved AP?\nCurrent link is kept until\ndisconnected. Password is erased.", BUTTON_OK | BUTTON_CANCEL) == BUTTON_OK) {
            int err = wifi_saved_action(CMD_WIFI_SAVED_FORGET, &record);
            ui->popup(err ? "Could not forget AP.\nRefresh list and retry." : "Saved AP forgotten.", BUTTON_OK);
        }
    }
}
void NetworkLWIP_WiFi::show_details(UserInterface *ui)
{
    char ssid[33], ip[16], gateway[16], mask[16], dns[16], dns2[16], message[320];
    wifi_display_name(ssid, wifi.getLastAP());
    strncpy(ip, hasUsableAddress() ? ipaddr_ntoa(&my_net_if.ip_addr) : "not ready", sizeof(ip));
    strncpy(gateway, ipaddr_ntoa(&my_net_if.gw), sizeof(gateway));
    strncpy(mask, ipaddr_ntoa(&my_net_if.netmask), sizeof(mask));
    // lwIP stores DHCP-supplied DNS in its global resolver table, not my_dns.
    // Snapshot both addresses without a task switch on this single-core host;
    // format after leaving the critical section. This does not change DNS policy.
    ip_addr_t primary_dns, secondary_dns;
    ENTER_SAFE_SECTION;
    primary_dns = *dns_getserver(0);
    secondary_dns = *dns_getserver(1);
    LEAVE_SAFE_SECTION;
    if (ip_addr_isany(&primary_dns)) strcpy(dns, "not set");
    else ipaddr_ntoa_r(&primary_dns, dns, sizeof(dns));
    if (ip_addr_isany(&secondary_dns)) strcpy(dns2, "not set");
    else ipaddr_ntoa_r(&secondary_dns, dns2, sizeof(dns2));
    const char *link = wifi.canTransmit() ? "Associated" : "Not associated";
    snprintf(message, sizeof(message), "%s\nAP: %s\n%s IP: %s\nGateway: %s\nNetmask: %s\nActive DNS 1: %s\nActive DNS 2: %s\nLast disconnect reason: %u\n%s", link, ssid,
        dhcp_enable ? "DHCP" : "Static", ip, gateway, mask, dns, dns2,
        wifi.getDisconnectReason(), wifi.getModuleName());
    ui->popup(message, BUTTON_OK);
}
SubsysResultCode_e NetworkLWIP_WiFi::status_screen(SubsysCommand *cmd)
{
    UserInterface *ui = cmd->user_interface;
    if (!ui) return SSRET_NO_USER_INTERFACE;
    NetworkLWIP_WiFi *net = NULL;
    for (int i = 0; i < NetworkInterface::getNumberOfInterfaces(); ++i) {
        NetworkInterface *candidate = NetworkInterface::getInterface(i);
        if (candidate->is_wireless()) { net = (NetworkLWIP_WiFi *)candidate; break; }
    }
    if (!net) return SSRET_GENERIC_ERROR;
    const char *actions[] = {"Connection details / refresh", "Reconnect last saved AP", "Saved access points", "Scan / select AP", "Disconnect", "Back"};
    for (;;) {
        const char *title = net->hasUsableAddress() && wifi.canTransmit() ? "WiFi: IP ready" :
            wifi.canTransmit() ? "WiFi: associated, no IP" : "WiFi: not connected";
        int action = ui->choice(title, actions, 6);
        if (action < 0 || action == 5) return SSRET_OK;
        if (action == 0) net->show_details(ui);
        else if (action == 1) reconnect(ui);
        else if (action == 2) saved_aps(ui);
        else if (action == 3) { cfg_show_aps(ui, NULL); return SSRET_OK; }
        else if (action == 4) {
            int err = wifi_wifi_disconnect();
            ui->popup(err ? "Disconnect request failed." : "Disconnect requested.", BUTTON_OK);
        }
    }
}
void NetworkLWIP_WiFi::cfg_status_screen(UserInterface *ui, ConfigItem *)
{
    SubsysCommand cmd(ui, 0, 0, 0, "", ""); status_screen(&cmd);
}
void NetworkLWIP_WiFi::cfg_saved_aps(UserInterface *ui, ConfigItem *) { saved_aps(ui); }
#endif

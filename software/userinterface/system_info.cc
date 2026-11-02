/*
 * system_info.cc
 *
 *  Created on: Jul 17, 2021
 *      Author: gideon
 */

#include "system_info.h"
#include "c1541.h"
#include "c64.h"
#include "iec_interface.h"
#include "command_intf.h"
#include "acia.h"
#include "versions.h"
#include "gitinfo.h"
#include "u64.h"
#include "rtc.h"

extern C1541 *c1541_A;
extern C1541 *c1541_B;

/*
Drive Status information:

Drive A:      Enabled
Drive type:   1541-II
Mounted disk: this_is_file_name.d65
path:         /Usb0/directory1/directory/2directory3
              /directory4/directory5/    (when too long, let it flow to the next line)

Drive B:      Enabled
Drive type:   1541
Mounted disk: blank_disk.g64. (disk inserted using 'Insert blank disk').
path:         None

 */

void SystemInfo :: drive_info(StreamTextLog &b, C1541 *drive, char letter)
{
    b.format("Drive %c:      %s\n", letter, drive->get_drive_power() ? "Enabled" : "Disabled");
    if (drive->get_drive_power()) {
        b.format("Drive Bus ID: %d\n", drive->get_current_iec_address());
        b.format("Drive type:   %s\n", drive->get_drive_type_string());
        b.format("Drive ROM:    %s\n", drive->get_drive_rom_file());
        mstring path, name;
        drive->get_last_mounted_file(path, name);
        b.format("Mounted disk: %s\n", name.c_str());
        b.format("Path:         %s\n", path.c_str());
    }
    b.format("\n");
}

void iec_info(StreamTextLog &b) __attribute__((weak));

void iec_info(StreamTextLog &b)
{

}

void SystemInfo :: hw_modules(StreamTextLog &b, uint16_t bits, const char *msg)
{
    const char *hw[] = { "ACIA 6551 at $DE00", "ACIA 6551 at $DF00", "RAM Expansion Unit", "16MB REU", "Ultimate Command Intf.", "Ultimate Command Intf.",
                         "Ultimate Command Intf.", "Audio Sampler", "Write Mirroring", "Dynamic Bus Optimization", "Kernal Replacement" };

    if (bits) {
        int indent = 0;
        b.format("%14s", msg);
        int index = 0;
        while(bits) {
            if (bits & 1) {
                b.format("%#s%s\n", indent, "", hw[index]);
                indent = 14;
            }
            bits >>= 1;
            index ++;
        }
    }
}

void SystemInfo :: cart_info(StreamTextLog &b)
{
    C64 *c64 = C64 :: getMachine();
    const cart_def *cart = c64->get_cart_definition();
    b.format("Cartridge:    %s\n", cart->name);
    b.format("Filename:     %s\n", ((cart_def *)cart)->filename.c_str());
    if (cart->type == CART_TYPE_GMOD2) {
        if (getFpgaCapabilities() & CAPAB_EEPROM) {
            b.format("EEPROM:       %s\n", c64->get_eeprom_dirty() ? "Dirty (Needs save!)" : "Clean");
        } else {
            b.format("EEPROM:       Not Supported");
        }
    }

    hw_modules(b, cart->disabled, "Incompatible:");
    uint16_t enabled = 0;
    if (C64_REU_ENABLE) enabled |= CART_REU;
    if (CMD_IF_SLOT_ENABLE) enabled |= CART_UCI;
    if (C64_SAMPLER_ENABLE) enabled |= CART_SAMPLER;

#if U64 // FIXME
    if (false) enabled |= CART_WMIRROR;
    if (false) enabled |= CART_DYNAMIC;
#endif
    if (C64_KERNAL_ENABLE) enabled |= CART_KERNAL;

    if (getFpgaCapabilities() & CAPAB_ACIA) {
        uint8_t acia_en, acia_nmi;
        uint16_t acia_addr;
        acia.GetHwMapping(acia_en, acia_addr, acia_nmi);
        if (acia_en && !(acia_addr & 0x100)) enabled |= CART_ACIA_DE;
        if (acia_en && (acia_addr & 0x100)) enabled |= CART_ACIA_DF;
    }

    hw_modules(b, enabled, "Now Enabled:");

    b.format("\n");
}

char *SystemInfo :: size_expression(uint64_t a, uint64_t b, char *buf)
{
    const char *scales[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
    uint64_t bytes = a * b;
    int scale = 0;
    while(bytes >= 10000) {
        bytes /= 1024;
        scale++;
    }
    sprintf(buf, "%4d %s", (uint32_t)bytes, scales[scale]);
    return buf;
}

void SystemInfo :: storage_info(StreamTextLog& b)
{
    FileManager *fm = FileManager :: getFileManager();
    IndexedList<FileInfo *> dir(8, NULL);
    Path path; // defaults to Root directory
    fm->get_directory(&path, dir, NULL);
    char buffer[16];

    for(int i=0; i<dir.get_elements(); i++) {
        FileInfo *inf = dir[i];
        path.cd("/");
        path.cd(inf->lfname);
        uint32_t free_clusters, cluster_size;
        FRESULT fres = fm->get_free(&path, free_clusters, cluster_size);
        if (fres == FR_OK) {
            b.format("%14s%s Free\n", inf->lfname, size_expression(free_clusters, cluster_size, buffer));
        } else {
            b.format("%14s%s\n", inf->lfname, FileSystem :: get_error_string(fres));
        }
    }

    // The list is a local, but the FileInfos in it are ours to free.
    for(int i=0; i<dir.get_elements(); i++) {
        delete dir[i];
    }
}

void SystemInfo :: generate(UserInterface *ui)
{
    StreamTextLog buffer(4096);
    char buf[40];

    buffer.format("System Information\n");
    buffer.format("==================\n");
    buffer.format("Date: %s\n", rtc.get_long_date(buf, 40));
    buffer.format("Time: %s\n\n", rtc.get_time_string(buf, 40));

    buffer.format("Version Information:\n");
    buffer.format("====================\n");
    buffer.format("Software version:   " APPL_VERSION_ASCII "\n");
    buffer.format("   Build date:      " APP_BUILD_DATE "\n");
    buffer.format("   Build machine:   " APP_BUILD_MACHINE "\n");
    buffer.format("Ultimate FPGA core: 1%02X\n", getFpgaVersion());
    if(getFpgaCapabilities() & CAPAB_ULTIMATE64) {
        buffer.format("C64 FPGA version:   V1.%02X\n", C64_CORE_VERSION);
    }
    Flash *flash = get_flash();
    if (flash) {
        buffer.format("Flash Type: %s\n", flash->get_type_string());
    }
    buffer.format("Git tag:    " APP_VERSION_TAG "\n");
    buffer.format("Git branch: " APP_VERSION_BRANCH "\n");
    buffer.format("Git hash:   " APP_VERSION_HASH "\n");
    buffer.format("Git date:   " APP_VERSION_DATE "\n\n");
    
    buffer.format("Drive Status:\n");
    buffer.format("=============\n");
    if (c1541_A) {
        drive_info(buffer, c1541_A, 'A');
    }
    if (c1541_B) {
        drive_info(buffer, c1541_B, 'B');
    }
    IecInterface::info(buffer);

    buffer.format("Cartridge Information:\n");
    buffer.format("======================\n");
    cart_info(buffer);

    buffer.format("Storage Devices:\n");
    buffer.format("================\n");
    storage_info(buffer);

    ui->run_editor(buffer.getText(), buffer.getLength());
}



#if U64 == 1 && !defined(RECOVERYAPP)
#include "menu.h"
#include "subsys.h"

// Register with the existing task menu; the text viewer handles scrolling/back.
// Remains available on demand rather than interrupting startup.
class UceAboutMenu : public ObjectWithMenu {
    static SubsysResultCode_e show(SubsysCommand *cmd) {
        if (!cmd || !cmd->user_interface) return SSRET_INVALID_PARAMETER;
        StreamTextLog buffer(3072);
        buffer.format("Ultimate Community Edition\n");
        buffer.format("UCE " APPL_VERSION_ASCII "\n\n");
        buffer.format("Community firmware for the original\n");
        buffer.format("Ultimate 64 and Ultimate 64 Elite.\n");
        buffer.format("Independent community project.\n");
        buffer.format("Not an official Gideon release.\n\n");
        buffer.format("Acknowledgements\n================\n");
        buffer.format("Gideon Zweijtzer and contributors:\n");
        buffer.format("Ultimate hardware, FPGA platform,\n");
        buffer.format("and original 1541ultimate firmware.\n\n");
        buffer.format("SpiffyCrew / Nifty:\n");
        buffer.format("Spiffy Ultimate enhancements that\n");
        buffer.format("formed the basis of this adaptation.\n\n");
        buffer.format("Tim Waite and community testers:\n");
        buffer.format("UCE project direction and board tests.\n\n");
        buffer.format("Also see AUTHORS.txt, CREDITS.md and\n");
        buffer.format("component copyright/license notices.\n\n");
        buffer.format("Source repositories\n===================\n");
        buffer.format("github.com/GideonZ/1541ultimate\n");
        buffer.format("github.com/spiffycrew/Spiffy_Ultimate\n\n");
        buffer.format("License and warranty\n====================\n");
        buffer.format("Firmware: GNU GPL version 3.\n");
        buffer.format("Redistribution/modification permitted\n");
        buffer.format("under its terms. No warranty.\n");
        buffer.format("See LICENSE.txt in the source.\n");
        buffer.format("Bundled components retain their\n");
        buffer.format("respective licenses and notices.\n");
        cmd->user_interface->run_editor(buffer.getText(), buffer.getLength());
        return SSRET_OK;
    }
public:
    void create_task_items(void) {
        TasksCollection::getCategory("About UCE", 90)->append(
            new Action("Acknowledgements", show, 0, 0));
    }
};
static UceAboutMenu uceAboutMenu;
#endif

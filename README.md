# Ultimate Community Edition (UCE)

Community firmware for the **original Ultimate 64 and original Ultimate 64 Elite**, based on Ultimate firmware **v3.15a**.

**UCE 1.0.1.** Firmware identifier: `3.15a-UCE.2`. UCE is an independent community project, not an official Gideon or SpiffyCrew release. It retains the upstream hardware identification and update format.

## Supported hardware and status

| Hardware | Status |
| --- | --- |
| Original Ultimate 64 Elite, board 1.4a, C64 core V1.50 | Primary development/test board |
| Original Ultimate 64, non-Elite | Intended target; separate tester acceptance needed |
| Ultimate 64 Elite-II / U64-II / Commodore 64 Ultimate | Not supported by this UCE candidate |
| Ultimate-II / II+ / II+L cartridges | Not validated by this UCE candidate |

The inherited repository contains other build targets. Their presence does not mean UCE supports them. Build **`make u64`**, not the default all-products target.

This release passed focused native checks and was compiled successfully with the supported toolchain on jiffyplex. The maintainer reports initial operation on the original Elite; broader regression testing remains ongoing. See [VALIDATION.md](VALIDATION.md) for current status.

## Features

- **Assembly64-compatible search:** choose public or local servers, edit server definitions and API paths, browse search results and copy files to local storage. Responses support up to 64 KiB and 6,000 JSON tokens within explicit limits.
- **Wi-Fi management:** connection progress and feedback, status/reconnect tools, scan/select an access point and manage saved access points.
- **Network information:** DHCP/static addressing, gateway, netmask and active DNS resolver addresses.
- **FTP file access:** server profiles, connection tests, reconnect/disconnect, passive directory browsing, uploads/downloads and transfer status. Read-only streaming on SIZE-capable servers removes the RAM-disk staging limit for REU loads and FTP-to-local copies; writable opens and legacy servers still use temporary storage.
- **File management:** explicit copy/move destination selection and menu navigation with keyboard shortcuts.
- **Interface:** configurable colours and optional branding, readable multiline dialogs and an acknowledgement viewer.
- **Machine control:** C=+F power-off from the file browser using the board's existing power control; C=+O opens the machine-code monitor.

UCE also retains upstream capabilities including disk/cartridge emulation, the HTTP API and FPGA streaming. See [USER_GUIDE.md](USER_GUIDE.md) for everyday operation and [CHANGELOG.md](CHANGELOG.md) for the initial release summary. Credits are in [CREDITS.md](CREDITS.md).

## Get the source

Clone this repository with submodules after it has been published:

```bash
git clone --recurse-submodules https://github.com/omnicron-c64/ultimate-community-edition.git ultimate-community-edition
cd ultimate-community-edition
git submodule update --init --recursive
```

For repository setup from this source package, see [GITHUB_SETUP.md](GITHUB_SETUP.md). Preserve the upstream history when publishing.

## Build tools

| Tool | Purpose | Version used for this project |
| --- | --- | --- |
| Quartus Prime Lite + Nios-II EDS | Nios-II C/C++ compiler/binutils and programming-file conversion tools | 18.1.0 build 625; GCC 5.3.0 |
| ESP-IDF | Builds the ESP32 Wi-Fi bridge included in the updater | v5.3.1 |
| GNU make, Git, Python 3, native C/C++ tools | Build orchestration, generated files and host utilities | Linux development host |

Install the Intel/Altera tools separately from the vendor's download service. The FPGA image for the original U64 is supplied by upstream; this procedure does not rebuild its closed FPGA core. Do not replace it with another board's image.

Install ESP-IDF in a separate directory if needed:

```bash
mkdir -p "$HOME/esp"
git clone --branch v5.3.1 --recursive https://github.com/espressif/esp-idf.git "$HOME/esp/esp-idf"
cd "$HOME/esp/esp-idf"
./install.sh esp32
```

For an existing installation, keep it and load its environment. On the development host the tools are installed under `/root/intelFPGA_lite/18.1` and `/root/esp/esp-idf`:

```bash
export QUARTUS_ROOTDIR="$HOME/intelFPGA_lite/18.1/quartus"
export PATH="$QUARTUS_ROOTDIR/bin:$HOME/intelFPGA_lite/18.1/nios2eds/bin:$HOME/intelFPGA_lite/18.1/nios2eds/bin/gnu/H-x86_64-pc-linux-gnu/bin:$PATH"
source "$HOME/esp/esp-idf/export.sh"

quartus_sh --version
nios2-elf-gcc --version
idf.py --version
```

Return to this repository, then build in Bash:

```bash
make u64_clean && (
    set -o pipefail
    make u64 2>&1 | tee uce-build.log
    build_status=${PIPESTATUS[0]}
    echo "Build exit status: $build_status"
    exit "$build_status"
)
```

Continue only after exit status **0** and successful build size checks:

```bash
ls -lh update.u64
sha256sum update.u64
tail -25 uce-build.log
```

The result is **`update.u64`** in the repository root. ESP32 bridge identity remains **V1.9**. Do not use `make u64_no_esp` as a substitute unless matching ESP build artifacts already exist. Legacy upstream tool instructions remain in [README.txt](README.txt); this README describes the UCE target.

## Update and test

1. Preserve your working updater and important configuration files.
2. Copy the newly built `update.u64` to USB and open it using the original U64's normal firmware update workflow.
3. Confirm the updater identifies the intended board, complete the update, then verify `3.15a-UCE.2` and the Wi-Fi bridge version in the running system.
4. Check normal boot, menus/settings, Wi-Fi reconnection, DNS status, Assembly64 search/copy/reopen, FTP downloads/uploads and acknowledgement-screen return. See [VALIDATION.md](VALIDATION.md).

This `.u64` updater installs into flash. It is distinct from the RAM-only `.cfw` overlay experiment. Flash recovery procedures depend on board revision and failure mode; retain access to the upstream recovery information and your known-working build.

## Configuration and useful menus

- **F6:** Assembly64-compatible search.
- **F5 → Assembly 64 → Edit search servers:** edit server definitions.
- **WiFi → WiFi status / reconnect:** inspect addressing and reconnect; manage saved APs from that screen.
- **F4:** System Information. **F5 → About UCE → Acknowledgements:** project credits.
- Search server file: `/Flash/config/server.json`. Optional colours/title file: `/Flash/config/branding.json`. Files in this repository's `config/` directory are examples; do not overwrite a working configuration unnecessarily.

The default Assembly64 client ID remains `Spiffy` for protocol compatibility. It is independent of the UCE product name and can be edited per server. Plain HTTP and plain FTP remain the supported protocols; this change does not add HTTPS, FTPS or SFTP.

## Credits and license

Thanks to **Gideon Zweijtzer and all upstream contributors**, **SpiffyCrew / Nifty**, and **Tim Waite and community testers**. See [CREDITS.md](CREDITS.md), [AUTHORS.txt](AUTHORS.txt) and [PROVENANCE.md](PROVENANCE.md).

The firmware retains the upstream **GNU GPL v3** license in [LICENSE.txt](LICENSE.txt). Bundled components retain their own notices/licenses. Rebranding does not remove those notices. Supplied FPGA images and third-party ROMs are not relicensed by UCE. No warranty is provided.

For bug reports, include the board revision, core/firmware/bridge versions, exact reproduction steps and whether Ethernet or Wi-Fi is in use. Remove passwords and other credentials from logs/configuration before sharing them.

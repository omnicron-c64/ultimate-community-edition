# Release notes

## UCE 1.0.1

Firmware identifier: `3.15a-UCE.2`. Supported-toolchain build succeeded; initial original-Elite operation reported by the maintainer. Detailed transfer verification remains ongoing.

- Stream read-only FTP files instead of first storing the whole file in the 16 MiB RAM disk. This permits full 16 MiB REU loads and larger FTP-to-local copies when the server supports SIZE.
- Isolate each open source on its own FTP connection; preserve seeking by skipping/replaying data with bounded memory.
- Validate advertised length, EOF and final FTP completion before a final read succeeds.
- Retain cached opens for writable files and servers without SIZE support; their temporary-storage limit remains.

## UCE 1.0.0

Initial Ultimate Community Edition release for the original Ultimate 64 and Ultimate 64 Elite, based on Ultimate firmware v3.15a.

Firmware identifier: `3.15a-UCE.1`. This source candidate awaits its final supported-toolchain build and hardware acceptance.

### Included functionality

- Assembly64-compatible searches with configurable public/local server profiles and API endpoints.
- Bounded search responses up to 64 KiB and 6,000 JSON tokens.
- Wi-Fi connection feedback, status/reconnect and saved access-point management.
- DHCP/static-IP status and active DNS information.
- Plain FTP server profiles, passive browsing, connection tools, file transfers and progress/status reporting.
- Copy/move destination selection, keyboard navigation and machine-control shortcuts.
- Configurable interface colours and optional branding.
- On-demand acknowledgements and product/system information.

Upstream disk, cartridge, networking and streaming capabilities remain available. See README.md and USER_GUIDE.md for supported hardware, operation and current limits.

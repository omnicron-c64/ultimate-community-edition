# Using Ultimate Community Edition

UCE 1.0.1 is designed for the original Ultimate 64 and Ultimate 64 Elite. Firmware identifier: `3.15a-UCE.2`.

## Menu and machine controls

| Control | Action |
| --- | --- |
| F2 | Settings |
| F3 | Keyboard help |
| F4 | System Information |
| F5 | Action menu |
| F6 | Assembly64-compatible search |
| RUN/STOP | Back/close |
| C=+F in the file browser | Board power-off action |
| C=+O | Machine-code monitor |

Use F5 → About UCE → Acknowledgements for credits. C64 hardware identification and core version remain visible in System Information.

## Search services

Use F5 → Assembly 64 → Edit search servers to configure a public or local Assembly64-compatible service. Profiles contain a name, host, port, client ID and search/preset/entry/download paths. A local service must implement the compatible API; an ordinary directory web server is insufficient.

Profiles are stored in `/Flash/config/server.json`. The default client ID remains `Spiffy` for server compatibility and is independent of the product name. HTTP is supported; HTTPS is not implemented in this client.

To copy a search result, choose Copy to, navigate to the destination and explicitly select the copy-into-this-folder action. RUN/STOP cancels the destination picker. After completion, inspect the destination for the downloaded file.

## Wi-Fi

Open WiFi status / reconnect from the WiFi menu. Connection information includes association/IP readiness, DHCP or static addressing, gateway, netmask, active DNS and module identification. Refresh/reopen the screen for updated information; it is not a continuously updating dashboard.

Saved access-point management lets you select a stored AP or forget it. To replace a password, connect through scan/select or manual AP entry. Saved credentials reside in the ESP32 module. The active DNS display reports the shared resolver state; it does not create a separate DHCP DNS override.

## FTP

Create or edit a profile under `/ftp` with server name, host, port, user, password and starting directory. Password summaries show whether a value is set; typed entry is not newly masked or encrypted storage.

Test connection checks login and directory data access. Reconnect establishes a fresh session. Disconnect closes the session; later operations may connect again. Status shows the current/last operation and transfer count. Refresh requests a directory reload; re-enter the server when necessary.

Browsing supports passive EPSV/PASV and MLSD with common LIST fallback. Listings are bounded to 4,096 entries. In UCE 1.0.1, read-only files stream directly when the server supports SIZE: full 16 MiB REU images and larger copies to USB do not need to fit in `/Temp`. Each source uses a private FTP connection. Backward seeks restart and replay the download, so remote disk-image browsing can be slower than local storage. Writable opens/uploads and servers that explicitly reject SIZE still use temporary storage; the approximately 16 MiB RAM disk limit applies to those paths, including large FTP-to-FTP copies. Copies/uploads report transfer stages and errors.

Plain FTP only: no FTPS or SFTP. Active socket operations cannot currently be cancelled interactively. Uploads are not atomic replacement and a failed upload may leave a partial remote file. Use test files when evaluating transfers.

## Appearance

Choose colours in User Interface Settings. Optional titles and colours can be supplied in `/Flash/config/branding.json`; `config/branding.json` is an example, not an automatically installed replacement for your configuration.

Interface Type selects Freeze or Overlay on HDMI. The integrated menu remains 40 columns. This release does not include the separate experimental 80-column workspace.

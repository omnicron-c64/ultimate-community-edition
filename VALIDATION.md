# Validation status

## UCE 1.0.0 source candidate

- Scope: the initial UCE functionality described in README.md and USER_GUIDE.md.
- Native regression coverage: destination picker/search lifecycle, HTTP response bounds and hex editor, Wi-Fi replies/scan/connection feedback, saved AP/server editor persistence, and FTP listing/lifecycle/loopback transfers.
- The new acknowledgement action uses the existing ObjectWithMenu registration and synchronous text viewer. Full target build and acknowledgement screen acceptance remain pending on jiffyplex/the board.
- The actual new acknowledgement class compiled and ran against native UI adapters: registration, viewer invocation, upstream/Spiffy credit text, warranty text and missing-UI handling passed. The optional branding example parses as strict JSON.
- Complete Nios-II/ESP-IDF build is not available in the preparation environment. No UCE 1.0.0 updater binary or target-build success is claimed.
- Sanitizer leak detection is disabled for native runs in the preparation container because its process restrictions prevent LeakSanitizer inspection; address and undefined-behavior checks remain enabled where the tests request them.

Popup/preview checks use the actual drawing functions and firmware formatter with bounded screen adapters: 40/80 columns, all standard button combinations, long connection errors, blank/CRLF lines, oversized text, control bytes, restoration, password previews and product-heading buffer canaries passed. Hardware rendering acceptance remains pending.

Commands from the repository root:

```bash
python3 software/test/popup_layout/run_tests.py
python3 software/test/picker_focus/run_tests.py
bash software/test/spiffy/run_host_tests.sh
ASAN_OPTIONS=detect_leaks=0 make -C target/pc/linux/wifireply test
ASAN_OPTIONS=detect_leaks=0 bash software/test/network_ui/run_tests.sh
python3 software/test/ftp_client/run_tests.py
```

## Hardware acceptance before a UCE release

1. Successful supported-toolchain build and application size checks.
2. Normal update and boot on original Elite; version `3.15a-UCE.1` and bridge V1.9.
3. Open/scroll/close acknowledgement viewer repeatedly; resume browser and settings normally. Check ordinary F4 system information.
4. FTP edit fields show actual values; failed connection/status popups wrap within the border and close cleanly.
5. Settings persistence, default heading and optional UCE branding JSON; no automatic replacement of user branding.
6. Wi-Fi cold boot/reconnect, correct/incorrect password feedback, scan names, saved AP selection/removal.
7. DHCP and static-IP status, active DNS display, Assembly64 and compatible local server searches.
8. Assembly64 destination copy/confirmation/cancel, actual downloaded file, repeated close/reopen, and prevention of F6 escaping a modal picker.
9. FTP server edit/test/reconnect, listings, copy/download/upload and visible failure handling. Use test files for upload/error cases.
10. Repeat on original non-Elite board before marking that hardware tested.

## Known boundaries

Plain HTTP/FTP only. FTP cancellation during an active socket operation is not implemented; remote uploads are not atomic replacement. Status pages update on actions/reopening. Active DNS is the shared resolver state, not a per-interface DHCP lease audit. The RAM-only 80-column experiment is separate from this firmware's menu. There is no UCE module loader, SSH service or browser in this candidate.

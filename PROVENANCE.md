# Source provenance

| Input | Revision |
| --- | --- |
| Ultimate firmware | https://github.com/GideonZ/1541ultimate |
| Upstream baseline | tag `v3.15a`, commit `dddd29b25d64c79d3a8cab95d8b86e1f8cafb816` |
| Baseline git date | 2026-09-12 08:38:38 +0200 |
| Target C64 core | V1.50, using the original upstream FPGA image |
| Spiffy donor | https://github.com/spiffycrew/Spiffy_Ultimate |
| Donor branch/revision | `spiffy-master-1.1.0`, `3b582da78c57fb5b2d53bacb6a64dea5a4535f66` |
| Donor comparison base | `7b628eb1` |
| HTTP submodule | `ad73d3217bb0e1b3f37e0dbba595309b942aea17` |
| lwIP submodule | `26a22151f4b7ebb2925523192edc83fa7f31cba9` |

The adaptation changes Nios-II firmware and the original-U64 ESP32 Wi-Fi bridge. It does not modify the FPGA bitstream. The firmware version is now `3.15a-UCE.1`; the bridge retains `ESP32 WiFi Bridge V1.9`.

The initial UCE source package is identified as UCE 1.0.0. Upstream and donor attribution, copyright notices and component licenses are retained. The public release notes describe the resulting functionality.

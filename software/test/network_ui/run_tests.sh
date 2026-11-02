#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
flags=(-std=c++11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
g++ "${flags[@]}" -Isoftware/wifi/raw_u64/main software/test/network_ui/saved_ap_test.cc -o "$build/saved_ap"
"$build/saved_ap"
g++ "${flags[@]}" -include stdio.h -DRUNS_ON_PC -ffunction-sections -fdata-sections -fpermissive -Wno-write-strings -Wl,--gc-sections \
 -Isoftware/api -Isoftware/components -Isoftware/system -Isoftware/io/stream -Isoftware/infra -Isoftware/userinterface \
 software/test/network_ui/server_editor_test.cc software/api/json.cc software/components/mystring.cc software/components/pattern.cc software/system/small_printf.cc software/io/stream/stream.cc -o "$build/server_editor"
"$build/server_editor"
g++ "${flags[@]}" -include stdio.h -DRUNS_ON_PC -ffunction-sections -fdata-sections -fpermissive -Wno-write-strings -Wl,--gc-sections \
 -Isoftware/api -Isoftware/components -Isoftware/system -Isoftware/io/stream -Isoftware/infra -Isoftware/userinterface \
 software/test/network_ui/server_file_test.cc software/api/json.cc software/components/mystring.cc software/components/pattern.cc software/system/small_printf.cc software/io/stream/stream.cc -o "$build/server_file"
"$build/server_file"

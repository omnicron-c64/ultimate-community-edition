#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
spiffy_test_dir=$(mktemp -d)
trap 'rm -rf "$spiffy_test_dir"' EXIT
g++ -std=c++11 -include stdio.h -DRUNS_ON_PC -DMHS_PORT=8080  -ffunction-sections -fdata-sections -fpermissive -Wno-write-strings -Wl,--gc-sections  -Isoftware/api -Isoftware/network -Isoftware/components -Isoftware/system  -Isoftware/io/stream -Isoftware/httpd/c-version/lib -Isoftware/infra  -Isoftware/filemanager -Isoftware/filesystem -Isoftware/chan_fat/ff14/source  software/test/spiffy/http_buffer_test.cc software/network/http_request.cc  software/api/json.cc software/components/mystring.cc software/components/pattern.cc  software/system/small_printf.cc software/io/stream/stream.cc -o "$spiffy_test_dir/http_buffer_test"
"$spiffy_test_dir/http_buffer_test"
mkdir -p target/pc/linux/hexeditortest/{output,result}
make -C target/pc/linux/hexeditortest
target/pc/linux/hexeditortest/result/hexeditortest

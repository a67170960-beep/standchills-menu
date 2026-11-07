#!/bin/bash

# ================================================================================
# Telegram community:
# Bot: @imguisource_bot
# ImGui Source: @ImGui_Source_FIXNEY
# shit src 🧨: @il2cpp_src
# ================================================================================

set -e

cd "$(dirname "$0")"

NDK="${ANDROID_NDK_HOME:-/root/Android/Sdk/ndk/android-ndk-r23b}"
PREBUILT="$(ls "$NDK/toolchains/llvm/prebuilt/" | head -1)"
CXX="$NDK/toolchains/llvm/prebuilt/$PREBUILT/bin/aarch64-linux-android24-clang++"
NDKBUILD="$NDK/ndk-build"

mkdir -p build/generated

python3 - <<'PY'
from pathlib import Path

source = Path("Include/text/InterVariable.ttf")
target = Path("build/generated/inter_font.hpp")

data = source.read_bytes()

lines = []
for i in range(0, len(data), 16):
    lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[i:i + 16]) + ",")

header = "#pragma once\n\nstatic const unsigned char inter_font_data[] = {\n"
header += "\n".join(lines)
header += "\n};\n"

target.write_text(header)
print(f"[+] Generated Inter font: {len(data)} bytes")
PY

"$CXX" -std=c++17 -O2 -fPIE -pie -static-libstdc++ \
-o build/inj Inj/inj.cpp -ldl

"$NDKBUILD" \
NDK_PROJECT_PATH=. \
APP_BUILD_SCRIPT="$(pwd)/Jni/Android.mk" \
NDK_APPLICATION_MK="$(pwd)/Jni/Application.mk" \
NDK_OUT="$(pwd)/obj" \
NDK_LIBS_OUT="$(pwd)/libs" \
-j"$(nproc)"

cp libs/arm64-v8a/libfreecamk.so build/freecamk.so

chmod 755 build/inj build/freecamk.so

echo "[+] build/inj"
echo "[+] build/freecamk.so"

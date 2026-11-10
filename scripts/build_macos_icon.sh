#!/bin/bash
set -euo pipefail

source_png="$1"
output_icns="$2"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
iconset="$work_dir/StereoMask.iconset"
mkdir "$iconset"
rounded_png="$work_dir/rounded.png"
/usr/bin/xcrun swift "$(dirname "$0")/round_macos_icon.swift" "$source_png" "$rounded_png"
for size in 16 32 128 256 512; do
    double_size=$((size * 2))
    /usr/bin/sips -z "$size" "$size" "$rounded_png"         --out "$iconset/icon_${size}x${size}.png" >/dev/null
    /usr/bin/sips -z "$double_size" "$double_size" "$rounded_png"         --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
/usr/bin/iconutil -c icns "$iconset" -o "$work_dir/StereoMaskRounded.icns"
mv "$work_dir/StereoMaskRounded.icns" "$output_icns"

#!/bin/bash
set -euo pipefail
trap 'echo "Update stopped. The existing installed app is retained unless installation already succeeded."' ERR
STEREOMASK_DEVELOPMENT="/Applications/StereoMask Development"
STEREOMASK_DOWNLOAD="$(mktemp -d -t stereomask-update)"
trap 'rm -rf "$STEREOMASK_DOWNLOAD"' EXIT
echo "Downloading the current Mac source into a temporary folder…"
/usr/bin/curl --fail --location --retry 2 \
    "https://github.com/TedInVR/StereoMask-Mac/archive/refs/heads/main.zip" \
    --output "$STEREOMASK_DOWNLOAD/source.zip"
/usr/bin/ditto -x -k "$STEREOMASK_DOWNLOAD/source.zip" "$STEREOMASK_DOWNLOAD/extracted"
STEREOMASK_SOURCE="$STEREOMASK_DOWNLOAD/extracted/StereoMask-Mac-main"
test -f "$STEREOMASK_SOURCE/CMakeLists.txt"
test -f "$STEREOMASK_SOURCE/appversion.h"
test -f "$STEREOMASK_SOURCE/scripts/repair_macos_bundle.py"
test -f "$STEREOMASK_SOURCE/scripts/install_macos_app.py"
mkdir -p "$STEREOMASK_DEVELOPMENT"
# Merge only the repository source. Local build logs and projects stay in place.
/usr/bin/ditto "$STEREOMASK_SOURCE" "$STEREOMASK_DEVELOPMENT"
echo "Source updated. Building and installing the permanent app…"
bash "$STEREOMASK_DEVELOPMENT/BUILD_BRUSH_VERSION.command"

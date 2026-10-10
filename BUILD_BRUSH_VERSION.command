#!/bin/bash
set -euo pipefail
trap 'echo; echo "Build stopped. Send the last error lines to ChatGPT."; read -r -p "Press Return to close." || true' ERR
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:$PATH"
STEREOMASK_SDK="$(xcrun --show-sdk-path)"
test -f "$STEREOMASK_SDK/usr/include/c++/v1/type_traits"
cmake -S . -B build-mac-brush -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$STEREOMASK_SDK" -DCMAKE_CXX_FLAGS="-isystem $STEREOMASK_SDK/usr/include/c++/v1" -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase;/opt/homebrew/opt/opencv"
STEREOMASK_APP="$PWD/build-mac-brush/StereoMask.app"
# Restore the actual binary before rebuilding/redeploying a previous R2 build.
if [ -f "$STEREOMASK_APP/Contents/MacOS/StereoMask-bin" ]; then
    mv -f "$STEREOMASK_APP/Contents/MacOS/StereoMask-bin" "$STEREOMASK_APP/Contents/MacOS/StereoMask"
fi
cmake --build build-mac-brush --parallel 4
export DYLD_FRAMEWORK_PATH="/opt/homebrew/opt/qtbase/lib:/opt/homebrew/opt/qtsvg/lib"
STEREOMASK_APP="$PWD/build-mac-brush/StereoMask.app"
/opt/homebrew/opt/qtbase/bin/macdeployqt "$STEREOMASK_APP" -always-overwrite -libpath=/opt/homebrew/opt/qtsvg/lib 2>&1 | tee build-mac-brush/deployment.log
if grep -q '^ERROR:' build-mac-brush/deployment.log; then
    echo "Packaging reported errors. Send deployment.log to ChatGPT."; exit 1
fi
unset DYLD_FRAMEWORK_PATH
# Rewrite all Homebrew aliases to bundled libraries, then verify and ad-hoc sign.
python3 scripts/repair_macos_bundle.py "$STEREOMASK_APP" 2>&1 | tee build-mac-brush/bundle-repair.log

echo "Packaging verified. Starting StereoMask Mac R10…"
env -u DYLD_LIBRARY_PATH -u DYLD_FRAMEWORK_PATH -u DYLD_FALLBACK_LIBRARY_PATH \
    "$STEREOMASK_APP/Contents/MacOS/StereoMask" > build-mac-brush/startup.log 2>&1 &
STEREOMASK_PID=$!
sleep 2
if ! kill -0 "$STEREOMASK_PID" 2>/dev/null; then
    echo "The app stopped during startup. Here is its log:"
    cat build-mac-brush/startup.log
    exit 1
fi
open -R "$STEREOMASK_APP"
echo "App is running. Help > User Guide opens the guide in your browser."
echo "After testing, copy StereoMask.app into Applications."
read -r -p "Press Return to close this Terminal window."

#!/bin/bash
set -euo pipefail
trap 'echo; echo "Build stopped. Send the last error lines to ChatGPT."; read -r -p "Press Return to close." || true' ERR
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:$PATH"
if /bin/ps -axo comm= | /usr/bin/grep -Eq '/Contents/MacOS/StereoMask$'; then
    echo "Save your project and quit all StereoMask windows before building/installing."
    exit 1
fi
# Qt plugins need these at deployment time, even when compilation succeeds.
if [ ! -f /opt/homebrew/opt/openssl@4/lib/libssl.4.dylib ] ||    [ ! -f /opt/homebrew/opt/openssl@4/lib/libcrypto.4.dylib ]; then
    brew install openssl@4
    brew link openssl@4
fi
if [ ! -f /opt/homebrew/opt/qtsvg/lib/QtSvg.framework/Versions/A/QtSvg ]; then
    brew install qtsvg
    brew link qtsvg
fi
for STEREOMASK_REQUIRED in     /opt/homebrew/opt/openssl@4/lib/libssl.4.dylib     /opt/homebrew/opt/openssl@4/lib/libcrypto.4.dylib     /opt/homebrew/opt/qtsvg/lib/QtSvg.framework/Versions/A/QtSvg; do
    if [ ! -f "$STEREOMASK_REQUIRED" ]; then
        echo "Homebrew dependency is still missing: $STEREOMASK_REQUIRED"
        echo "Run brew reinstall openssl@4 qtsvg, then run this script again."
        exit 1
    fi
done
STEREOMASK_SDK="$(xcrun --show-sdk-path)"
test -f "$STEREOMASK_SDK/usr/include/c++/v1/type_traits"
cmake --fresh -S . -B build-mac-brush -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$STEREOMASK_SDK" -DCMAKE_CXX_FLAGS="-isystem $STEREOMASK_SDK/usr/include/c++/v1" -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase;/opt/homebrew/opt/opencv"
STEREOMASK_APP="$PWD/build-mac-brush/StereoMask.app"
# Restore the actual binary before rebuilding/redeploying a previous R2 build.
if [ -f "$STEREOMASK_APP/Contents/MacOS/StereoMask-bin" ]; then
    mv -f "$STEREOMASK_APP/Contents/MacOS/StereoMask-bin" "$STEREOMASK_APP/Contents/MacOS/StereoMask"
fi
cmake --build build-mac-brush --parallel 4
# Explicitly bundle QtSvg for our post-deployment dependency repair.
mkdir -p "$STEREOMASK_APP/Contents/Frameworks"
/usr/bin/ditto /opt/homebrew/opt/qtsvg/lib/QtSvg.framework     "$STEREOMASK_APP/Contents/Frameworks/QtSvg.framework"
# Ensure SVG plugins are included even if macdeployqt's resolver skips them.
for STEREOMASK_PLUGIN_ROOT in /opt/homebrew/opt/qtsvg/share/qt/plugins /opt/homebrew/opt/qtsvg/plugins /opt/homebrew/share/qt/plugins; do
    for STEREOMASK_PLUGIN in imageformats/libqsvg.dylib iconengines/libqsvgicon.dylib; do
        if [ -f "$STEREOMASK_PLUGIN_ROOT/$STEREOMASK_PLUGIN" ]; then
            mkdir -p "$STEREOMASK_APP/Contents/PlugIns/$(dirname "$STEREOMASK_PLUGIN")"
            /usr/bin/ditto "$STEREOMASK_PLUGIN_ROOT/$STEREOMASK_PLUGIN" "$STEREOMASK_APP/Contents/PlugIns/$STEREOMASK_PLUGIN"
        fi
    done
done
# macdeployqt follows the executable's LC_RPATH entries during discovery.
STEREOMASK_SVG_PATH="/opt/homebrew/opt/qtsvg/lib"
if ! /usr/bin/otool -l "$STEREOMASK_APP/Contents/MacOS/StereoMask" | /usr/bin/grep -Fq "path $STEREOMASK_SVG_PATH (offset"; then
    /usr/bin/install_name_tool -add_rpath "$STEREOMASK_SVG_PATH" "$STEREOMASK_APP/Contents/MacOS/StereoMask"
fi
export DYLD_FRAMEWORK_PATH="/opt/homebrew/opt/qtbase/lib:/opt/homebrew/opt/qtsvg/lib"
STEREOMASK_APP="$PWD/build-mac-brush/StereoMask.app"
if /opt/homebrew/opt/qtbase/bin/macdeployqt "$STEREOMASK_APP" -always-overwrite -libpath=/opt/homebrew/opt/qtsvg/lib 2>&1 | tee build-mac-brush/deployment.log; then
    STEREOMASK_DEPLOY_STATUS=0
else
    STEREOMASK_DEPLOY_STATUS=1
fi
# QtSvg is explicitly bundled. Defer only its resolver message and old
# signatures; the repair below must resolve every dependency and sign strictly.
python3 scripts/check_deployment.py build-mac-brush/deployment.log "$STEREOMASK_DEPLOY_STATUS" "$STEREOMASK_APP"
unset DYLD_FRAMEWORK_PATH
# Rewrite all Homebrew aliases to bundled libraries, then verify and ad-hoc sign.
python3 scripts/repair_macos_bundle.py "$STEREOMASK_APP" 2>&1 | tee build-mac-brush/bundle-repair.log

python3 scripts/install_macos_app.py "$STEREOMASK_APP" /Applications/StereoMask.app
STEREOMASK_APP="/Applications/StereoMask.app"
echo "Packaging verified. Starting the permanent StereoMask Mac…"
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
echo "Updates now replace /Applications/StereoMask.app. Use your existing Dock icon."
read -r -p "Press Return to close this Terminal window."

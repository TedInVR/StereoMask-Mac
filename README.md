# StereoMask Mac

StereoMask is a collaborative effort between **Subbiah Rathinagiri** and **Theodore Whitten**. This repository contains the Mac port and foreground-editing improvements based on StereoMask 2.0.0, now incorporating Subbiah’s Windows v2.3.0 additions.

- **Subbiah Rathinagiri (S. Rathinagiri):** original StereoMask author and upstream PC/Windows development.
- **Theodore Whitten:** Mac port, editing workflow and foreground-selection improvements, developed with Codex assistance.
- **Upstream:** [SRathinaGiri/StereoMask](https://github.com/SRathinaGiri/StereoMask).

The original MIT copyright and license are preserved in [LICENSE](LICENSE). Windows v2.3.0 incorporates the Mac workflow and creative tools plus new Windows enhancements. Mac R20 / 2.3.1 brings the new color selection, text effects, geometry fixes and processing feedback back to Mac. Original creative inspiration: [Bernd Paksa’s Stereomasken](https://www.stereomasken.de/index_en.htm).

## Voluntary support

Help → Support StereoMask opens an optional nonmodal panel, also available through in-window Help and Main Menu → Help. Its exact public links support Ted in VR's Mac development and other creative projects:

- [Ko-fi — Leave a Tip](https://ko-fi.com/tedwhitten)
- [PayPal — Make a Donation](https://paypal.me/theodorewhitten1)
- [Patreon — Become a Supporter](https://patreon.com/TedWhitten)

No embedded payments, startup requests, ads, editing interruptions or paid app features. Links open only after user activation. Payment/payout processing and Patreon membership availability have not been tested. Subbiah's original authorship and collaborative credit remain unchanged.

## Features

- Fuzzy Select (connected color region) and Select by Color Threshold, with tolerance, per-eye Add/Subtract History objects and Undo.
- Flat/Embossed/Engraved text, colored letters/cutout plates and X/Y/Z stereo orientation, persistent with old-project defaults.
- Directional alignment uses selected extremes; Align Depth uses median disparity. Near-edge insertion and closing-edge order fixes.
- Opaque processing messages and worker snapshots; applied text caches shared by preview/export.
- Optional SBS Stereo Window preview; Mac retains anaglyph and wheel zoom defaults. View can enable editing-wheel panning.
- appversion.h supplies runtime, CMake project and macOS bundle version.


- Create Frame Shapes: rectangle, square, oval and circle; drag/resize, stereo depth, editable cubic dots and curve-preserving point insertion.
- Drop Shadow: opacity, distance, direction, softness, spread and color; per-eye background composition and transparent PNG.
- Text & Bubbles: system fonts, editable multiple text objects, stereo depth, speech/thought styles, draggable size/tail, fill opacity and optional foreground occlusion; saved projects and Undo/Redo.


- First-step Stereo Window: isolated anaglyph slider, precise pixels, Apply/Cancel/Reset, Undo/Redo and saved offset; original photo stays untouched.

- Launch requests foreground activation and opens maximized with the Mac menu bar available.
- Close Project in File and its toolbar section (Command+W); Stereo Window has its own section.
- Guided Process: step-by-step dock for source depth, frame creation, both-eye foreground selections, review and export.
- Help includes readable Keyboard Shortcuts and an editor with saved preferences, conflict checks and Restore Defaults; drawing gestures remain fixed.
- Main Menu reaches every control group; Stereo Window opens directly. Open Recent and last-folder persistence are available.
- Logical groups: File, View, Stereo Window, Create Frame, Edit Frame, Out-of-Frame, History, Layout and Help.
- Compact in-window controls: independent collapsible sections, Show/Hide all, complete menu access in full screen, and a persistent History toggle.
- Frame point insertion at the nearest edge preserves existing vertices.
- Picture Frame: customizable color and width, Flat/Beveled/Decorative Trim styles; preserved foreground covers the border.
- Stereo Window: Push back/Pull forward buttons, dialog-wide Left/Right keys and larger pixel arrows.
- Editable stereo-window frame points, curves and disparity/depth; three AutoMask choices and Freehand.
- Select/Erase brushes, brush-circle cursor and bracket-key sizing.
- Regular and optional Smart polygon lassos, Add/Subtract, point insertion/removal and last-segment Delete.
- Independent selection objects: rename, hide, delete, reorder, reopen points, switch operation and Undo/Redo.
- Whole-polygon drag with horizontal constraint; optional estimated opposite-eye pairing creates independent objects.
- Ordered frame composition, including holes inside the frame and later additions.
- Source editing, Preview Result and Clean Preview; matching preview/export, transparent checkerboard and composed anaglyph.
- Pointer zoom, shared pan, arrow scrolling, Command+0 fit and Command+Plus/Minus zoom.
- Transparent/black/white/custom cutouts and Match preview; synchronized mask appearance and source eyedropper.
- Windows v2.1.1 object-format .msk persistence and older flattened-mask migration.
- Offline browser User Guide and manual Mac GitHub update checking.
- Build/install script replaces the permanent `/Applications/StereoMask.app`.

[Read the User Guide](docs/user-guide.html) by downloading and opening the HTML file in your browser. The in-app Help command opens the same guide.

## Build on Apple Silicon

R18 is source code, not a prebuilt download. It targets Apple Silicon (arm64). Intel builds and minimum compatible macOS versions have not been validated.

Install Xcode Command Line Tools and Homebrew, then the build dependencies:

```bash
brew install cmake qtbase qtsvg openssl@4 opencv python
```

Download this repository with Code → Download ZIP. Put its source contents in `/Applications/StereoMask Development` (retain your existing build directory and any personal projects). Save and quit all StereoMask windows, then run:

```bash
bash "/Applications/StereoMask Development/BUILD_BRUSH_VERSION.command"
```

For later updates, run `bash "/Applications/StereoMask Development/UPDATE_STEREOMASK.command"`. It downloads the current public Mac source to a temporary folder, updates this same development folder, then builds and installs. It does not create another Downloads revision folder. It replaces repository source files, so keep any personal code changes in a separate checkout.

From a Git checkout, run `bash BUILD_BRUSH_VERSION.command` in its source directory.
The script configures CMake, builds, deploys Qt, repairs dependency references, verifies the bundle and ad-hoc signs it. It stops on deployment or startup errors. It stages and verifies the new bundle, replaces `/Applications/StereoMask.app` and launches that permanent copy. A failed install verification restores the previous installed bundle. Keep the same Dock icon. If Applications is not writable, the installer stops and you can use Finder to copy the built app and choose Replace. CMake’s cache is regenerated so a moved development folder does not retain its old location.

### R18 status

R18 imports the upstream v2.1.1 core and UI, with Mac shortcuts, guide, updater endpoint and build packaging preserved. Cross-platform regression results are recorded in DEVELOPMENT_LOG.md. Actual Apple Silicon packaging, Finder launch, physical Command-click/Return delivery and relocation still need Mac confirmation for this revision. Paired alignment is an estimate; check the opposite-eye outline and adjust manually. Smart lasso remains optional.

The dependency repair redirects non-system loads into the bundle, checks for duplicate OpenMP and re-signs. It does not bypass duplicate-runtime checks.
Ad-hoc signing is local signing; this is not an Apple Developer ID signed or notarized public binary. Do not label a binary release ready until both Finder launch and relocation to Applications have been verified. Third-party binary notices/licenses must accompany any binary release; this source repository preserves the original app license.

Build logs: `build-mac-brush/deployment.log`, `bundle-repair.log`, `startup.log`.

## Other platforms and testing

The cross-platform Qt/C++ source remains compatible with the upstream Windows direction. CMake chooses OpenCV `stereo` for version 5 and `calib3d` for version 4. Windows packaging retains the existing upstream vcpkg configuration; the R18 packaging helper runs only on macOS.

```bash
cmake -S . -B build -DSTEREOMASK_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
```

Qt 6 Widgets/Gui/Core/Concurrent/Network and OpenCV core/imgproc/stereo (or calib3d) are required. The tests exercise per-eye masks, polygon completion, Add/Subtract, Undo, project restoration and view navigation. Packaging tests use simulated Mach-O dependency graphs; they do not replace an actual Mac launch test.

## Development history

[DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) is the cumulative technical handoff. [HISTORICAL_RELEASE_NOTES.md](HISTORICAL_RELEASE_NOTES.md) retains previous revision instructions; use the R18 instructions above for this build. `upstream-brush.patch` is a historical early brush patch, not the complete R18 changeset. Current source is authoritative.

## Publishing future updates

The app checks `updates.json` on the public `main` branch through GitHub's contents API. No login or token is included. Publish a new version by updating `appversion.h`, CMake's project/bundle version, visible revision labels and the development log, then publish `updates.json` with the same three-part numeric version. Its `page_url` must be an HTTPS page within this repository. `package_type` is `source` until a tested prebuilt app is actually published; then it may be `app` with a release-page URL. Keep useful plain-text `notes`.

The checker compares numeric version components, runs asynchronously, rejects invalid version/URL data, has request and total timeouts, handles HTTP/network failures and blocks overlapping checks. It opens a browser only after the user chooses Open GitHub; it does not overwrite or execute an app download. The checker opens GitHub after your click; it does not install updates by itself. Build the new source to update the permanent app.

## Planned guided workflow

Theodore requested an optional guided workflow that walks users through stereo-window adjustment, basic frame creation/editing, foreground selection and export, one step at a time with Skip/Next/accept choices. R18 implements Guided Process with Back/Next/Skip, live image editing, required-step checks, and saving/exporting.

Stereo-window offsets and picture-frame settings are Mac project extensions. Windows v2.1.1 and earlier Mac builds ignore them; exchange an adjusted exported pair or keep copies when using older apps.

# StereoMask Mac

StereoMask is a collaborative effort between **Subbiah Rathinagiri** and **Theodore Whitten**. This repository contains the Mac port and foreground-editing improvements based on StereoMask 2.0.0.

- **Subbiah Rathinagiri (S. Rathinagiri):** original StereoMask author and upstream PC/Windows development.
- **Theodore Whitten:** Mac port, editing workflow and foreground-selection improvements, developed with Codex assistance.
- **Upstream:** [SRathinaGiri/StereoMask](https://github.com/SRathinaGiri/StereoMask).

The original MIT copyright and license are preserved in [LICENSE](LICENSE). This repository documents the Mac work; upstream integration and reciprocal credits are coordinated by the collaborators, and are not claimed to have shipped already.

## Features

- Editable stereo-window frame points and disparity/depth.
- Select Brush and Erase Brush, with visible brush size and bracket-key resizing.
- Polygonal lasso with multiple additive/subtractive areas in each eye.
- Delete removes the latest unfinished segment; closed selections remain while making another.
- Pointer-centered wheel zoom, Space-drag pan, Command+0 fit, Command+Plus/Minus zoom.
- Transparent PNG, black, white, custom-color and eyedropper-selected backgrounds.
- Editable .msk projects with per-eye masks, saved lasso boundaries and undo/redo.
- Help → User Guide opens a bundled, offline guide in your default browser.
- Help → Check for Updates checks GitHub and offers the newer download page.

[Read the User Guide](docs/user-guide.html) by downloading and opening the HTML file in your browser. The in-app Help command opens the same guide.

## Build on Apple Silicon

R10 is source code, not a prebuilt download. It targets Apple Silicon (arm64). Intel builds and minimum compatible macOS versions have not been validated.

Install Xcode Command Line Tools and Homebrew, then the build dependencies:

```bash
brew install cmake qtbase qtsvg opencv python
```

Download this repository with Code → Download ZIP and unpack into Downloads. Save and quit the older app, then run:

```bash
bash "$HOME/Downloads/StereoMask-Mac-main/BUILD_BRUSH_VERSION.command"
```

From a Git checkout, run `bash BUILD_BRUSH_VERSION.command` in its source directory.
The script configures CMake, builds, deploys Qt, repairs dependency references, verifies the bundle and ad-hoc signs it. It stops on deployment or startup errors. After testing, copy `build-mac-brush/StereoMask.app` to Applications and launch that copy.

### R10 status

R8 editing behavior has been tested by Theodore on his Mac. R9 added names, documentation and dependency-graph packaging repair; R10 adds manual GitHub update checking. Mac launch verification for R10 is still pending. The repair matches versioned Homebrew aliases to already bundled libraries by Mach-O UUID, redirects non-system loads into the bundle, checks for duplicate OpenMP and re-signs. It does not bypass OpenMP's duplicate-runtime checks.

Ad-hoc signing is local signing; this is not an Apple Developer ID signed or notarized public binary. Do not label a binary release ready until both Finder launch and relocation to Applications have been verified. Third-party binary notices/licenses must accompany any binary release; this source repository preserves the original app license.

Build logs: `build-mac-brush/deployment.log`, `bundle-repair.log`, `startup.log`.

## Other platforms and testing

The cross-platform Qt/C++ source remains compatible with the upstream Windows direction. CMake chooses OpenCV `stereo` for version 5 and `calib3d` for version 4. Windows packaging retains the existing upstream vcpkg configuration; the R10 packaging helper runs only on macOS.

```bash
cmake -S . -B build -DSTEREOMASK_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
```

Qt 6 Widgets/Gui/Core/Concurrent and OpenCV core/imgproc/stereo (or calib3d) are required. The tests exercise per-eye masks, polygon completion, Add/Subtract, Undo, project restoration and view navigation. Packaging tests use simulated Mach-O dependency graphs; they do not replace an actual Mac launch test.

## Development history

[DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) is the cumulative technical handoff. [HISTORICAL_RELEASE_NOTES.md](HISTORICAL_RELEASE_NOTES.md) retains previous revision instructions; use the R10 instructions above for this build. `upstream-brush.patch` is a historical early brush patch, not the complete R10 changeset. Current source is authoritative.

## Publishing future updates

The app checks `updates.json` on the public `main` branch through GitHub's contents API. No login or token is included. Publish a new version by updating `appversion.h`, CMake's project/bundle version, visible revision labels and the development log, then publish `updates.json` with the same three-part numeric version. Its `page_url` must be an HTTPS page within this repository. `package_type` is `source` until a tested prebuilt app is actually published; then it may be `app` with a release-page URL. Keep useful plain-text `notes`.

The checker compares numeric version components, runs asynchronously, rejects invalid version/URL data, has request and total timeouts, handles HTTP/network failures and blocks overlapping checks. It opens a browser only after the user chooses Open GitHub; it does not overwrite or execute an app download. Recompile/install R10 once to gain the menu; older versions do not acquire new menus automatically.

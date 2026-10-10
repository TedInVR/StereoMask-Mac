# StereoMask Brush Mac preview - 2026-10-10

- Based on S. Rathinagiri's StereoMask 2.0.0; original MIT license retained.
- Mac changes: conditional Windows icon.rc; app bundle; OpenCV include dirs.
- New independent full-resolution per-eye reveal QImages; round reveal/erase
brush with diameter control; source-coordinate strokes respect panning, zoom,
and Swap. Stroke stays locked to its initial physical eye.
- Brush alpha unions with polygon opening. Erase modifies only reveal mask.
- Stroke undo/redo via QUndoCommand and implicit-sharing snapshots.
- .msk embeds PNG masks as base64, plus cutout/background fields; mask dimensions
validated on load. One-time existing project backup before brush save.
- New cutout export renders original eye with DestinationIn against union alpha.
Transparent PNG or opaque black/white. PNG required for transparent output.
Padding and gap receive the selected background. Existing async export takes
immutable snapshots; original exporter retained for non-cutout mode.
- SBS preview checkerboard is display-only. Anaglyph combines red left and
green/blue right after compositing. Frame control points remain editable.
- Toolbar selects Edit frame / Reveal brush / Erase reveal, pixel diameter,
and output background. Brush activation turns off Pan and Anaglyph.
- OpenCV 4 calib3d and OpenCV 5 stereo conditional selection.
- Build script reuses qtbase/qtsvg/OpenCV, explicit SDK C++ headers,
DYLD_FRAMEWORK_PATH for deployment, separate build-mac-brush directory,
and checks deployment ERROR messages. Existing installed app is untouched.

Validation: bash syntax passed; source review completed. Linux runtime lacks
Qt/OpenCV development packages; attempted dependency installation could not run.
C++ compilation and GUI tests have NOT been completed. Mac test required.

Mac validation:
1. Compile and launch from Finder.
2. Open SBS, retain four-point frame at nonzero depth.
3. Reveal/erase both eyes; verify undo/redo and Swap.
4. Save/reopen .msk and confirm backup.
5. Export transparent PNG and inspect alpha and dimensions in Photoshop.
6. Export black/white, check gap/padding.
7. Check anaglyph for eye mismatch and reopen old .msk.

Limits: no edge detection, stereo stroke propagation or soft brush.
Old opacity/feather bypassed for cutout. Large QImage masks/undo can use substantial
RAM; preview composites full-resolution images. Toolbar state can differ from
loaded project fields until explicitly selected. Rendering should be profiled.
No Intel build, clean-Mac test, signed/notarized release or distribution claim.


## R2 — 2026-10-10

User validated R1 compilation and opening after DYLD_LIBRARY_PATH was set to
/opt/homebrew/opt/libomp/lib. dyld diagnostics showed the same libomp UUID
loaded from Homebrew Cellar and the app bundle, causing OpenMP Error 15.

- Working SBS view now uses full source pixels; cutoutEye is used by export only.
- Green alpha overlay shows per-eye brush masks. Cache keyed by QImage::cacheKey
  avoids rebuilding full-size tint on mere mouse movement.
- Brush cursor is painted in widget logical coordinates with diameter=size*scale;
  contrasting white/black outline and blank native cursor over eye viewports.
- Window QShortcuts [ and ] adjust the spinbox by about 10% (at least one pixel),
  bounded 1..1000, only while a brush tool is selected.
- Custom QColor background added as mode 3; immutable export captures QColor.
- Explicit image eyedropper samples original physical eye pixels using existing
  widgetToImage transform and Swap mapping. One click completes; Escape cancels.
- .msk stores cutoutColor as HexArgb, mode3 accepted; both image-sidecar load and
  explicit .msk load restore reveal masks. Toolbar mode/background synced.
- Fixed R1 misplaced backup code: backup belongs before save, not image load.
  QSaveFile now commits project atomically; pre-brush-backup retained once.
- Launcher reproduces verified local libomp-path override for icon startup.
  Wrapped binary remains in Contents/MacOS for unchanged executable rpaths.
  Build script restores binary on repeats before macdeployqt scans it.
- Separate R2 source folder and same build-mac-brush subfolder inside it.
  Bundle identifier org.stereomask.brush, version2.0.2, title StereoMask Brush R2.

Validation: bash -n for builder and extracted launcher passed; launcher tested
with stub binary for path-with-spaces, argument forwarding, libomp override,
and removing deployment-only DYLD_FRAMEWORK_PATH. No C++ build available here.

Remaining manual Mac checks: build; Finder launch; full source visible with each
background choice; per-eye reveal/erase+undo/redo; cursor at two zoom levels;
brackets with toolbar focus; picker and eyedropper with pan+swap; Save/reopen;
transparent PNG alpha; solid custom color including padding+gap.
Local Homebrew startup dependency persists. Distribution/notarization/Intel and
VR180 projection are not part of this revision. Legacy opacity/feather does not
apply to cutout export. No edge detection or automatic cross-eye propagation.

## R2 packaging correction — 2026-10-10

User completed C++ build (100%) successfully. macdeployqt failed resolving
QtSvg despite DYLD_FRAMEWORK_PATH. Added the documented explicit
-libpath=/opt/homebrew/opt/qtsvg/lib flag. This supplies macdeployqt's own
framework search path. Mac packaging/launch confirmation remains pending.


## R3 Smart Brush — 2026-10-10

User confirmed R2 works and exported a transparent leg cutout, but manually
cleaning rough paint in Photopea was tedious. Requested guided automatic edges.
Implemented local OpenCV GrabCut rather than model-download segmentation.

UI:
- Outside frame adds Smart box (mode3), Keep hint (4), Remove hint (5),
  Erase hints (6). Find edges and Clear hints actions on the same toolbar.
- Loose box selects one local region in one physical eye. Eye mapping follows
  Swap, source-coordinate strokes follow zoom/pan, and box overlays identify
  active physical eye. Other polygons/frame depth are independent.
- Existing rough reveal is a probable-foreground initialization only. Explicit
  green Keep and red Remove strokes provide hard labels, confined to the box.
- Find edges requires a frame and >=5 Keep samples. Box must be >=8x8 pixels.
  Border supplies background samples unless explicitly seeded; user should
  leave background margin. Repeated Keep/Remove corrections trigger fresh
  GrabCut initialization using previous reveal as uncertain guidance.

Algorithm and state:
- smartselection.h/cpp is a UI-free function taking source, ROI, seed QImage,
  and previous rough reveal; returns ROI-sized alpha QImage or error text.
- Converts source RGB888 to OpenCV BGR with QImage row stride respected.
  5 GrabCut iterations in GC_INIT_WITH_MASK; definite labels cannot change.
- Foreground components disconnected from all explicit Keep seeds are removed.
  Source images, frame coordinates, and disparity values are never mutated.
- Small regions run at original resolution. ROI largest dimension capped1600;
  output resized to original ROI. Original-resolution Keep/Remove marks are
  reapplied, including narrow strokes. Cutout export still uses source pixels.
- QtConcurrent worker captures immutable input. QFutureWatcher returns on UI
  thread; main view processing guard blocks opening/drawing until finish.
  Revision counter rejects stale results after a newer mask/hints undo/change.
- Successful result replaces reveal only within ROI using CompositionMode_Source,
  preserves reveal everywhere else, and pushes one existing RevealStrokeCommand.
  SmartHintsCommand makes hint strokes, Clear hints, and ROI selection undoable.
- .msk brushVersion3: smartHintsLeftPNG/rightPNG, rect arrays smartRegionLeft/right,
  smartActiveRight. Both image-sidecar and explicit project load restore them.
  One-time backup and atomic project writes from R2 retained.
- Full source preview, green reveal tint, red/green hints, rectangle and brush
  outlines remain editing overlays only. PNG export contains none of them.
- CMake includes new algorithm sources; optional STEREOMASK_BUILD_TESTS defaults
  OFF and enables native smartselection_test + offscreen CTest if requested.
- Builder retains explicit QtSvg -libpath and verified local OpenMP launcher.
  Separate StereoMask-Brush-Mac-R3 folder and app title StereoMask Smart Brush R3.

Validation:
- Full application compiled/linked on Linux with Qt6.4.2/OpenCV4.6 using g++17.
  Test dependencies extracted into /tmp; no system installation required.
- Native algorithm tests cover sloppy-mask cleanup, required seeds, invalid box,
  hard Keep/Remove preservation, seed-connected components, and fresh selections.
- Native widget tests cover async result, ROI-only replacement, unchanged other
  eye/frame/disparity, undo/redo, .msk reload, Swap mapping, stale-job rejection,
  and exported PNG alpha including frame interior. Results recorded after run.
- Initial reload test compared differing QImage storage formats; corrected test
  to compare normalized premultiplied pixel images. Pixels survived correctly.
- OpenCV5 Python synthetic test also passed for sloppy foreground cleanup and
  hard seed preservation. Mac uses OpenCV5 and Qt6.11; Mac rebuild still needed.
- Builder bash syntax and ZIP integrity checked before delivery.

Remaining practical limits:
Similar colors/soft edges require user hints. Large ROI processing can soften
fine detail on rescaling; use tight but generous-margin local boxes. No automatic
stereo correspondence; work each eye separately. No AI model, magnetic lasso,
real-photo quality guarantee, Intel build, clean-Mac packaging or notarization.
All mask/hint undo snapshots can use substantial RAM. Local launcher still
requires Homebrew dependencies; this remains a local working-build package.

Final R3 result: native full-app build PASS; all native tests above PASS,
including fresh selection without prior paint and rejecting an unseeded
disconnected object of the same color. Mac app packaging remains user-tested.


## 2026-10-10 — R4 polygonal and magnetic lasso
- Added modes 7 polygonal and 8 magnetic; keep all R3 behavior and dependencies.
- Pending anchors store physical-eye coordinates, per-segment magnetic flags and
  traced polylines. Drag updates adjacent segments. Mixed tool use retains earlier
  segment choices. Enter/Close previews closure; Apply commits a union into reveal.
- Delete/Backspace removes one endpoint/segment, never the whole outline. The
  existing toolbar Delete action routes to the same operation in lasso modes.
  Escape cancels pending anchors without touching source/frame/reveal.
- Replace box clears only existing physical-eye Smart ROI and unions the lasso,
  all within one RevealStrokeCommand. Other eye and distant painted areas retained.
- Pending outlines are transient and must be applied before project save. Export
  blocks pending outlines with status instructions. Image/project load resets them.
- Separate mask, brush and selection toolbar rows; short brush-size label.
  Hide bright smart-hint overlays during lasso editing; source remains visible.
- smartselection.cpp magneticLassoPath: local 28px-margin ROI, bounded longest
  dimension 600, RGB Gaussian/Sobel strongest channel gradients, eight-neighbour
  Dijkstra cost with chord-distance penalty. Exact clicked endpoints are retained.
  Segments run on clicks/drag, not every hover. No new OpenCV modules required.
- Native Qt6.4/OpenCV4.6 app compilation and existing R3 tests passed. Added event
  tests for polygon fill, one-segment Delete/Backspace, Escape, one undo and eye
  isolation; strong-edge magnetic path/endpoints passed. Real-photo diagnostic
  traces checked on both eyes of supplied 1248x420 Belushi SBS.
- Mac deployment/startup remains Homebrew-specific and must be tested by Ted.
  Magnetic paths are edge estimates, not semantic segmentation; manual polygon
  mode and editable anchors provide correction. Final masks persist through the
  existing PNG-in-project mechanism; pending vector outlines are not persisted.
- Final added drag-point and Replace-box tests passed: moved vertex enlarges
  polygon, sloppy ROI exterior cleared, distant paint retained, undo restores
  original rough mask. Shell build script syntax check passed.


## 2026-10-10 — R5 navigation and removal of Smart controls
- User found no delete-box control and could not pan in R4 lasso mode. Navigation
  was intercepted by the earlier lasso handler. Route Space/Option/middle/Pan
  gestures before editing and move active-pan handling before lasso move logic.
- Space+drag keeps source-coordinate anchors/segments intact; keyboard release
  restores edit cursor. Fit view resets zoom/pan without clearing pending outline.
- Removed Smart box/Keep/Remove/Erase hints and Find/Clear hint actions from UI.
  Combo itemData maps visible indexes to legacy mode IDs 0,1,2,7,8. Saved legacy
  rectangles/hints are discarded on load; reveal/frame/background still load.
  Hide obsolete overlays except through legacy internal modes used in tests.
- Replaced Replace-box action with Erase lasso (subtract enclosed reveal only).
  Added Clear painted reveal, one undo macro across both physical eyes; frame
  points unchanged. Prior algorithm/API remains compiled for regression tests,
  but user-facing entry points are removed.
- Fixed an unrelated R4 broad-edit guard in setSelectedPointDisparity so it does
  not accidentally remove lasso segments. Delete guard remains specifically in
  deleteSelectedPoints.
- Builder now launches the app directly with explicit DYLD_LIBRARY_PATH matching
  the command Ted confirmed works, rather than relying on Finder open. Captures
  startup.log. Finder launch is still not proven and package remains Homebrew-local.
- Native full app compilation and existing selection, persistence, alpha, undo,
  physical-eye isolation and erase-lasso tests passed. Additional navigation tests
  exercise Space pan, wheel zoom, Fit view and source-coordinate lasso preservation;
  clear-reveal undo also tested. Shell syntax validated before packaging.
- Final navigation and clear-reveal undo assertions passed; unfinished outline
  produces identical source alpha after Space pan, wheel zoom and Fit view.


## 2026-10-10 — R6 polygon-only tool and first-point closure
- User reported magnetic paths unsuitable for real manual workflow; removed
  Magnetic lasso combo entry. Existing unreachable internal magnetic algorithm
  retained to minimize changes to compiled dependencies and regression coverage.
- Polygon mouse handler now closes when clicking the first anchor within 7 screen
  pixels, provided >=3 anchors and outline not already closed. Previously it began
  dragging the first anchor instead, preventing expected click-to-close interaction.
- Closed outlines still support dragging every point including the first. Enter
  and Close outline retained; applying remains explicit via Apply/Erase lasso.
- Added native event tests: click-first closes with status, raster result equals
  Enter-closed baseline, first anchor can still be dragged after closure.
- R5 startup behavior unchanged; repeated Mac startup crash remains unresolved.
  User can finish current R5 work with Enter then Apply without upgrading mid-edit.
- Validation: full native Linux/Qt app compilation, first-anchor closure and
  drag tests, all existing regression checks, and shell syntax passed.


## 2026-10-10 — R7 Command zoom shortcuts
- Fit view QAction now uses Qt CTRL+0 (Command+0 on macOS). Restores zoom1 and
  zero pan without modifying any source/frame/reveal/pending polygon coordinates.
- Added Zoom+ and Zoom− toolbar actions with CTRL+Plus/Equals and
  CTRL+Minus respectively. Qt maps CTRL shortcuts to Command on macOS. Zoom
  multiplier1.2 and existing setZoom bounds0.1..20; keyboard zoom keeps view-center
  source position fixed. Wheel's pointer-anchored behavior unchanged.
- R6 click-first closure and magnetic-menu removal included. Startup launcher is
  unchanged; Mac startup reliability remains unresolved. No new dependencies.
- Validation: native Qt/OpenCV app compiled successfully, existing closure,
  navigation, undo and alpha regressions passed; build shell syntax passed.
  Actual Command-key activation still requires testing on the Mac.


## 2026-10-10 — R8 persistent multi-selection and explicit Add/Subtract
- User closed an outline and could not start another; Escape erased pending
  outline. Closed pending shapes now auto-commit on the next non-anchor click,
  including in the other physical eye. Escape on a closed shape commits it;
  Escape only cancels an open unfinished shape. Cancel unfinished matches this.
- Added Selection operation combo: Add (default) or Subtract. Changing operation
  commits a closed pending shape with its previous operation first. Apply selection
  routes through finishLasso with selected operation. Pending point editing remains.
- Store independent completed QPainterPaths per physical eye. Union and subtract
  update retained vector boundary; render as dashed yellow/black lines in lasso mode.
  Multiple disjoint contours and overlaps retained. Completed paths are not exported.
- LassoSelectionCommand snapshots both alpha and vector path for undo/redo; applies
  path before alpha/autosave to persist consistent state. Brush stroke command still
  modifies alpha alone. Clear painted reveal clears alpha+path in one undo macro.
- .msk brushVersion4 adds lassoOutlineLeft/right as JSON fill rule and subpath
  polygons. loadImage resets paths; loadBrushSettings restores them. Legacy masks
  remain compatible, but old vector boundaries cannot be recovered from brush alpha.
- User-facing Save/Export finalize closed pending contours; open unfinished work
  gets a status reminder. Tool switch to frame/brush keeps a closed pending contour.
- Split selection and navigation toolbar rows to keep the operation selector and
  controls visible. R7 zoom shortcuts and R6 polygon closure remain.
- Native build passes. Added tests for auto-commit on new contour, two disjoint
  shapes, closed Escape retention, subtract only enclosed area, Add overlap, Undo
  restoring alpha and paths together, and save/load preserving subtract holes.
- Mac startup issue remains unresolved and launcher unchanged. Completed outlines
  retain operation geometry; later manual brush edits do not rebuild vector edges.
  Undo/rebuild is needed to edit points of previously committed outlines.
- Final multi-selection save/reopen assertion passed after normalizing both
  QImage pixel formats for comparison (PNG decode format differs, pixel data
  unchanged). All added multi-selection and existing regression tests passed.


## R9 — Browser guide, clear names, collaboration credits, dependency repair (2026-10-10)

- User validated R8 polygon completion and multiple additive/subtractive selections on Mac, using explicit DYLD_LIBRARY_PATH launch.
- Rename Reveal brush / Erase reveal to Select Brush / Erase Brush; rename menu label Outside frame to Editing Tool. Keep numeric mode IDs for existing project compatibility.
- New Help menu and toolbar Help open the same offline HTML guide with QDesktopServices. Embedded resource is written atomically to AppLocalDataLocation/help/r9/user-guide.html on each invocation. Guide covers frame creation/depth, per-eye foreground tracing, keyboard/mouse controls, toolbar/settings, save/export, troubleshooting and practical limitations.
- User corrected collaborator name to Subbiah Rathinagiri. About/splash/docs/README credit original author Subbiah Rathinagiri (license S. Rathinagiri), Mac work Theodore Whitten. Original MIT license unchanged. Upstream Windows integration is a collaboration plan; do not claim already merged. Original Gemini CLI and new Codex assistance distinguished.
- Supplied crash report Pasted text(9).txt: R8 Mach-O aborted before main at __kmp_register_library_startup in bundled libomp, called from bundled OpenBLAS initializer. Report lists duplicate identical UUID OpenCV/OpenBLAS/libomp libraries loaded both inside bundle and /opt/homebrew. Launcher workaround confirmed by user, but Finder still launched native executable without sufficient library unification.
- Replace shell-wrapper workaround with actual dependency repair after macdeployqt. scripts/repair_macos_bundle.py scans all Mach-O files including Qt plugins/frameworks, parses LC_LOAD*_DYLIB excluding LC_ID_DYLIB, matches absolute version aliases by original/bundled UUID, then rewrites non-system references to @loader_path paths. Preflight resolves entire graph before mutation. Verification rejects external/missing targets and multiple bundled OpenMP runtimes. Re-sign every nested binary and bundle ad-hoc and verify signature. Keep native executable; no KMP_DUPLICATE_LIB_OK suppression.
- Builder logs bundle-repair.log, stops on any unresolved packaging issue, launches without inherited DYLD paths, captures startup.log and checks process after two seconds. This brief health check is not an exhaustive runtime test. SDK libc++ workaround, OpenCV 5 stereo split and QtSvg -libpath handling retained.
- Seven simulated dependency-graph tests pass: UUID alias repair, missing dependencies, external references, path escapes, duplicate OpenMP, symlink deduplication and dependency parser ID exclusion.
- Native Linux Qt 6.4.2/OpenCV 4.6 full application compile and all pre-existing selection regression tests pass. New help integration test checks Help-menu action invokes file URL handler, exact embedded HTML bytes are written, corrected credit is present, and Select/Erase Brush names replace old names.
- GitHub repository publication authorized by user, proposed name StereoMask-Mac. GitHub account TedInVR connected and public repository TedInVR/StereoMask-Mac created by the user; initial source publication in progress. Package has clean README/CREDITS/.gitignore and historical release notes; no source images, crash reports, personal projects or native test binaries included.
- Outstanding: actual R9 Mac packaging/launch, Finder launch and copied Applications launch need user verification. No validated signed/notarized binary release is claimed. Testing simulated dependency graphs on Linux cannot validate macOS install_name_tool/codesign execution. Minimum OS and Intel support remain unvalidated.


## R10 — Manual GitHub update check (2026-10-10)

- User requested the same Check for Updates workflow as InjectZ. Add Help > Check for Updates; publish update notice with source on TedInVR/StereoMask-Mac.
- updatechecker.h/cpp uses Qt Network asynchronously to GET GitHub contents endpoint for updates.json on main, Accept application/vnd.github.raw+json. Public request includes app User-Agent only, no credentials. Manual checks only; image work remains accessible.
- appversion.h declares installed version 2.0.10; CMake bundle/project version agrees. Main QApplication exposes applicationVersion; visible R10 labels and current help guide updated.
- Manifest schema: version (three numeric components, optional v), page_url (HTTPS within github.com/TedInVR/StereoMask-Mac), notes (plain text), package_type (source/app). Versions compared numerically, so 2.0.10 exceeds 2.0.9. Invalid data, network/TLS/HTTP errors, missing feed, rate limit and >64 KiB data produce failed-check results, never false up-to-date results. Transfer inactivity timeout 15s and total deadline 20s. No overlapping requests.
- MainWindow disables action while checking, restores after completion, displays installed/latest version and notes as plain text. Newer version offers Open GitHub; source notices explicitly say build required. Failure offers repository browser fallback. No automatic download or installation.
- Current updates.json publishes version 2.0.10 as source, with repository download page; no prebuilt/validated Mac binary claimed. Future maintainer workflow documented in README. Existing R9 installation cannot gain this menu until rebuilt once.
- New test uses local HTTP fixture to check raw Accept header, newer/equal/older versions, malformed data, 404/403, oversized response and duplicate-check suppression. Validation rejects insecure/lookalike/wrong-repository URLs. Help-menu test requires update action alongside embedded guide.
- Full Linux compile, existing selection tests, Help-menu/browser handoff test, update HTTP tests and seven packaging tests pass. R10 macOS runtime and actual GitHub HTTPS fetch still need user confirmation. R9 dependency repair retained; does not regress masking/selection code.

### R10 packaging dependency repair — 2026-10-10

The reported Mac build compiled successfully but deployment could not find OpenSSL 4 dylibs or QtSvg. The builder now installs missing OpenSSL 4 and QtSvg dependencies before compiling, verifies their files, and copies QtSvg into the app before macdeployqt discovers plugins. The README includes OpenSSL 4 explicitly. Shell syntax was checked; deployment and startup still require confirmation on the user's Mac.

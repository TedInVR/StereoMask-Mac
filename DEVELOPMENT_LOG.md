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

### R10 QtSvg discovery and final signing — 2026-10-10

The next Mac log still reported unresolved QtSvg during plugin discovery and an invalid signature on a modified Brotli library. Add qtsvg to the executable's LC_RPATH before macdeployqt. Defer only its old-signature verification messages to the existing dependency repair and final strict codesign verification; all other deployment errors still stop the build. Shell syntax checked. Mac execution remains to be verified.

## Mac R11 / 2.1.1 — upstream Windows v2.1.1 integration (2026-10-11)

### Changes
- Ported Subbiah Rathinagiri’s Windows v2.1.1 core and UI from upstream commit c1186d71b281610ac887753d5451b419857697a9, using docs/windows-mac-comparison.md as the inspected R10 baseline checklist. Mac parent: 94c8ff8269d5e0e744ddb8aefcd61e487bcc6e73.
- Independent polygon/raster selection objects with identities, ordered Add/Subtract, visibility, naming, reorder, deletion, point editing and Undo/Redo. Whole-polygon movement keeps partners independent and preserves geometry.
- Frame holes, common per-eye compositor, cache, composed anaglyph, Preview Mask, Clean Preview and Match preview export; legacy cutout modes retained on project load. New images default to Match preview.
- Optional paired matching and Smart polygon lasso, Shift-edge point insertion, Command-click removal, Return/C closure, Command+Return apply, arrow pan and Freehand routing fix. Regular lasso remains available.
- Shared appearance controls, separate custom cutout color, Settings cancel without mutation, original Bernd Paksa attribution. Adapted offline guide and labels for Mac.
- Preserved Mac-only updater endpoint/URL allowlist and Mach-O bundle repair; native Windows deployment files were not imported. Reopened Smart objects emit the actual brush mode so the tool dropdown stays synchronized.
- Permanent install: scripts/install_macos_app.py stages and verifies before replacement; rollback restores prior app on final verification failure. Rejects a different bundle identity, symlink destinations and active StereoMask processes. BUILD_BRUSH_VERSION.command installs and launches /Applications/StereoMask.app. Uses CMake --fresh to survive development-folder relocation and links missing Homebrew formulas.
- UPDATE_STEREOMASK.command downloads source to temporary storage, merges repository files into /Applications/StereoMask Development, builds and installs; no accumulating Downloads revisions. Personal source edits need a separate checkout.

### Architecture / handoff
- SelectionObject/SelectionState, SelectionObjectsCommand, JSON brushVersion 5 and selectionObjects.version 1 are in stereoviewwidget.h/.cpp. Flattened old projects import raster objects; original polygon vertices are not reconstructed. Save retains object order and metadata.
- openingMask, exportEye, maskedPreviewEye and composed cache share preview/export appearance. Object edits and mask appearance invalidate rendering. Per-eye anaglyph uses stereoimageprocessor::createAnaglyphFromEyes.
- alignStereoPolygon in smartselection uses horizontal grayscale template correlation, reverse validation and disparity/median fallback. Copies are independent, not enduring constraints; inspect the opposite eye. Magnetic helpers remain the prior shared foundation.
- mainwindow.cpp wires selection dock and actions; macOS Qt::ControlModifier maps to Command, while Qt::MetaModifier maps to physical Control. Kept Qt semantics and wrote Mac-facing Command labels. Actual Mac event delivery still needs verification.
- Version is 2.1.1, visible label Mac R11, guide cache help/r11. Mac updates.json remains source-only. No prebuilt Mac release was generated in this Linux environment.

### Validation / limits
- Native Qt 6.4.2/OpenCV 4.6 full application compilation completed in Linux. Tests require QT_PLUGIN_PATH for extracted JPEG plugins as well as the offscreen platform path; initial JPEG test failure was a missing test-environment plugin search path. Final results are appended below.
- Python dependency graph and transactional installation tests: 12 passed. Both shell scripts passed bash -n.
- Actual Apple Silicon compilation with OpenCV 5, Qt deployment, strict signing, Finder startup, Command-click/keypad Return and the installed Applications path still need user-Mac confirmation. Packaging fixes from R10 remain included, not claimed as independently Mac-certified. Intel compatibility and minimum macOS deployment target are unverified.
- No calibrated occlusion recovery, canvas click-to-select applied objects, groups or per-object feathering added. Smart contours and paired matches are estimates.

### Final R11 checks
- Native smart-selection integration suite passed: object/JSON/legacy migration, independent pairing, whole-polygon movement/cancel/clamp/Undo, point gestures, navigation, preview color/alpha, PNG/JPEG export parity, anaglyph, cache invalidation and Freehand routing.
- Help/UI suite passed, including dock/actions, exact embedded guide browser URL, smart/paired/move tools, shared Settings controls, cancellation without mutation, color picker and clickable creator attribution. Inspected offscreen UI screenshot.
- Update suite passed after adjusting network version fixtures for 2.1.1: newer/equal/older, trusted URLs, errors, size limits and duplicate requests.
- 12 Python bundle/installer tests passed. bash -n passed both scripts. HTML internal navigation anchors were checked. Mac execution remains the final platform check.

### R11 Mac deployment follow-up — 2026-10-11
The user’s native Apple Silicon build reached 100%, but macdeployqt still could not resolve QtSvg from its plugin search paths. Explicitly copy SVG plugins when available and defer only the exact QtSvg resolver error (when its framework exists) and stale signature messages. All other deployment errors stop. The existing repair must then resolve every Mach-O load to bundled dependencies and verify strict signing before installation. Added deployment-log regression checks for missing SVG, mixed/unrelated errors, silent failures and known signature messages. This is not blanket suppression of deployment failures; real Mac runtime verification remains pending.

## Mac R12 / 2.1.2 — Stereo Window (2026-10-11)
- Theodore requested a first optional step similar to Stereo Photo Maker’s horizontal Easy Adjustment: source-pair depth, distinct from custom frame disparity.
- Added Stereo Window near New/Open: isolated source anaglyph dialog, horizontal slider, source-pixel spin field, Reset, Apply/Cancel. Positive relative left-minus-right translation moves forward, negative back. Integer shifts are split between eyes; dimensions stay constant and new side strips are transparent.
- StereoImageProcessor retains original eyes and recomputes integer translations from originals, preserving zero-offset pixel identity and avoiding repeated resampling. All source access/export/AutoMask/pairing now uses adjusted eyes. Anaglyph preview uses a display-resolution copy and never mutates the project.
- StereoWindowCommand records before/after offsets as one undo step; ++m_smartRevision invalidates rendering and stale smart jobs. Existing frame/selection geometry stays fixed. Unfinished lasso and polygon move mode block dialog opening.
- stereoWindowOffset is stored alongside brushVersion 5. Common loadBrushSettings restores it for explicit project load and image-sidecar auto-load. Missing field defaults zero; bounds are half eye-width. Current Windows v2.1.1/earlier Macs ignore the extension, so guide recommends adjusted-pair export or copies for exchange.
- Version 2.1.2, visible Mac R12, help cache r12. Updated guide with first-step tutorial, source preservation, side-strip behavior and exchange limits.

### Next requested feature — optional Guided Workflow (not implemented)
Theodore wants a less intimidating, step-by-step editing route: optionally adjust stereo window; create a basic frame; edit or accept frame; choose brush/lasso for foreground protrusions; review and export. Each step should provide a short instruction, appropriate tools, and accept/Next/Skip navigation without losing completed work. Keep manual editing available. This is the next proposed task after Stereo Window, not a shipped feature.

### Validation
Native build, pixel/alpha/offset/Undo/Redo/source-preservation/JSON/sidecar/export-cache checks and Apply/Cancel UI tests are being run. Apple Silicon packaging and physical Mac interaction remain user-side validation.

### Final R12 results
Full native Qt/OpenCV application compiled. Smart-selection suite passed with new stereo-window assertions for aligned anaglyph pixels, preview isolation, shifted eyes/transparent side strips, unchanged dimensions, export cache, one-step Undo/Redo, source-pixel reset, original file preservation, explicit/sidecar project load and unchanged custom frame coordinates. Help/UI suite passed Apply/Cancel/Undo through the actual dialog. Existing object, pairing, preview/export, navigation and updater suites passed. 16 Python packaging/installer tests and both shell syntax checks passed. Guided Workflow was recorded as a future feature, not added to the UI. Actual Mac build and launch remain to be confirmed.

## 2026-10-11 — Mac R13 / 2.1.3

Theodore confirmed R12 Stereo Window works on his Mac. R13 adds clickable Push back and Pull forward buttons at the slider ends, one-pixel dialog-wide Left/Right keyboard control, and 32×22-pixel spinbox arrows in a 44-pixel-high field. Pixel entry remains synchronized with the slider. Numeric-field cursor handling is intercepted for plain horizontal arrows, as requested.

Added optional Picture Frame toolbar dialog: Show picture frame, width in source pixels, color, Flat/Beveled/Decorative Trim. An outside-only stroke follows each physical eye's custom frame path and disparity. The full ordered opening mask removes frame pixels under preserved foreground, so Add selections and brush reveals appear in front. The border is clipped to the existing canvas; it does not expand the photo. Preview Mask/Clean Preview and export share the same composed eyes. Transparent PNG retains opaque border pixels and transparent surrounding pixels. Settings persist in the new pictureFrame project extension and have undo/redo. Earlier Windows/Mac releases ignore that extension.

Updated embedded browser User Guide, README, start instructions and updates.json. Guided Workflow remains a recorded future request, not a shipped feature.

Validation: full native Qt/C++ build and existing selection/export/update/help regression suites passed in Linux offscreen Qt. Added checks for buttons/Left-Right controls in spinbox, slider, button and dialog; frame color/width, exterior alpha, foreground overlap, eye isolation, decorative cache refresh, project save/reopen, Undo/Redo and composed PNG padding/pixels. 16 Python packaging/installer tests passed; both command scripts passed bash syntax checks and git diff whitespace checks. Visually inspected Stereo Window dialog screenshot. R13 Apple Silicon build, macOS keyboard delivery and packaging still need user's Mac confirmation; no prebuilt Mac binary is being published.

## 2026-10-11 — Mac R14 / 2.1.4

Theodore requested less toolbar clutter, in-window access to native menu commands during full screen, independent collapsible sections and the panel name History. Added an always-present compact toolbar: Menus shares every existing native File/Edit/Help/View menu; seven section toggles open existing control rows independently; Layout offers Show all/Hide all; History directly toggles the renamed dock. First use defaults to collapsed rows and hidden History. QSettings retains row choices; History visibility persists while the main window is visible. Related controls are initially grouped in existing rows (File/View/Assist/App, Edit/Align, Transform containing Scale/Move/Curve/Rotate, Frame, Editing Tool, Selection, Navigate). This is the first compact-layout iteration, not a guided workflow.

History remains the ordered current selection-object list, with its existing per-edit controls; it is not a full-project chronological rollback timeline. Guide explains that distinction and the new layout.

Changed right-click insertion for closed frames to the nearest sampled curve/straight edge, avoiding the prior off-edge append ordering. Existing vertices remain; insertion supports Undo/Redo. Guide explains right-click edge, drag the inserted dot, and two dots for a rectangular extension. Click an edge away from an existing dot: clicking a dot selects it.

Validation: full native Qt/C++ application build succeeded. Selection/render/export/help/update regression suites passed. Added independent collapse/expand and History reopen checks, and nearest-edge insertion preserving old vertices/order plus Undo/Redo. All 16 Python packaging/installer tests passed and both command scripts passed bash syntax checks. Visually inspected compact control bar. Apple Silicon/full-screen behavior and deployment still need user's Mac confirmation; Linux offscreen checks are not Mac validation.

## 2026-10-11 — Mac R15 / 2.1.5

Final scope incorporates Theodore's approved logical grouping. Narrow bar: File (only New/Open/Close/Save/Export), View (navigation and preview), Stereo Window (own section), Create Frame (points/Auto/Freehand), Edit Frame (points/delete/align/transforms/depth/Picture Frame/assists), Out-of-Frame (brush/lasso/selection/background/color/settings), History, Layout and Help. Existing controls are regrouped using their original actions and QWidgetActions, preserving handlers. Edit Frame and Out-of-Frame toggle multiple rows together. Show/Hide all retained. Organized section preferences use a fresh namespace so old grouped-row settings do not accidentally expand unrelated controls. All action shortcuts remain attached to the main window while rows are collapsed.

Out-of-Frame Background replaces Export background. The foreground-tool dropdown no longer includes Edit frame; Create with points and Edit points explicitly enter frame mode, clear Pan/Anaglyph/Clean Preview, and focus the image. Added precise Frame point depth numeric control synchronized with selected dots. User Guide contains section table and updated tutorials. History still means the ordered individual selection-object edit list, not a complete chronological rollback timeline. File's native menu also contains only the five requested operations; Command+Q remains available to quit.

main.cpp uses showMaximized (not native full screen), then requests raise/activate/setActiveWindow. macOS-only appactivation.mm sets regular activation policy and calls AppKit activateIgnoringOtherApps; CMake enables OBJCXX and links AppKit only on Apple. Native Mac menu bar stays available in a maximized window.

Close unloads the current project without quitting. Auto Save saves first; Auto Save off offers Save/Discard/Cancel. Save failure retains the project. Unfinished outlines must be applied/canceled; active refinement/brush/polygon drag cannot be unloaded. Closing stops auto-mask playback and clears source, frame, objects, masks, caches, undo and freehand state; files are not deleted.

Validation: native Qt/C++ regression tests check exact section names and five File controls, separate Stereo Window action, no Edit frame in the foreground dropdown, frame-depth field, independent row toggles and History reopening. Close test checks empty source/frame/selection/undo/cache and subsequent reopen. The full native Qt/C++ build and selection/export/update/help tests passed, along with 16 Python packaging/installer tests, both bash syntax checks and git whitespace checks. Visually inspected the compact layout with a loaded stereo pair. Mac AppKit activation, maximization and packaging require user's macOS confirmation; Linux offscreen checks do not validate Apple runtime behavior.

## 2026-10-11 — Mac R16 / 2.1.6

Implemented Theodore's complete approved request batch. Main Menu replaces Menus and reaches File/View/Stereo Window/Create Frame/Edit Frame/Out-of-Frame/History/Layout/Help/Guided Process, plus root Check for Updates and About. Stereo Window on the narrow bar opens its dialog directly; Show All excludes its redundant control row. Help now contains User Guide and Keyboard Shortcuts, so its two-choice section remains collapsible. Main Menu includes every ordinary action, Auto Frame modes, brush/lasso/background choices and numeric input dialogs for brush size, frame depth and background opacity. Existing editing/rendering actions and handlers are preserved.

Restored Open Recent in native File, File controls and Main Menu. It remembers up to five image/project paths; untouched newly opened images can reopen before their .msk exists. Successful recent opens update lastFolder; New/Open already persist lastFolder between launches. Clear labels restored: Scale with Enlarge Frame/Shrink Frame, Move and Curve/Rotate; Add Point enters frame mode and explains right-click insertion, Delete Point explains selecting dots and Delete. Background Color/Sample Background Color/Clear Foreground Selections/Background & Export Settings replace opaque mask/reveal labels; Preview Result replaces Preview Mask. Guide explains Match preview color/opacity versus the separate solid background color.

Added keyboardshortcuts.cpp: readable popup spells Command/Control/Option/Shift, including contextual editing gestures. Edit Shortcuts supports common command bindings and optional new bindings for Guided Process, Stereo Window, frame point editing, picture frame, scale and shortcut help. Save persists QSettings overrides; Cancel changes nothing; Restore Defaults preserves alternate aliases. Duplicate aliases and reserved editing keys are rejected. Text fields use word-based combinations. Delete/Backspace/Escape/arrows/Space/Enter/C/Command+Enter and mouse modifier gestures remain fixed; not every drawing interaction is customizable.

Added guidedprocess.cpp: resumable dock with goals for basic frame/out-of-frame/source-window adjustment; image loading; optional source depth; frame creation (at least three points); frame refinement; protrusion yes/no; brush/polygon choice; both-eye foreground editing; background/border; clean/anaglyph review; separate save/export. Back uses visited-step history, optional steps have Skip, Next checks image/frame/unfinished outlines and both-eye confirmation. Choosing foreground tools disables clean/masked preview for source visibility. Guide closure retains edits. The guide does not segment subjects or verify depth automatically.

User Guide, README, start instructions and update notice refreshed. Validation: full native Qt/C++ build and selection/export/update/help suites passed. Added UI checks for complete Main Menu coverage/direct Stereo Window; readable shortcut list; cancel/save/reload/defaults and alias preservation; reserved-key conflict rejection; guided no-image/no-frame gates, yes/no branches, polygon activation, both-eye confirmation, Back through skipped steps, project saving and preserving edits on Finish. All 16 Python packaging/installer tests and shell syntax checks passed. Visually inspected shortcut popup and guided dock with stereo image. Apple Silicon packaging and macOS keyboard behavior still need the user's Mac validation.

## 2026-10-11 — Mac R17 / 2.1.7

Stereo Window now opens maximized with an expanding full-resolution anaglyph canvas rather than a fixed 800x450 thumbnail. Zoom/fit buttons, Command-plus/equal/minus/zero, wheel zoom and drag panning inspect exact source detail. Depth updates preserve zoom/pan; Fit follows viewport resizing. Manual and Guided Process use the same dialog. Preview generation no longer downsamples to 1000 pixels; large images can therefore take more time and memory per depth change.

Guided navigation moved directly below instructions and visible step controls, with 12 logical pixels of spacing; remaining stretch is below the buttons. User Guide and update notice revised. Full native build/regression validation recorded after running; actual Mac keyboard/packaging behavior remains user-side verification.

## 2026-10-11 — Mac R18 / 2.1.8 — shapes, shadows, stereo text

- Shapes dialog: rectangle/square/oval/circle, per-eye preview, drag position and resize, source-pixel width/height/depth, zoom/fit and Apply/Cancel. Square/circle are constrained during creation, then ordinary editable geometry. Four cubic Bezier segments approximate an ellipse. Replacing a frame asks first, retains foreground selections and uses one undo command. Added curve subdivision on inserted points, preserving shape and interpolating per-eye control-point disparities.
- Picture Frame settings include optional shadow independent of border: color, opacity, offset distance/direction, spread and Gaussian softness. Source-canvas per-eye silhouette follows frame disparity and visible border width; ordered opening removes it from preserved pixels. Shadow then border then annotations share cached preview/export composition. Shadow clipped to source canvas, no physical photo lighting/canvas extension.
- Text & Bubbles editor: persistent vector objects with IDs, editable text/system fonts/pixel size/bold/italic/alignment/color, position/box/depth, text-only/oval speech/rounded rectangle speech/thought styles, fill opacity and outline width/color, draggable box/size/tail in either eye, Add/Duplicate/Delete/visibility and anaglyph inspection. Right-eye X is left-eye X minus depth. Optional final foreground-preservation alpha occludes text; depth alone does not produce automatic photo occlusion.
- Draft editor changes commit once on Apply; Cancel does not mutate project. Text collection and shape commands support Undo/Redo and autosave. Shapes and Text & Bubbles are reachable in native Edit, Main Menu and in-window controls; Guided Process offers Shapes at frame creation. Shortcut editor can assign optional shortcuts.
- New stereoTextVersion 1 / stereoText array; shadow fields extend pictureFrame. Old app versions ignore them and may lose them on resave; guide recommends project copies or rendered exports. Fonts are not embedded. Loading limits 100 objects and 10,000 characters per object, with bounded numeric settings.
- New decorations.h/.cpp and creativetools.cpp; main rendering/JSON in stereoviewwidget; CMake test targets include new and previous guided/shortcut sources.
- Updated User Guide, README, start instructions, update manifest and maintainer handoff. Mac/Homebrew build/install path retained.
- Validation: native Qt/OpenCV compile and first complete selection, creative-rendering, help/UI and updater suites passed. Added tests cover cubic insertion alpha parity, shadow depth/opacity/blur/cache, bubble alpha/depth/occlusion, font/style rendering, undo, saved project/export parity and dialog Apply/Cancel. Final checks follow.

Final R18 validation: full native Qt/OpenCV application compilation and selection/render/export, creative tools, help/UI and updater suites passed. UI checks exercised Shapes Apply/Undo, shadow dialog Apply/Undo, text/bubble Apply/Cancel/Undo and dragging a bubble tail. Reviewed screenshots of shape, text and shadow editors; adjusted text-panel layout to avoid horizontal clipping. All 16 Python packaging/installation tests, shell syntax and whitespace checks passed. Apple Silicon build, physical Mac input and deployment remain user-side confirmation; no prebuilt/notarized binary claimed.

## 2026-10-15 — Mac R19 / 2.3.0 — reciprocal Windows v2.3.0 integration

Reviewed upstream commit 00b8bd6c696624085b5bcca584639a6370e1f0d1 and its windows-to-mac-handoff.md against Mac R18 / 705095b74b5e54bbb99e020dffb6d8c3ee0ca8a7; upstream is two commits beyond the previously integrated c1186d71b281610ac887753d5451b419857697a9. Ported color selection/tolerance/crosshair and Guided Process routing, extreme/median alignment, frame corner/closing-edge ordering, optional SBS source-depth preview, opt-in editing-wheel navigation, processing stages/workers/text caches and relief/cutout/stereo-plane orientation. appversion.h now supplies runtime and CMake/macOS bundle version.

Preserved Mac anaglyph and wheel-zoom defaults, explicit optional wheel-pan action with saved preference, physical Command/Control mapping and spelled-out keys, compact layout/background/scale names, recent image support, foreground launch, permanent Applications updater and strict dependency/signature checks. Preserved Mac two-eye shape handles; shared new text transforms drive eye-specific outlines/hit testing. Restored portable Ctrl+0 literal after checking Mac label substitution, actual reopened polygon mode, non-object JSON skip and text-cache release on Close/load.

First full build succeeded; smart-selection assertions passed but process aborted at destruction. Traced invalid free to parented value-member progress dialog surviving its widget via an async watcher. Changed panel/label/main-window tracking to QPointer with heap-owned Qt widgets; host-first regression and the full selection suite now exit successfully. Progress startup dispatches layout/paint rather than unrestricted queued work. Fuzzy uses four neighbors per upstream handoff (upstream source allowed diagonals); documented separately for Subbiah's optional review.

Second native full build and selection, creative, help/UI and updater suites passed. New tests cover pixel/Swap/cursor/color operation ordering, text relief/tilt/cutout/JSON/cache/export, preview modes, opt-in navigation/default retention, live effects/progress UI and parent-lifetime cleanup. Existing shape/shadow/shortcut/guided/save behavior retained. Sixteen Python packaging/install tests and shell/whitespace checks passed. Final rebuild after guide/preview/trackpad documentation follows. Actual Mac native build/input/deployment remains user-side verification; no binary release claimed.

Updated embedded User Guide, README, start instructions and source update manifest; retained upstream handoff snapshot and added docs/mac-r19-integration-notes.md plus pointer in the outgoing handoff. No changes made to Subbiah's Windows repository.

Final R19 result: native full build and all four C++ suites passed after final guide/trackpad/preview changes; all 16 Python packaging/install tests, both shell syntax checks and git diff whitespace checks passed. Reviewed text-effects and processing-panel screenshots. Published source only; Mac installation uses the existing permanent updater command.

## 2026-10-18 — Mac R20 / 2.3.1 — voluntary support panel

User provided Stereo_Mask_Support_Handoff(1).md from Inject Z and authorized the same public support links for StereoMask Mac. Copied R19 to a separate R20 working tree before modification. Added supportdialog.h using existing Qt Widgets/DesktopServices only, and a retained QPointer-owned nonmodal dialog through MainWindow::showSupport. Native Help, in-window Help and Main Menu → Help share the same QAction.

Exact links: https://ko-fi.com/tedwhitten, https://paypal.me/theodorewhitten1, https://patreon.com/TedWhitten. No browser opening during panel construction/show, only button activation; no payment SDK, credentials, embedded checkout or user settings. Panel spells out collaborative authorship and identifies Ted in VR as recipient of these contributions. No PayPal email, pricing, membership-tier promises or tested-payout claims. No startup wiring or feature gates. Close/Escape/window close/Command-W hide and retain the panel; browser launch failures show the public URL.

Version updated centrally to 2.3.1 (Mac R20) so existing version-based checks detect the source update. Embedded guide, README, start instructions and outgoing handoff updated. Existing macOS identifier, signing/dependency/install scripts and Mac updater remain intact; no Inject Z installer/signing patches imported. GitHub source publication does not replace any existing prebuilt release asset. Mac packaging/native interaction cannot be certified by Linux verification.

R20 verification: full application compilation, existing help/UI integration suite with new exact-URL/nonmodal/reuse/keyboard/window-close assertions, and updater suite passed on Linux Qt/OpenCV. HTTPS actions were captured locally; no real payment pages or transactions were opened. All 16 Python packaging/install tests, shell syntax and whitespace checks passed. Reviewed the support-panel screenshot and sized wrapped copy to avoid clipping. Mac-specific appearance, physical Command-W delivery and packaging still require user-Mac confirmation.

## 2026-10-20 — Mac R21 / 2.5.0 — Windows v2.4–v2.5 integration

Reviewed Windows master/v2.5.0 f08a2465d899661935ea46262f0a8621c0532e46, cumulative changes since the already-integrated Windows v2.3.0 baseline, and Subbiah's current handoff against Mac R20 425f403755e8085973a58ef95052188ceef60017. Imported toolbox drawers, Quick Access, version-generated help, alignment grids and three-band outline frames. Preserved Mac support URLs, updater, AppKit activation, maximized launch, wheel defaults, readable Command/Option shortcuts, four-neighbor Fuzzy Select and deployment/install safeguards. See docs/mac-r21-integration-notes.md for the source map and intentional differences.

Mac uses 190-pixel drawers (History 240), clarifies outline color tracking as “Use preview background color,” and opens File alone on every startup, even when another drawer was left open/floating. Saved positions and favorites remain. Reset opens File. Added a missing Create Frame icon to the rail. Existing editable selection, text/relief, guide, recents, support and Stereo Window checks remain. New outline JSON is optional; grid/favorites are local workspace preferences. Outlines are clipped to the existing canvas and remain behind preserved foreground. Clean Preview/export include applied outlines independently of normal-editor visibility; grids never export.

Generated resources.qrc.in embeds the generated guide with appversion.h's numeric version; source docs retain a placeholder and revision label. Help cache is /help/<version>/. The window title, splash, About, bundle and update metadata now identify R21 / 2.5.0. The build script's stale R16 launch message was made revision-independent. User Guide, README, start instructions, credits, integration notes and outgoing AI handoff are updated. The outgoing handoff asks Subbiah's AI to explain individual Mac choices and let him choose Adopt/Adapt/Defer/Decline.

Verification: full application and five C++ test executables built on Linux with Qt 6.4.2/OpenCV 4.6. All five CTest suites pass (smart-selection, user-guide, creative-tools, update-checker, quick-access). This includes grid pixels/clipping/two-color contrast and export exclusion; outline bands/gap/curves/disparity/foreground/color invalidation; editor vs output visibility; settings Apply/Cancel; Undo/Redo/project reload; favorites state/context/manager/persistence; File-only restart after saving a floating drawer; and original Mac support/guide/stereo/text workflows. Updated an old test click coordinate for the taller canvas and changed the updater test to derive newer/equal versions from appversion.h, avoiding stale fixed-version fixtures. The first JPEG check required exposing the extracted Qt image plugins in this isolated build environment, with no product change. All 16 Python packaging/install tests pass; both shell scripts pass syntax checks and git diff --check is clean. Reviewed toolbox, favorites and outline-dialog screenshots for readable controls.

Native Apple Silicon Qt/OpenCV 5 compilation, macOS font/DPI and physical modifier/secondary-click delivery, dependency repair/signing, Finder launch and relocation remain unverified in this Linux environment. Source update only; no prebuilt release asset replaced. Existing permanent /Applications source/app updater remains the installation path.

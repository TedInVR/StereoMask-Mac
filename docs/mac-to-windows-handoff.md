# Mac-to-Windows handoff: optional changes after Windows v2.1.1

**Update 2026-10-15:** Windows v2.3.0 has already incorporated the main R12–R18 features below. Mac R19 integrates its reciprocal additions. For current changes and individually reviewable corrections, see [Mac R19 integration notes](mac-r19-integration-notes.md). The original item-by-item approval guidance remains in force for new Windows work.

Prepared 2026-10-11 for Subbiah Rathinagiri and his coding assistant.
Mac maintainer: Theodore Whitten (TedInVR). Original StereoMask author: Subbiah Rathinagiri. Original license and credits remain intact.

## Scope and reference

This is a technical handoff, not a request to merge every Mac preference into Windows.
Baseline: upstream Windows commit `c1186d71b281610ac887753d5451b419857697a9`, incorporated into Mac R11 / 2.1.1.
Reviewed implementation: Mac R16 / 2.1.6, commit [f0365b7](https://github.com/TedInVR/StereoMask-Mac/commit/f0365b7f59279cdcfbdd50ac1da7e837cd8608ad).
The initial items cover subsequent R12–R16 work; follow-up sections add R17 and R18. The R18 source is commit 705095b74b5e54bbb99e020dffb6d8c3ee0ca8a7 (Mac 2.1.8); the current tree now contains R19. Existing selection objects, pairing and other Windows v2.1.1 features are not claimed as new Mac contributions.
Read the source at that commit; this handoff describes intent and boundaries. DEVELOPMENT_LOG.md contains revision-specific implementation and validation notes.

## Instructions for the receiving AI

Subbiah controls the Windows version. Theodore explicitly welcomes different design choices and selective adoption.

Before implementing ANY item below:
1. Explain that individual item to Subbiah in plain language: what changes, why it was added, how the user operates it, and relevant compatibility or platform implications.
2. Ask him to choose Adopt, Adapt, Defer or Decline for that item. Record his choice. You may group the presentation for convenience, but obtain a separate decision for each numbered item.
3. For Adapt, establish the desired Windows behavior before editing. Do not interpret approval of one item as approval of the others.
4. Implement only approved scope, preserving existing Windows functionality. Source inspection and planning may happen before decisions; implementation must wait.
5. Explain technical dependencies where selective adoption needs supporting code. Request approval for those dependencies too; do not silently import unrelated UI changes.

Do not blindly copy mainwindow.cpp or replace the Windows tree. These files contain intertwined Mac changes. Prefer small, reviewed ports. Preserve Windows shortcuts, packaging, updater destination, credits and license.

## Individually selectable changes

### 1. Stereo Window adjustment
Purpose: set the starting stereo depth before framing or tracing foreground objects.
UI: source-image red/cyan anaglyph, horizontal slider, pixel entry, Reset and Apply/Cancel. Push Back/Pull Forward buttons step one pixel; plain Left/Right arrows synchronize slider and number. Enlarged spin arrows improve clicking.
Implementation: stereoimageprocessor.h/.cpp, stereoviewwidget.h/.cpp, undocommand.h/.cpp, mainwindow.h/.cpp.
Original eyes are retained. Integer horizontal translations are recomputed from originals, avoiding repeated resampling. Positive relative left-minus-right translation pulls forward; negative pushes back. Relative offset is split between eyes, bounded to half an eye's width. Dimensions stay fixed; exposed strips are transparent.
Apply is one undo step. Preview does not mutate the project; Cancel retains prior state. Existing frame and foreground coordinates stay fixed, so use this before tracing. Unfinished lasso or polygon movement blocks opening.
Optional subchoices: adopt the underlying adjustment, then independently choose button wording and keyboard behavior.

### 2. Picture Frame
Optional Flat, Beveled or Decorative Trim border with color and width in source pixels.
Implementation: PictureFrameSettings and rendering in stereoviewwidget.h/.cpp; dialog in mainwindow.cpp; undo support in undocommand.h/.cpp.
Border follows each eye's frame path and disparity, outside the opening. Ordered foreground preservation removes border pixels under protruding subjects. Preview and export share composed-eye rendering.
Border is clipped to the existing canvas; no automatic canvas expansion. Transparency remains available around an opaque border. Decorative Trim is a procedural style, not a library of ornate frame images.

### 3. Compact, collapsible interface
A permanent narrow bar opens independently collapsible control sections. Default layout keeps most rows closed and History hidden; Layout provides Show All/Hide All. Preferences persist.
Implementation: MainWindow::createToolbar in mainwindow.cpp.
This is a Mac usability preference, not a required rendering change. Subbiah may retain the Windows arrangement.

### 4. Logical control grouping and complete Main Menu
Sections: File, View, Stereo Window, Create Frame, Edit Frame, Out-of-Frame, History, Layout, Help and Guided Process.
Main Menu exposes section commands and widget choices; Check for Updates and About also appear there. Stereo Window directly opens its dialog rather than expanding a redundant one-button row.
Help currently contains User Guide and Keyboard Shortcuts. Shared QAction handlers prevent duplicate command implementations.
Implementation: mainwindow.cpp. Adopt independently of the collapse mechanism if desired.

### 5. History panel naming
The existing selection-object dock is labeled History and can be reopened from the compact bar/Main Menu.
Important: it remains an ordered list of current editable selection objects, NOT a chronological history of every project operation. Whole-project reversal still uses Undo/Redo.
Implementation: mainwindow.cpp and user-guide.html.
Please explain this distinction before adopting the label; Subbiah may prefer the original name.

### 6. Frame point insertion and clear editing controls
Closed-frame right-click insertion chooses the nearest sampled straight/curved edge, preserving existing vertex order. Clicking an existing dot selects it. Two inserted points can form an extension when dragged.
Add Point and Delete Point controls provide gesture tooltips. Frame-point depth is exposed in Edit Frame.
Implementation: stereoviewwidget.cpp, mainwindow.cpp; undo uses existing point commands.
Check Windows mouse modifiers and deletion behavior rather than importing Mac Command gestures unaltered.

### 7. Visible frame transforms
Edit Frame clearly exposes Scale, Move, Curve and Rotate. Enlarge Frame/Shrink Frame label the existing scaling actions and explain selecting frame points; scaling uses 10% steps.
Implementation: mainwindow.cpp.
Primarily visibility and naming changes, not a new transformation engine.

### 8. Clearer background and selection wording
Preview Result replaces Preview Mask. Out-of-Frame Background, Background Color, Sample Background Color, Clear Foreground Selections, and Background & Export Settings replace ambiguous mask labels.
Match Preview uses shared preview color/opacity. Choose Solid Color is a separate opaque export choice. Sample Background Color chooses a source color and uses Match Preview at full opacity. Clear Foreground Selections clears preserved foreground in both eyes, retains frame and is undoable.
Implementation: mainwindow.cpp and docs/user-guide.html; internal mask API names remain.
A mask is the internal visibility map; the frame is geometry, preserved foreground is the exception outside it, and background is the appearance of removed areas. Do not accidentally merge these concepts when renaming.

### 9. Close image/project
Close (Mac Command+W) clears the active document without quitting.
Implementation: MainWindow::closeProject, StereoViewWidget::closeProject, mainwindow.cpp/.h and stereoviewwidget.cpp/.h.
Save/Discard/Cancel applies when autosave is disabled; save failures retain the document. In-progress editing has guards. Clear source, frame/selection state, undo and rendering caches consistently.
Use an appropriate Windows shortcut.

### 10. Open Recent and remembered folder
Up to five recent image/project paths are exposed in File and Main Menu. Recent images work before a .msk sidecar exists. Successful opens and saves update recents; dialogs start in the last chosen folder.
Implementation: mainwindow.cpp and existing settings support.
Local preference data, not project schema.

### 11. Maximized, foreground launch
Startup uses a maximized window, keeping the normal menu available. Mac AppKit activation brings an intentionally launched app forward.
Implementation: main.cpp, appactivation.mm and APPLE-only CMake code.
Port maximization only if approved. AppKit/Objective-C++ is Mac-specific and must not be compiled on Windows; use appropriate Windows/Qt activation behavior.

### 12. Readable keyboard-shortcut reference
Help → Keyboard Shortcuts opens a scrollable list with words such as Command, Control, Option and Shift, plus fixed drawing/navigation gestures.
Implementation: keyboardshortcuts.cpp, mainwindow.h/.cpp and CMakeLists.txt.
Windows should display Control/Alt/Windows as appropriate. On Mac, Qt ControlModifier represents Command and MetaModifier represents physical Control; do not reuse Mac text-to-modifier mapping on Windows.

### 13. Customizable keyboard shortcuts
Edit Shortcuts supports common commands and several optional actions, saved preferences, blank-to-disable, Cancel without mutation, Restore Defaults, duplicate/reserved-key rejection and retained default aliases.
Implementation: keyboardshortcuts.cpp. Bindings use stable IDs/properties and QSettings keys keyboardShortcuts/<id>.
Fixed contextual gestures remain noncustomizable; this is not a universal gesture editor. Preserve Windows aliases and resolve platform-specific reserved keys. Adopt independently of item 12 only with an alternative entry point.

### 14. Guided Process
Optional dock-based tutorial keeps the image editable and visible. Goals: basic frame, out-of-frame effect or stereo-window adjustment only.
Steps: open/use image; optional stereo depth; create frame; refine; decide whether anything protrudes; choose Select Brush or Polygonal Lasso; inspect both eyes; background/optional Picture Frame; preview; save project/export.
Implementation: guidedprocess.cpp, mainwindow.h/.cpp and CMakeLists.txt.
Back follows the actual visited branch; optional steps have Skip. Gates check image presence, at least three frame points, unfinished lasso and user confirmation of both-eye review. These checks do not judge selection accuracy or stereo comfort. Foreground editing turns result/clean preview off so the source remains visible.
Finish hides the dock; reopening resumes state. There is no dedicated Restart button. It instructs the user rather than automatically segmenting subjects.
Adapt instructions/tool entry points to whichever other items Subbiah approves; do not advertise rejected features.

### 15. Updated User Guide
Embedded browser-opened HTML covers the above tools, tutorials, shortcuts, compatibility and limitations.
Implementation: docs/user-guide.html, resources.qrc and MainWindow::showHelp. Mac help cache path is versioned.
Port only approved content and Windows key names. README/START_HERE/DEVELOPMENT_LOG are supporting documentation, not Windows UI requirements.

## Project compatibility

| Extension | Meaning | Selective-port requirement |
| --- | --- | --- |
| stereoWindowOffset | Integer relative eye translation; absent means zero | Port processor application, JSON load/save, cache invalidation and undo together |
| pictureFrame | Border enable/width/style/color settings | Port settings, JSON load/save, compositor and undo together |
| Existing brushVersion 5 / selectionObjects version 1 | Windows v2.1.1-derived selection architecture | Retain ordered per-eye selection semantics; these are baseline, not new R12–R16 format claims |

Older Windows v2.1.1 and earlier Mac versions ignore the two new extensions, so opening a project there will not reproduce the stereo offset or picture border. Do not assume round-trip saving in an older version preserves unrecognized fields. Keep copies when exchanging projects; an exported rendered pair preserves appearance but not editable project state.
UI layout, recent files and customized shortcuts are local preferences, not portable .msk project fields.

## Validation and limits

Linux native Qt 6.4.2/OpenCV 4.6 full application compilation and offscreen selection/render/export, help/UI, guided-flow, shortcut and update-check suites passed. Sixteen Python packaging/installer tests and shell syntax checks passed.
New assertions cover source-offset identity/reset/undo/JSON, border alpha and foreground overlap, insertion order, Close, Main Menu coverage, shortcut persistence/defaults/conflicts and guided branches/gates/save.
Relevant tests: tests/smartselection_test.cpp, tests/help_test.cpp, tests/updatechecker_test.cpp and Python tests under tests/.
Theodore confirmed the initial Stereo Window feature works on his Mac. This is not blanket certification of R16 on macOS or Windows. Latest Mac interaction/deployment still requires user verification. No validated Intel build, minimum macOS target, notarized release or prebuilt Mac binary is claimed.
After each approved Windows port, compile and test on Windows, including actual keyboard delivery, project exchange, Undo/Redo, both-eye preview and PNG alpha/export parity.

## Mac infrastructure: retain Windows equivalents

BUILD_BRUSH_VERSION.command, UPDATE_STEREOMASK.command and scripts/repair_macos_bundle.py, check_deployment.py and install_macos_app.py address Homebrew, Mach-O dependencies, signing and permanent /Applications installation. Do not import them into Windows packaging.
Mac updatechecker/updates.json target TedInVR/StereoMask-Mac. Retain Subbiah's own Windows update feed and versioning.
No changes are being made to the Windows repository by this handoff.

## Decision record

For each numbered item record: Adopt / Adapt / Defer / Decline, Subbiah's requested behavior, supporting dependencies, implementation commit and Windows verification. Unreviewed items remain unapproved.


## R17 follow-up (2026-10-11): separate approval items

16. Stereo Window inspection: maximized dialog, full-resolution anaglyph, zoom/fit buttons, Command-plus/minus/zero, wheel zoom and drag panning. Depth changes preserve zoom/inspection area. Both manual and Guided Process invoke the same dialog. Files: stereowindowview.h, mainwindow.cpp, stereoimageprocessor.cpp. Windows key names should be adapted. Full-resolution generation increases memory/time for large images.

17. Guided Process navigation placement: Back/Next/Skip/Finish moved immediately below instructions and step controls, rather than the panel bottom. File: guidedprocess.cpp.

As with items 1–15, explain each follow-up to Subbiah and obtain his individual Adopt/Adapt/Defer/Decline decision before implementing it.

## R18 follow-up (2026-10-11): individually optional creative tools

18. Frame Shapes: Rectangle/Square/Oval/Circle creation dialog, source dimensions and stereo depth, drag placement/resize, zoom/fit, replace confirmation and undo. Applied shapes become normal editable frame points. Square/circle constraints apply only during creation. Ellipses use four cubic segments; inserting a point subdivides a curve and interpolates control disparities rather than distorting it. Files: decorations.h/.cpp (shapePoints), creativetools.cpp (MainWindow::createFrameShape), stereoviewwidget.cpp (frame command and curve insertion), mainwindow.cpp, guidedprocess.cpp. Existing point JSON already stores this geometry, so no new shape project field is required.

19. Drop Shadow: optional settings independent of border enablement in Picture Frame. Opacity, distance, clockwise direction (0 right, 90 down), Gaussian softness, spread and color. Per-eye frame/border silhouette follows existing disparity; shadow is composited only outside the ordered opening, before border/text. Transparent PNG carries partial alpha. Does not calculate physical shadows on pictured objects; no canvas expansion. Files: PictureFrameSettings in stereoviewwidget.h, shadowOverlay/exportEye/JSON in stereoviewwidget.cpp, adjustPictureFrame in mainwindow.cpp. New shadowEnabled/shadowOpacity/shadowDistance/shadowAngle/shadowBlur/shadowSpread/shadowColor fields inside pictureFrame; missing fields use disabled defaults.

20. Stereo Text & Speech Bubbles: multiple persistent vector text objects; system fonts, size, Bold/Italic, alignment/colors, box/position/depth, optional oval/rounded rectangle speech or thought bubbles, fill opacity/outline, draggable body/resize/tail, numeric controls, duplicate/delete/visibility and anaglyph inspection. Same object renders in both eyes; right X = left X minus depth. Optional foregroundCovers removes text/bubble pixels beneath final foreground-preservation masks; disparity alone does not infer photo occlusion. Entire dialog is a draft until Apply, committed as one undoable operation. Files: decorations.h/.cpp (StereoText, textOverlay and JSON), creativetools.cpp (MainWindow::editStereoText and canvas), stereoviewwidget.h/.cpp (storage, compositor, undo/load/save), mainwindow.h/.cpp and CMakeLists.txt. New stereoTextVersion=1 and stereoText array preserve text IDs, content, font, rect, tail, depth, style, alignment, colors, opacity, outline, visibility and foregroundCovers. Objects render in collection order. Fonts are referenced, not embedded. Limits: 100 objects, 10,000 characters each; rendering clips to the canvas. Large full-resolution images/many objects increase composition cost.

These three items require separate decisions from Subbiah before Windows implementation. Explain behavior, dependencies and alternatives individually. Shape creation does not require text or shadow. Shadow reuses the existing picture-frame compositor; text uses the shared per-eye export pipeline but can be ported without the shape dialog. Adapt Command/Option labels and shortcut mapping for Windows. Earlier versions ignore new fields and may discard them when saving, so use copies for project exchange.

Verification includes tests/creative_test.cpp (geometry, curve alpha parity, shadow transparency/depth/cache, bubble opacity/occlusion, project/export and undo) and tests/help_test.cpp (actual dialogs, drag interaction and Apply/Cancel). Mac-specific packaging and physical keyboard behavior still require Mac confirmation; Windows ports need independent native testing.

## R20 follow-up (2026-10-18): optional support panel

Mac R20 / 2.3.1 adds native Help, in-window Help and Main Menu → Help → Support StereoMask. Source: supportdialog.h plus MainWindow::showSupport and supportStereoMaskAction. A retained, nonmodal Qt window launches three exact public URLs only on button activation; no startup prompts, SDK, checkout, payment credentials or feature restrictions. Close/Escape/Command-W/window close hide it safely for reuse.

These particular accounts support **Ted in VR**: Ko-fi https://ko-fi.com/tedwhitten; PayPal https://paypal.me/theodorewhitten1; Patreon https://patreon.com/TedWhitten. Panel text preserves Subbiah's authorship and collaboration credit, and does not imply these are joint donation accounts. Ko-fi/PayPal are one-time choices; Patreon offers ongoing support through whatever options its public page currently exposes. No payment/payout or published paid-tier validation is claimed.

As before, explain this item to Subbiah before implementing anything in Windows. He can Adopt, Adapt, Defer or Decline independently; explicitly let him decide whether Windows should have support links and whose accounts it should use. Do not silently copy Ted's payment destinations into the Windows version. Adapt Command/Control wording and normal Windows dismissal conventions if approved. No Windows source was modified.

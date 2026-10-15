# Mac R19 / 2.3.0 — Windows v2.3.0 integration

Prepared 2026-10-15 for Theodore Whitten and Subbiah Rathinagiri.

## Source references and ownership

- Mac starting point: R18 / 2.1.8, commit `705095b74b5e54bbb99e020dffb6d8c3ee0ca8a7`.
- Previous integrated Windows baseline: `c1186d71b281610ac887753d5451b419857697a9` (v2.1.1).
- New Windows source: `00b8bd6c696624085b5bcca584639a6370e1f0d1` (v2.3.0), repository SRathinaGiri/StereoMask.
- Reviewed Subbiah's Windows-to-Mac handoff; a source snapshot is retained as windows-to-mac-handoff-v2.3.0.md.

These are Subbiah's new Windows enhancements brought back to Mac. Existing Mac workflow, shapes, shadows and text/bubbles already present in his version were not counted as new features. Original MIT license and collaboration credits remain intact. No Windows repository was modified.

## Integrated features

1. Fuzzy Select region and full-eye Select by Color Threshold, tolerance 0–255 (default 32), physical-eye raster History objects, Add/Subtract, Undo/Redo and existing project format. Source editing retains a visible crosshair.
2. Alignment targets selected geometric extremes; depth alignment uses the median disparity, including the average of the middle two for even counts.
3. Frame corner creation appends far-from-edge clicks in order; clicks within 10 screen pixels insert on an edge. Closing-edge insertion keeps the first vertex and subdivides cubic geometry/disparities.
4. Optional SBS Stereo Window preview, full-resolution zoom/pan, shared slider and Apply/Cancel behavior, with correct Swap mapping.
5. Editing navigation supports wheel/trackpad vertical pan, Shift-wheel horizontal pan and modified-wheel zoom. See preserved Mac defaults below.
6. Opaque processing stages for image/project opening, selections, Auto Frame, edge refinement, text application, saving and export. Pure calculations run on captured image/state snapshots; document mutation stays on the GUI thread. No guessed percentages or cancellation during commit.
7. Eye-specific applied-text caches keyed by source image, foreground alpha and all serialized text fields; preload left/right on Apply and reuse in preview/anaglyph/export. Clear caches on image replacement/Close.
8. Flat/Embossed/Engraved text, Colored Letters/Cutout Plate, relief size and shared stereo X/Y/Z plane orientation. Optional JSON extensions retain flat/unrotated defaults for old projects; fonts remain unembedded.
9. appversion.h is the single application-version source for runtime and CMake/bundle metadata. Mac revision R19 is a separate display label.

## Mac choices retained

- Anaglyph remains the Stereo Window default because Theodore explicitly requested it; the new checkbox offers SBS. Main canvas Swap is respected.
- Ordinary wheel still zooms by default. View / Main Menu → View → Scroll to Pan While Editing opts into the new mode for frame/lasso tools; this preference persists. Command-wheel (and physical Control-wheel when delivered by the OS) zooms in that mode. Middle-drag, Space-drag and Option-drag remain available. Creative/Stereo Window dialogs retain wheel zoom, including pixel-delta trackpads.
- Mac Command/Control/Option names and modifier interpretation remain intact, including Command+0 in creative dialogs.
- Keep the permanent /Applications/StereoMask.app and /Applications/StereoMask Development updater/install workflow, native foreground activation, Homebrew/OpenCV 5 support, dependency repair and strict signing checks.
- Mac updater URLs/allowlist still target TedInVR/StereoMask-Mac. Preserve clearer background/scale labels, recent images/projects, compact sections, shape editing in either eye and direct tool access.

## Small corrections worth individual Windows review

Theodore's original request still applies: explain each correction to Subbiah before implementing it in Windows; let him Adopt, Adapt, Defer or Decline independently.

### A. Processing-panel lifetime

The initial port's smart-selection suite completed its assertions but aborted during widget destruction with an invalid free. The upstream OperationProgress stores a parented QDialog as a value member. An asynchronous watcher can retain the progress object after the QWidget begins deleting its QObject children; Qt then attempts to delete the embedded dialog directly.

Mac now allocates the panel and its label normally on the heap, with QPointer tracking for panel/label/main window. It deletes the panel only if it still exists and tolerates the host already being destroyed. This keeps Qt parent lookup/transient ownership while avoiding double deletion. A regression explicitly destroys the host before the progress owner, calls setStage and releases the progress owner, checking cursor restoration. The full selection suite now exits successfully.

Panel construction also delivers only layout/paint posted events instead of general processEvents; worker waits retain the upstream modal event loop. This reduces arbitrary timer/queued-command reentry before synchronous guards are established. Some operations remain synchronous behind a visible stage; do not describe the entire program as asynchronous.

### B. Fuzzy connectivity discrepancy

The upstream handoff describes four-neighbor connectivity, but the source's nested dx/dy loop accepts diagonal neighbors as well. Mac explicitly uses four shared-edge neighbors to match the handoff. A diagonal-only red-pixel fixture verifies that choice. Subbiah can keep eight-neighbor behavior if that was intentional; this is a selectable convention rather than a required Windows correction.

### C. Port preservation and compatibility

Retain actual reopened polygon mode when updating the tool selector (rather than always reporting mode 7), skip non-object JSON text entries as R18 did, clear obsolete text caches, and keep Mac shortcut spellings out of literal QKeySequence strings (Qt's portable Ctrl token represents Command on Mac). These are port-specific preservation details, not demands to copy Mac UI naming into Windows.

## Validation

Native Linux Qt 6.4.2/OpenCV 4.6 build and smart-selection, creative-rendering, help/UI and updater suites are used, plus 16 Python packaging/installer tests and shell syntax/whitespace checks. Tests cover:

- All legacy Mac selection/preview/export/guide/shortcut/guided-flow behaviors.
- New color connectivity/tolerance, both physical eyes, Swap, crosshair, Add/Subtract and Undo.
- Frame corner order, closing-edge subdivision, navigation and source geometry retention.
- Relief/cutout alpha, X/Y disparity gradients with matching Y, Z roll and visibility at steep angles on large images.
- Old JSON defaults, bounded new fields, save/reopen, Undo/Redo, text-cache invalidation and PNG pixel parity.
- Actual new effects controls, processing-stage visibility/opacity/worker execution, failure cleanup and parent-before-progress destruction.

Reviewed native offscreen screenshots of the text-effects editor and processing message. Linux checks do not certify Apple Silicon packaging, native Mac font/key delivery, signing, Finder launch, Intel support or a minimum OS. The source updater rebuilds on Theodore's Mac; no prebuilt/notarized binary is claimed.

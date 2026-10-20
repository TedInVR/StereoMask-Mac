# Mac R21 / 2.5.0 — Windows v2.4–v2.5 integration

Compared on 2026-10-20. Mac baseline: R20 / 2.3.1, `425f403755e8085973a58ef95052188ceef60017`. Upstream baseline already integrated in R19: Windows v2.3.0, `00b8bd6c696624085b5bcca584639a6370e1f0d1`. New upstream: Windows v2.5.0, `f08a2465d899661935ea46262f0a8621c0532e46`. Reviewed the cumulative v2.3.0-to-v2.5.0 source diff and the [upstream handoff snapshot](windows-to-mac-handoff-v2.5.0.md); no Windows-only binary/deployment files were copied.

## Adopted

- Toolbox rail and docked, scrollable drawers, accordion behavior, floating/left/right placement, collapse/reset actions, full-width dropdowns and Recent Files.
- Quick Access command registry, original action state/handlers, star drawer, secondary-click menus, searchable draft manager, ordering, duplicate/unknown-ID filtering and local preferences. Stable upstream command IDs retained.
- Generated embedded help from appversion.h and version-specific browser cache. Mac revision label retained in splash/title/About.
- Alignment grid: per-axis counts 0–100, single/two-color lines, opacity, source-image anchoring and viewport clipping, anaglyph overlay, Clean Preview exclusion and local settings. No export/project/Undo changes for grid preferences.
- Three-band outline: per-eye curved geometry, gap/widths/colors, preview-background-color tracking, foreground occlusion, independent normal-editor visibility, applied Clean Preview/export output, Undo/Redo and optional pictureFrame/outline fields. Existing border/shadow remain independent. Outline cache keys include source, geometry, selections/settings and preview background color.
- Upstream lifetime/malformed-text/cache regressions; corresponding Mac fixes already existed. Existing Mac tests retained.

## Intentional Mac choices

1. **Startup:** every new launch opens File only, including after quitting with another drawer open or floating. Saved placement and favorites remain; open/closed state is deliberately overridden. Reset also opens File. This implements Ted's pending request from 2026-10-18.
2. **Drawer width:** 190 logical pixels instead of Windows' 150; History remains 240. Favorite labels use the extra width. Native Mac font/DPI confirmation is still required.
3. **Keyboard and navigation:** Show Grid uses Command+G; the shortcut list spells modifier names out. Mac anaglyph Stereo Window default, ordinary wheel zoom default and optional editing-wheel panning remain. Fuzzy Select retains four shared-edge neighbors; Windows intentionally retains eight. Saved raster selections are compatible.
4. **Terminology:** outline bands say “Use preview background color” instead of “Use mask color.” This is the preview/background QColor, not a custom cutout color or opacity. JSON keys remain upstream-compatible.
5. **Mac services:** native menus, AppKit foreground activation, maximized startup, source updater URLs, optional Ted in VR support links, bundle identity, SDK/dependency repair, signing and transactional /Applications installation remain.

## Compatibility

New outline settings are optional under pictureFrame/outline; old projects default disabled. Invalid numeric values are bounded. Older readers may discard new fields: exchange copies of editable projects. Grid and favorites are local layout.ini preferences. Existing text, selection, foreground masks and stereoWindowOffset formats are unchanged. No margins are added: the existing canvas clips outlines and shadows.

## Verification

Results are recorded in DEVELOPMENT_LOG.md. Cross-platform Qt/OpenCV checks cover the new dialogs, drawers/favorites, grid pixels/clipping/export exclusion, outline geometry/disparity/curves/foreground, editor/output differences, Undo/Redo and project reload, plus existing Mac workflows/support/updater. Python tests cover packaging/install helpers. These tests do not certify native Apple Silicon deployment, font sizing, physical key/secondary-click delivery or Finder launch; those require the user's Mac.

Authorship remains collaborative: Subbiah Rathinagiri and Theodore Whitten, with original creative inspiration credited to Bernd Paksa's Stereomasken. MIT license retained.

# StereoMask Polygonal Lasso R8 — multiple selections

## Install
Apply and save your work in the old app, quit it, unzip this folder into Downloads,
then run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R8/BUILD_BRUSH_VERSION.command"
```

Wait for **StereoMask Lasso R8**. Startup reliability remains under investigation;
the same explicit Terminal launcher and startup.log are retained.

## Select multiple legs or objects
1. Choose Polygonal lasso and **Selection: Add to selection** (the default).
2. Click around one leg in one eye, then click the first point to close.
3. Click away from its yellow editing points to start another outline. The closed
   selection commits automatically; its boundary stays visible as a dashed yellow
   line. Earlier shapes are kept. Overlapping new shapes enlarge the selection.
4. Alternatively, click **Apply selection** to commit the current outline.
5. Choose **Subtract from selection** to outline an unwanted area. It removes
   only that area from painted reveal; it does not erase the whole selection.
6. Repeat separately for the other eye. Masks/boundaries belong to physical eyes,
   independent of display swapping. Save your project; export transparent PNG.

Escape cancels only an OPEN unfinished outline. If the outline is already CLOSED,
Escape keeps it and exits point editing. Cancel unfinished follows the same rule.
Delete/Backspace removes the most recent point/segment of the outline currently
being edited. Undo removes the latest applied selection and restores alpha and
boundaries together. Closed pending points remain draggable until the outline is
applied or you start the next one. Committed outlines remain visible but are no
longer individually point-editable; undo a recent contour if it needs rebuilding.

Save and Export automatically apply a closed pending outline. They request that
an open outline be closed or applied first. Committed boundaries persist in .msk
projects; reopening earlier brush-only projects retains their alpha masks, but
cannot reconstruct their original vector outlines. Brush editing changes alpha;
retained outlines describe lasso operations, not individual manual brush strokes.
The source pixels and stereo frame/depth remain unchanged.

Space+drag pans during a lasso. Command+0 fits both complete images; Command+Plus
(or Equals) zooms in; Command+Minus zooms out. Scroll-wheel zoom still works.
Clear painted reveal clears committed alpha and boundaries in both eyes, undoably.

In the old R5–R7 versions, finish each outline with Enter then Apply lasso before
starting the next. Older versions do not retain applied vector boundaries.

Native Linux build/selection regression tests run; the Mac runtime still needs
user verification. This remains a Homebrew-dependent local Apple Silicon build.

---
## Prior revision notes (historical)

# StereoMask Polygonal Lasso R7 — Mac zoom shortcuts

## Install
Save your applied work, quit the older app, unzip this folder into Downloads, and run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R7/BUILD_BRUSH_VERSION.command"
```

Wait for the **StereoMask Lasso R7** window. This includes the R6 fix for clicking
its first polygon point to close and removes the magnetic tool from the menu.

## Zoom shortcuts
- **Command+0**: fit the complete side-by-side pair and reset panning.
- **Command+Plus**: zoom in. **Command+Equals** also works without holding Shift.
- **Command+Minus**: zoom out.
- Scroll-wheel zoom remains available, as does Space+drag to move the view.
- Fit and zoom actions retain your unfinished polygon outline and all frame/mask data.
- Visible Fit view / Zoom + / Zoom − buttons are on the selection toolbar.

Startup diagnostics remain ongoing: the build retains the confirmed Terminal
launch environment and startup.log. Finder-icon launch reliability is not yet
established. Pending outlines must be applied before saving a project.

---
## Prior revision notes (historical)

# StereoMask Polygonal Lasso R6

## Install
Quit the prior app. Unzip this folder into Downloads, then run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R6/BUILD_BRUSH_VERSION.command"
```

Wait for **StereoMask Lasso R6**. Startup diagnosis remains ongoing; this revision
retains the R5 Terminal launch environment and startup.log. No claim is made that
Finder-icon startup has been repaired.

## Polygonal lasso
The magnetic lasso is removed from the tools menu. Select Polygonal lasso, click
successive outline points in one eye, and **click the first yellow point** to close
(after at least three points). Enter/Close outline still works as an alternative.
Click Apply lasso to preserve the enclosed area. Click Erase lasso to subtract it
from painted reveal; the frame stays open. A closed outline remains adjustable:
drag yellow points, including the first, before applying.

Delete/Backspace steps backward one clicked segment. Escape cancels the unfinished
outline. Hold Space and drag to pan without losing your outline. Scroll over the
photo to zoom; Fit view resets the view. Repeat separately in the other eye.

You can finish the current R5 outline without installing this update: press Enter,
then Apply lasso. Pending outlines are temporary: apply before saving/exporting.
Frame/depth, source image and committed reveal are preserved by project loading.

Native app build and event-based tests cover first-point closure and subsequent
point adjustment, alongside the existing navigation/alpha/undo checks. Mac runtime
must still be checked on the user's computer.

---
## Previous revision notes (historical)

# StereoMask Lasso R5 — navigation and Smart-tool cleanup

## Install
Quit the old app, unzip this folder into Downloads, then run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R5/BUILD_BRUSH_VERSION.command"
```

Wait for the **StereoMask Lasso R5** window. The builder launches with the explicit
Homebrew OpenMP environment that worked on Ted's Mac, and records startup output
in build-mac-brush/startup.log. Finder-icon launch portability remains unverified.

## Zoom and move the view
- Place the pointer over the part of the photo you want, then use the scroll wheel.
- Hold **Space** and drag the photo to bring a leg/foot into view. Release Space
  to continue the lasso. This works during an unfinished outline; it does not
  move the frame points or change the outline's image coordinates.
- Option-drag or middle-button drag also temporarily pans.
- **Fit view** restores the full-image view while retaining the unfinished outline.
- Pan button dragging also works during lasso mode. Turn Pan off to place points.

## Selection tools
Outside frame now contains only Edit frame, Reveal brush, Erase reveal,
Polygonal lasso and Magnetic lasso. Smart box / Keep / Remove / Find edges are
removed from the UI. Existing Smart boxes and hints are discarded on project/image
load; frame/depth and painted reveal masks are preserved. Original project files
remain intact until saved, with the existing backup and atomic-write behavior.

Click successive outline points; drag yellow anchors to correct. Delete/Backspace
removes only the last clicked segment. Escape cancels the pending outline.
Enter or Close outline closes for inspection. Apply lasso adds the enclosed area;
**Erase lasso** subtracts it from painted reveal, leaving the stereo frame open.
**Clear painted reveal** clears old paint in both eyes while preserving the frame;
Undo restores it in one step. Use it to start fresh after the poor Smart result.
All applied lasso operations have Undo/Redo. Work separately in each eye.

An unfinished outline is temporary and must be applied before saving/exporting.
The export action reminds you if an outline is pending. The original photo remains
visible throughout. Transparent export uses PNG.

Native Linux app build and tests run; Mac runtime is tested by the user. Magnetic
edge selection remains approximate, with polygonal lasso as the manual fallback.

---
## Prior revision notes (historical; R5 instructions above supersede them)

# StereoMask Lasso R4 — Apple Silicon Mac

This revision adds polygonal and magnetic lasso tools and separate toolbar rows.

## Build and launch
Quit the earlier StereoMask app. Extract this folder into Downloads, then run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R4/BUILD_BRUSH_VERSION.command"
```
Wait for packaging and the app to open. Press Return by itself at the closing prompt.
The window title is **StereoMask Lasso R4**. The build includes the QtSvg path and
Homebrew OpenMP launcher fixes used by the working R3 build. This is a local
Apple Silicon build requiring the existing Homebrew dependencies; it is not a
standalone distribution package.

## Lasso workflow
1. Open the image/project. Your existing frame is preserved.
2. In **Outside frame**, choose **Polygonal lasso** or **Magnetic lasso**.
3. Click successive outline points in one eye. Magnetic segments snap toward
   local image edges; place points closer together around toes and bends.
4. **Delete/Backspace** removes only the last clicked segment and endpoint.
   Repeat to step backward. **Escape** cancels the unfinished outline.
5. Drag a yellow point to adjust the adjacent segments. You may switch between
   the two lasso modes while drawing; each new segment uses the selected mode.
6. **Enter** or **Close outline** closes the outline for inspection. Points can
   still be dragged. Click **Apply lasso** to add the enclosed area to the reveal.
7. To replace existing sloppy paint, use **Replace box** instead. It clears the
   old reveal inside that eye's existing Smart box, then adds the lasso shape.
   Other reveal areas are preserved. A Smart box must exist in the same eye.
8. Repeat separately in the other eye. Save the project and export transparent PNG.

Application is one undoable operation. The stereo frame and depth remain unchanged.
The source image stays visible. Bright Keep/Remove hints are hidden in lasso modes.
An unfinished outline is temporary; apply it before saving or exporting. Export
is blocked while an outline is pending so it cannot silently omit your work.
Magnetic edges can still be ambiguous: use shorter segments, move points, or use
the polygonal tool. Neither tool changes the source pixels or invents detail.

## Validation
Native Linux/Qt compilation and automated event/alpha tests completed. Magnetic
traces were visually checked against the supplied Belushi stereo image in both
eyes. The Mac build must still be tested on your Mac.

---

# StereoMask Smart Brush R3

## Install

Unzip StereoMask-Brush-Mac-R3 into Downloads and run:

```bash
bash "$HOME/Downloads/StereoMask-Brush-Mac-R3/BUILD_BRUSH_VERSION.command"
```

The app is build-mac-brush/StereoMask.app inside that new folder. Window title:
StereoMask Smart Brush R3. Existing installed apps and source folders are kept.

## Find the edges of a protruding leg

1. Open your existing .msk project; retain your frame and depth.
2. In Outside frame choose Smart box. Drag a loose box around ONE leg in ONE
   eye's image. Include surrounding background; do not hug the leg's edge.
3. Choose Keep hint. Paint a short stroke safely INSIDE the leg or shoe. Reduce
   size with [ if needed. This is a certainty mark, not an outline. Do not use
   Keep to roughly cover scenery; every Keep pixel is preserved.
4. Choose Remove hint and mark nearby bench/scenery if necessary. Do not mark
   the actual leg; Remove pixels are excluded from the brush selection.
5. Click Find edges. It finds a local cutout from original photo pixels, rough
   existing paint, and these hints. Green overlay shows the resulting reveal.
6. Correct unwanted areas with Remove hint, or missing areas with Keep hint,
   then click Find edges again. Erase hints deletes an incorrect hint stroke.
7. Repeat for the other eye and each other leg. Find edges operates on the eye
   you most recently boxed/painted. Its active box has a cyan outline.

Refinement replaces brush reveal ONLY inside the selected box. It retains all
paint outside that box and never modifies the frame or its depth. The frame
opening always survives, even if Remove hints overlap it. Give disconnected
objects separate Keep strokes. Clear hints clears hints in the active box only.

Existing sloppy paint is merely probable foreground; the algorithm can remove
it. A small explicit Keep stroke is required so it knows which object matters.
Keep, Remove, boxes, and edge refinements can be undone/redone. Save stores
per-eye hint PNGs and boxes with the masks in the .msk sidecar. Reopening restores
these. Hint colors, box outlines, and cursor are never exported.

The full photo stays visible. [ / ] resize brushes. Reveal brush and Erase reveal
still do manual touch-ups. Export as PNG for transparent background, or choose
black, white, a custom color, or Eyedropper. The startup/QtSvg fixes from R2 remain.

## Precision and processing

This uses OpenCV GrabCut locally, with no AI model download. Similar-colored
foreground/background may require extra Keep/Remove strokes. Find edges runs in
the background. Photo opening is blocked until it finishes; stale results are
discarded if Undo/Redo changes the input during processing.
Small boxes use full source resolution. Very large boxes are processed with a
1600-pixel maximum dimension to bound memory; their masks are resized back to
source dimensions. Use a smaller box for fine detail. The box rim supplies
background samples, so leave background space around the object.

A Mac rebuild and hands-on photo test are still required. Native test results
and implementation details are recorded in DEVELOPMENT_LOG.md.

---

Previous R2 and upstream documentation follows for reference.

# StereoMask Brush R2 for Apple Silicon Macs

Run BUILD_BRUSH_VERSION.command to rebuild using the Qt/OpenCV already installed.
The new app is in build-mac-brush/StereoMask.app. Keep the earlier app until tested.
This package builds separately from StereoMask-Brush-Mac and StereoMask-Mac-Setup.

## Painting

Open your existing .msk project. Select Reveal brush and paint each eye separately.
The complete source photo stays visible. A light green overlay marks preserved
brush strokes; this overlay and the brush outline never appear in exports.
Erase reveal removes only those strokes. The frame opening always survives.
The brush circle shows its actual diameter at the current zoom.
Press [ for smaller or ] for larger, or type a diameter in Brush size.

Export background affects the export, not the working image: transparent PNG,
black, white, or Choose color. Choose color opens a color picker. Eyedropper
lets you click an original color in either photo; Escape cancels sampling.
For transparent output use a .png filename. Save keeps frame, per-eye paint,
and custom color in the .msk project. Existing projects get a one-time backup
before this version writes them; new writes are atomic.

## Local startup fix

The app launcher includes the OpenMP environment setting verified on Ted's Mac.
It relies on /opt/homebrew/opt/libomp/lib and existing Homebrew dependencies.
This is a local working build; self-contained distribution remains unverified.
The script restores the binary before a repeated build, deploys Qt, then creates
the launcher. It does not modify the original app in Applications.

Validation: build/launcher shell syntax and launcher environment/argument test
passed in Linux. Qt/OpenCV development libraries are unavailable here, so C++
compilation and interactive testing must be completed on the Mac.

---

Original upstream README follows; original author and MIT license retained.

# StereoMask v2.0.0

A precision masking tool for side-by-side (SBS) stereo images, developed with **GEMINI CLI** and Qt6.

![StereoMask screenshot](Screenshot.jpg)

## New in v2.0.0

- **OpenCV AutoMask:** Native MinGW/OpenCV stereo matching creates editable mask points from SBS image overlap.
- **Depth-Aware Auto Points:** Auto-generated points carry per-point disparity so the right-eye mask follows depth offsets.
- **Freehand Mask Drawing:** Draw masks directly with a simplified editable polygon output.
- **Freehand Cleanup:** Freehand strokes are smoothed, simplified, and capped to a practical point count.
- **Undoable Mask Creation:** AutoMask and Freehand mask creation integrate with the existing undo stack.
- **Bundled OpenCV Runtime:** Release builds copy required vcpkg/OpenCV DLLs beside the application.

## New in v1.3.0

- **Grouped Two-Row Toolbar:** Cleaner File/View and Edit/Mask toolbars with grouped controls.
- **Custom Scalable Icons:** Replaced emoji and legacy 16px icons with a consistent in-app vector icon set.
- **Curve-Aware Rotation:** Curve and Rx/Ry/Rz controls are grouped together; rotation is enabled only for curved selections.
- **Async Export:** Export runs off the UI thread and reports success/failure accurately.
- **Safer Image Handling:** Added SBS dimension validation and bounded image allocation.

## New in v1.2

- **Refined Mask Feathering:** Soften mask edges with pixel-perfect precision (0-200px) without affecting image borders.
- **Snapping Toggle:** Quickly toggle point snapping on/off via the toolbar or shortcut (**`S`**).
- **Export Progress Tracking:** Real-time feedback in the status bar during image export.
- **Smart Folder Persistence:** Automatically remembers the last used directory for all file operations.
- **Optimized Rendering:** Improved masking engine with better memory safety and performance capping.

## Core Features

- **Grouped Toolbar:** Intuitive controls for file operations, editing, masking, and transforms.
- **Precision Masking:** Add, move, and multi-select points to define custom masks for 3D images.
- **OpenCV AutoMask:** Generate editable mask polygons from stereo disparity using native C++ OpenCV.
- **Freehand Masking:** Draw an initial mask shape by hand, then refine the generated control points.
- **Masked Anaglyph Preview:** Real-time 3D preview of your mask using Red/Cyan channels, supporting both parallel and cross-eye sources.
- **Project Persistence (.msk):** Save your work as mask projects that store point data and project-specific settings.
- **Interleaving Space:** Configure gaps between eye images in exported masks, filled with customizable background colors.
- **AutoSave:** Optional automatic saving of project changes.
- **Recent Files:** Quick access to the last 5 worked-on projects.
- **High-DPI Support:** Fully optimized for 4K and multi-monitor setups.

## Author

**S. Rathinagiri**

## License

This project is licensed under the [MIT License](LICENSE).

## Requirements

- Qt 6.x
- CMake 3.16+
- C++17 Compatible Compiler
- OpenCV 4 built for MinGW via vcpkg (`opencv4:x64-mingw-dynamic`)

## Building

```powershell
$env:PATH = "D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The project expects the vcpkg OpenCV install at `D:\vcpkg\installed\x64-mingw-dynamic`.
The build copies the required OpenCV runtime DLLs into `build\` beside `StereoMask.exe`.

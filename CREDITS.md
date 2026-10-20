# Credits

**Bernd Paksa** created the original **Stereomasken** program and its creative 3D framing and masking ideas. StereoMask is inspired by that work, and acknowledges his contribution to the stereo photography community. Original website: [Stereomasken](https://www.stereomasken.de/index_en.htm).

StereoMask is a collaborative effort between Subbiah Rathinagiri and Theodore Whitten.

Subbiah Rathinagiri (credited in StereoMask's MIT license as S. Rathinagiri) created the modern StereoMask Windows implementation and maintains the upstream PC/Windows project: https://github.com/SRathinaGiri/StereoMask.

Theodore Whitten contributed the Mac port, feature direction and foreground-editing improvements, with Codex assistance: brush preservation/erasing, custom and transparent backgrounds, eyedropper sampling, polygonal lasso, multiple additive/subtractive areas, view shortcuts and this user guide.

The original application describes development with GEMINI CLI and Qt 6. That upstream credit is retained. Qt and OpenCV are third-party dependencies, not authored by the collaborators.

Windows v2.1.0 incorporates these foreground-editing improvements from the Mac source, with Windows shortcuts, documentation and update checking. This source distribution preserves the original MIT license. Binary distributions must also include applicable third-party license notices.

The subsequent Windows work adds independent selection objects and ordered frame holes, applied polygon editing, paired stereo alignment, point gestures and shortcuts, synchronized mask controls, and consistent export-mode preview. The magnetic-lasso and GrabCut helper foundation was imported from Theodore's Mac work; it is not claimed as a newly authored Windows engine. Mac R11 ports these Windows v2.1.1 additions back to the Mac, preserving Mac packaging, documentation and update checking. The original comparison is an R10 baseline, not a statement of current feature differences.

Mac R21 integrates Subbiah's Windows v2.4.0–v2.5.0 toolbox drawers, Quick Access favorites, versioned help, alignment grids and three-band outline frames. The outline/grid work continues the Stereomasken inspiration. Mac adaptations preserve native navigation and packaging, use wider drawers and start with File only at Theodore's request. This is reciprocal collaboration, not a claim that the Windows additions originated in the Mac port.

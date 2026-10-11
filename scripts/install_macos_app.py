#!/usr/bin/env python3
"""Stage a verified app before replacing the permanent Applications copy."""
import argparse
import plistlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def verify(app):
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)], check=True)


def copy_bundle(source, target):
    subprocess.run(['/usr/bin/ditto', str(source), str(target)], check=True)


def install(source, target, copier=copy_bundle, verifier=verify):
    source, target = Path(source), Path(target)
    if source.resolve() == target.resolve() or source.is_symlink() or target.is_symlink():
        raise RuntimeError('Source and destination must be separate real app bundles.')
    def identity(app):
        with (app / 'Contents/Info.plist').open('rb') as stream:
            return plistlib.load(stream).get('CFBundleIdentifier')
    expected = 'org.stereomask.brush'
    if identity(source) != expected or (target.exists() and identity(target) != expected):
        raise RuntimeError('Refusing to replace a bundle with an unexpected application identity.')
    verifier(source)
    # Stage on the destination filesystem so each rename is atomic.
    with tempfile.TemporaryDirectory(prefix='.stereomask-install-', dir=target.parent) as folder:
        stage = Path(folder) / 'StereoMask.app'
        previous = Path(folder) / 'previous.app'
        copier(source, stage)
        verifier(stage)
        had_previous = target.exists()
        if had_previous:
            target.rename(previous)
        try:
            stage.rename(target)
            verifier(target)
        except BaseException:
            if target.exists():
                shutil.rmtree(target)
            if had_previous:
                previous.rename(target)
            raise
    print(f'Installed: {target}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path, nargs='?', default=Path('/Applications/StereoMask.app'))
    args = parser.parse_args()
    if sys.platform != 'darwin':
        raise RuntimeError('The installer requires macOS.')
    running = subprocess.run(['/bin/ps', '-axo', 'comm='], check=True, text=True, capture_output=True).stdout
    if any(line.strip().endswith('/Contents/MacOS/StereoMask') for line in running.splitlines()):
        raise RuntimeError('Save your project and quit all StereoMask windows before installing.')
    try:
        install(args.source, args.target)
    except PermissionError as error:
        raise RuntimeError('Applications is not writable. Install the built app with Finder and choose Replace.') from error


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(f'Installation stopped: {error}', file=sys.stderr)
        sys.exit(1)

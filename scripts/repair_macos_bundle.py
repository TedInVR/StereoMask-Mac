#!/usr/bin/env python3
"""Resolve every non-system Mach-O dependency inside a deployed app.

macdeployqt can leave versioned Homebrew aliases pointing outside the bundle.
Match those aliases by Mach-O UUID before rewriting all edges to loader-relative
paths. Never suppress OpenMP's duplicate-runtime check.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

MACHO = {b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
         b'\xfe\xed\xfa\xce', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
         b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'}


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


def is_macho(path):
    if not path.is_file():
        return False
    with path.open('rb') as f:
        return f.read(4) in MACHO


def uuids(path):
    return tuple(sorted(re.findall(r'UUID: ([0-9A-Fa-f-]+)', run('/usr/bin/dwarfdump', '--uuid', path))))


def dependencies(path):
    # otool -l distinguishes LC_LOAD_DYLIB from the library's own LC_ID_DYLIB.
    commands = re.split(r'Load command \d+\n', run('/usr/bin/otool', '-l', path))
    result = []
    for block in commands:
        if re.search(r'cmd LC_(?:LOAD|LOAD_WEAK|REEXPORT|LAZY_LOAD|LOAD_UPWARD)_DYLIB\b', block):
            name = re.search(r'\n\s*name (.*?) \(offset \d+\)', block)
            if name:
                result.append(name.group(1))
    return result


def system_dependency(name):
    return name.startswith(('/usr/lib/', '/System/Library/'))


class Bundle:
    def __init__(self, app):
        self.app = app.resolve()
        self.executable_dir = self.app / 'Contents/MacOS'
        self.files = sorted({p.resolve() for p in self.app.rglob('*') if is_macho(p)})
        if not self.files:
            raise RuntimeError('No Mach-O files found in the app.')
        self.by_uuid = {}
        self.by_name = {}
        self.uuid_cache = {}
        for path in self.files:
            uid = uuids(path)
            if uid:
                self.by_uuid.setdefault(uid, []).append(path)
            self.by_name.setdefault(path.name, []).append(path)

    def inside(self, path):
        return path == self.app or self.app in path.parents

    def resolve(self, owner, name):
        candidate = None
        if name.startswith('@loader_path/'):
            candidate = owner.parent / name[len('@loader_path/'):]
        elif name.startswith('@executable_path/'):
            candidate = self.executable_dir / name[len('@executable_path/'):]
        elif name.startswith('/'):
            candidate = Path(name)
        if candidate is not None and candidate.exists():
            candidate = candidate.resolve()
            if self.inside(candidate) and candidate in self.files:
                return candidate
            # An absolute Homebrew alias can have a completely different basename.
            # UUID matching proves it is the same library already copied by Qt.
            if candidate not in self.uuid_cache:
                self.uuid_cache[candidate] = uuids(candidate)
            matches = self.by_uuid.get(self.uuid_cache[candidate], [])
            if len(matches) == 1:
                return matches[0]
            if len(matches) > 1:
                raise RuntimeError('Duplicate bundled library UUID: ' + name)
        matches = self.by_name.get(Path(name).name, [])
        if len(matches) == 1:
            return matches[0]
        raise RuntimeError('Cannot resolve bundled dependency: %s -> %s' % (owner, name))

    def plan(self):
        changes = []
        for owner in self.files:
            for name in dependencies(owner):
                if system_dependency(name):
                    continue
                target = self.resolve(owner, name)
                new = '@loader_path/' + os.path.relpath(target, owner.parent)
                if name != new:
                    changes.append((owner, name, new))
        return changes

    def verify(self):
        for owner in self.files:
            for name in dependencies(owner):
                if system_dependency(name):
                    continue
                if not name.startswith('@loader_path/'):
                    raise RuntimeError('External or unresolved dependency remains: ' + name)
                target = (owner.parent / name[len('@loader_path/'):]).resolve()
                if not self.inside(target) or target not in self.files:
                    raise RuntimeError('Dependency points outside bundle: ' + name)
        omp = [p for p in self.files if p.name.startswith('libomp') and p.suffix == '.dylib']
        if len(omp) > 1:
            raise RuntimeError('More than one OpenMP runtime is bundled.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Run this packaging tool on macOS.')
    bundle = Bundle(args.app)
    if not args.check_only:
        changes = bundle.plan()  # Resolve the whole graph before modifying any file.
        for owner, old, new in changes:
            run('/usr/bin/install_name_tool', '-change', old, new, owner)
        print('Repaired %d dependency references.' % len(changes))
    bundle.verify()
    if not args.check_only:
        # Dependency edits invalidate signatures. Sign nested binaries, then bundle.
        for path in bundle.files:
            run('/usr/bin/codesign', '--force', '--sign', '-', path)
        run('/usr/bin/codesign', '--force', '--deep', '--sign', '-', bundle.app)
        run('/usr/bin/codesign', '--verify', '--deep', '--strict', bundle.app)
    print('Verified: all non-system dependencies resolve inside the app; no duplicate OpenMP runtime.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print('Packaging failed: %s' % exc, file=sys.stderr)
        sys.exit(1)

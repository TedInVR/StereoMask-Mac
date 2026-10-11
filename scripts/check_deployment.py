"""Allow only errors handled by the subsequent strict bundle repair."""
from pathlib import Path
import sys


def check(log, status, app):
    svg = Path(app) / 'Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg'
    deferred = False
    svg_context = False
    failures = []
    for line in log.splitlines():
        if not line.startswith('ERROR:'):
            continue
        if line == 'ERROR: Cannot resolve rpath "@rpath/QtSvg.framework/Versions/A/QtSvg"' and svg.is_file():
            deferred = True
            svg_context = True
        elif svg_context and line.startswith('ERROR:  using QList('):
            svg_context = False
        elif line == 'ERROR: codesign verification error:' or 'invalid signature (code or signature have been modified)' in line:
            deferred = True
            svg_context = False
        else:
            failures.append(line)
            svg_context = False
    if failures or (status != 0 and not deferred):
        raise RuntimeError('\n'.join(failures) or 'macdeployqt failed without a recognized repairable error')
    if deferred:
        print('QtSvg/signature checks deferred to mandatory dependency repair and strict signing verification.')


if __name__ == '__main__':
    try:
        check(Path(sys.argv[1]).read_text(), int(sys.argv[2]), sys.argv[3])
    except Exception as error:
        print(f'Packaging dependencies failed: {error}', file=sys.stderr)
        sys.exit(1)

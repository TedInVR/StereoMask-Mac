#!/usr/bin/env python3
"""Per-user, closed-app-only source updater. No network activity while app runs."""
import argparse
import fcntl
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

LABEL = 'org.stereomask.autoupdate'
SUPPORT = Path.home() / 'Library/Application Support/StereoMask Updates'
APP = Path('/Applications/StereoMask.app')


def running():
    processes = subprocess.run(['/bin/ps', '-axo', 'comm='], check=True, capture_output=True, text=True).stdout
    return any(line.strip().endswith('/Contents/MacOS/StereoMask') for line in processes.splitlines())


def version(value):
    if not re.fullmatch(r'\d+\.\d+\.\d+', value):
        raise ValueError('Invalid release version')
    return tuple(map(int, value.split('.')))


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'StereoMask-Mac-Updater'})
    with urllib.request.urlopen(request, timeout=60) as response:
        if not response.url.startswith('https://'):
            raise ValueError('HTTPS required')
        return response.read()


def extract(data, destination):
    archive = destination / 'source.zip'
    archive.write_bytes(data)
    with zipfile.ZipFile(archive) as source:
        total = 0
        for entry in source.infolist():
            path = Path(entry.filename)
            total += entry.file_size
            if path.is_absolute() or '..' in path.parts or total > 100_000_000:
                raise ValueError('Unsafe source archive')
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('Source symlinks not supported')
        source.extractall(destination / 'source')
    roots = list((destination / 'source').iterdir())
    if len(roots) != 1 or not (roots[0] / 'CMakeLists.txt').is_file():
        raise ValueError('Invalid source package')
    return roots[0]


def install_agent(source):
    SUPPORT.mkdir(parents=True, exist_ok=True)
    helper = SUPPORT / 'automatic_updates.py'
    shutil.copy2(source, helper)
    python = "/opt/homebrew/bin/python3" if Path("/opt/homebrew/bin/python3").exists() else sys.executable
    agent = Path.home() / 'Library/LaunchAgents' / (LABEL + '.plist')
    agent.parent.mkdir(parents=True, exist_ok=True)
    config = {'Label': LABEL, 'ProgramArguments': [python, str(helper)],
              'StartInterval': 900, 'RunAtLoad': True,
              'StandardOutPath': str(SUPPORT / 'updater.log'),
              'StandardErrorPath': str(SUPPORT / 'updater.log')}
    with agent.open('wb') as stream:
        plistlib.dump(config, stream)
    # Register only in the signed-in user's GUI session.
    subprocess.run(['/bin/launchctl', 'bootout', 'gui/' + str(os.getuid()), str(agent)], capture_output=True)
    subprocess.run(['/bin/launchctl', 'bootstrap', 'gui/' + str(os.getuid()), str(agent)], check=True)


def update():
    if running():
        return
    SUPPORT.mkdir(parents=True, exist_ok=True)
    preference = SUPPORT / 'preferences.json'
    if preference.exists() and not json.loads(preference.read_text()).get('enabled', True):
        return
    with (SUPPORT / 'update.lock').open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        if not APP.exists():
            return
        with (APP / 'Contents/Info.plist').open('rb') as stream:
            installed = plistlib.load(stream).get('CFBundleShortVersionString', '0.0.0')
        # Pin the download to the exact inspected commit, not a moving branch.
        commit = json.loads(fetch('https://api.github.com/repos/TedInVR/StereoMask-Mac/commits/main'))['sha']
        if not re.fullmatch(r'[0-9a-f]{40}', commit):
            raise ValueError('Invalid commit')
        notice = json.loads(fetch('https://raw.githubusercontent.com/TedInVR/StereoMask-Mac/' + commit + '/updates.json'))
        if version(notice['version']) <= version(installed):
            return
        if running():
            return
        with tempfile.TemporaryDirectory(prefix='staged-', dir=SUPPORT) as folder:
            root = Path(folder)
            source = extract(fetch('https://github.com/TedInVR/StereoMask-Mac/archive/' + commit + '.zip'), root)
            declaration = (source / 'appversion.h').read_text()
            if '"' + notice['version'] + '"' not in declaration:
                raise ValueError('Package version does not match release notice')
            if running():
                return
            env = dict(os.environ, STEREOMASK_BACKGROUND_UPDATE='1')
            # Build script stages/verifies and atomically replaces the app. It rechecks
            # that the app is closed immediately before installation. Never auto-launch.
            subprocess.run(['/bin/bash', str(source / 'BUILD_BRUSH_VERSION.command')], env=env, check=True)
            helper = SUPPORT / 'automatic_updates.py'
            staged_helper = SUPPORT / 'automatic_updates.next.py'
            shutil.copy2(source / 'scripts/automatic_updates.py', staged_helper)
            staged_helper.replace(helper)
            print('Installed StereoMask ' + notice['version'] + ' from ' + commit, flush=True)


if __name__ == '__main__':
    try:
        if '--install-agent' in sys.argv:
            install_agent(Path(__file__).resolve())
        else:
            update()
    except Exception as error:
        print('Update deferred; current app retained: ' + str(error), file=sys.stderr, flush=True)
        sys.exit(1)

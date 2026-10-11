import importlib.util
import plistlib
from pathlib import Path
import shutil
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('installer', Path(__file__).parents[1] / 'scripts/install_macos_app.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        root = Path(self.temp.name)
        self.source, self.target = root / 'build/StereoMask.app', root / 'Applications/StereoMask.app'
        self.target.parent.mkdir()
        self.bundle(self.source, 'new')
        self.bundle(self.target, 'old')

    def tearDown(self):
        self.temp.cleanup()

    def bundle(self, path, value, identity='org.stereomask.brush'):
        (path / 'Contents').mkdir(parents=True, exist_ok=True)
        (path / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier': identity}))
        (path / 'version').write_text(value)

    def test_replacement_removes_old_only_files(self):
        (self.target / 'obsolete').write_text('old library')
        installer.install(self.source, self.target, shutil.copytree, lambda _: None)
        self.assertEqual((self.target / 'version').read_text(), 'new')
        self.assertFalse((self.target / 'obsolete').exists())

    def test_bad_staged_signature_leaves_installed_app_untouched(self):
        def check(app):
            if app != self.source:
                raise RuntimeError('bad signature')
        with self.assertRaisesRegex(RuntimeError, 'bad signature'):
            installer.install(self.source, self.target, shutil.copytree, check)
        self.assertEqual((self.target / 'version').read_text(), 'old')

    def test_final_verification_failure_rolls_back(self):
        def check(app):
            if app == self.target:
                raise RuntimeError('verify after move')
        with self.assertRaisesRegex(RuntimeError, 'verify after move'):
            installer.install(self.source, self.target, shutil.copytree, check)
        self.assertEqual((self.target / 'version').read_text(), 'old')

    def test_wrong_identity_is_not_overwritten(self):
        self.bundle(self.target, 'other', 'another.app')
        with self.assertRaisesRegex(RuntimeError, 'identity'):
            installer.install(self.source, self.target, shutil.copytree, lambda _: None)
        self.assertEqual((self.target / 'version').read_text(), 'other')

    def test_first_install(self):
        shutil.rmtree(self.target)
        installer.install(self.source, self.target, shutil.copytree, lambda _: None)
        self.assertEqual((self.target / 'version').read_text(), 'new')


if __name__ == '__main__':
    unittest.main()

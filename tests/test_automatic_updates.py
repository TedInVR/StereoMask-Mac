import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec=importlib.util.spec_from_file_location('automatic_updates',Path(__file__).resolve().parents[1]/'scripts/automatic_updates.py')
updater=importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)

class AutomaticUpdateTests(unittest.TestCase):
    def archive(self,name,content='project(StereoMask)'):
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as archive:archive.writestr(name,content)
        return stream.getvalue()
    def test_running_app_prevents_network_and_files(self):
        with patch.object(updater,'running',return_value=True),patch.object(updater,'fetch') as network:
            updater.update()
            network.assert_not_called()
    def test_disabled_prevents_network(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(updater,'SUPPORT',Path(folder)),patch.object(updater,'running',return_value=False),patch.object(updater,'fetch') as network:
            (Path(folder)/'preferences.json').write_text(json.dumps({'enabled':False}))
            updater.update()
            network.assert_not_called()
    def test_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):updater.extract(self.archive('../outside.txt'),Path(folder))
    def test_invalid_archive_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):updater.extract(self.archive('project/README.md'),Path(folder))
    def test_valid_source(self):
        with tempfile.TemporaryDirectory() as folder:
            source=updater.extract(self.archive('project/CMakeLists.txt'),Path(folder))
            self.assertEqual(source.name,'project')
    def test_version_validation(self):
        self.assertGreater(updater.version('2.6.0'),updater.version('2.5.1'))
        with self.assertRaises(ValueError):updater.version('2.6.0; command')
    def test_symlink_rejected(self):
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as archive:
            item=zipfile.ZipInfo('project/link');item.external_attr=0o120777<<16;archive.writestr(item,'/tmp/outside')
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):updater.extract(stream.getvalue(),Path(folder))

if __name__=='__main__':unittest.main()

"""Dependency-graph tests without requiring an actual Mac or modifying libraries."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('repair', Path(__file__).parents[1] / 'scripts/repair_macos_bundle.py')
repair = importlib.util.module_from_spec(spec)
spec.loader.exec_module(repair)


class BundleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.app = self.root / 'StereoMask.app'
        self.exe = self.make('Contents/MacOS/StereoMask')
        self.core = self.make('Contents/Frameworks/libopencv_core.500.dylib')
        self.omp = self.make('Contents/Frameworks/libomp.dylib')
        self.plugin = self.make('Contents/PlugIns/platforms/libqcocoa.dylib')
        self.alias = self.root / 'homebrew/libopencv_core.5.0.0.dylib'
        self.alias.parent.mkdir(); self.alias.write_bytes(b'\xcf\xfa\xed\xfe')
        self.uids = {self.exe: ('EXE',), self.core: ('CORE',), self.alias: ('CORE',),
                     self.omp: ('OMP',), self.plugin: ('PLUGIN',)}
        self.edges = {self.exe: ['@rpath/libopencv_core.500.dylib'],
                      self.core: ['@rpath/libomp.dylib', '/usr/lib/libSystem.B.dylib'],
                      self.omp: ['/usr/lib/libSystem.B.dylib'],
                      self.plugin: [str(self.alias)]}
        self.uuid_patch = patch.object(repair, 'uuids', side_effect=lambda p: self.uids[p]); self.uuid_patch.start()
        self.dep_patch = patch.object(repair, 'dependencies', side_effect=lambda p: self.edges[p]); self.dep_patch.start()

    def tearDown(self):
        self.uuid_patch.stop(); self.dep_patch.stop(); self.temp.cleanup()

    def make(self, relative):
        p = self.app / relative; p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b'\xcf\xfa\xed\xfe'); return p.resolve()

    def test_alias_is_relinked_to_same_bundled_uuid(self):
        bundle = repair.Bundle(self.app)
        changes = bundle.plan()
        for owner, old, new in changes:
            self.edges[owner][self.edges[owner].index(old)] = new
        bundle.verify()
        self.assertEqual(self.edges[self.plugin], ['@loader_path/../../Frameworks/libopencv_core.500.dylib'])
        self.assertEqual(self.edges[self.core][0], '@loader_path/libomp.dylib')
        self.assertEqual(len(changes), 3)

    def test_missing_dependency_stops_before_any_mutation(self):
        self.edges[self.exe].append('@rpath/missing.dylib')
        with self.assertRaisesRegex(RuntimeError, 'Cannot resolve'):
            repair.Bundle(self.app).plan()

    def test_external_reference_fails_verification(self):
        with self.assertRaisesRegex(RuntimeError, 'External or unresolved'):
            repair.Bundle(self.app).verify()

    def test_loader_path_cannot_escape_bundle(self):
        self.edges[self.exe] = ['@loader_path/../../../../homebrew/libopencv_core.5.0.0.dylib']
        self.edges[self.core] = []; self.edges[self.plugin] = []
        with self.assertRaisesRegex(RuntimeError, 'outside bundle'):
            repair.Bundle(self.app).verify()

    def test_duplicate_openmp_runtime_is_rejected(self):
        extra = self.make('Contents/Frameworks/libomp-copy.dylib')
        self.uids[extra] = ('OMP',); self.edges[extra] = []
        self.edges[self.exe] = []; self.edges[self.core] = []; self.edges[self.plugin] = []
        with self.assertRaisesRegex(RuntimeError, 'More than one OpenMP'):
            repair.Bundle(self.app).verify()

    def test_symlink_is_not_counted_as_another_loaded_library(self):
        (self.omp.parent / 'libomp-alias.dylib').symlink_to(self.omp.name)
        self.assertEqual(len(repair.Bundle(self.app).files), 4)

    def test_parser_excludes_library_id_and_handles_spaces(self):
        sample = '''header
Load command 1
          cmd LC_ID_DYLIB
         name @rpath/own.dylib (offset 24)
Load command 2
          cmd LC_LOAD_DYLIB
         name /a path/libomp.dylib (offset 24)
Load command 3
          cmd LC_LOAD_WEAK_DYLIB
         name /usr/lib/libSystem.B.dylib (offset 24)
'''
        self.dep_patch.stop()
        with patch.object(repair, 'run', return_value=sample):
            self.assertEqual(repair.dependencies(self.core), ['/a path/libomp.dylib', '/usr/lib/libSystem.B.dylib'])
        self.dep_patch.start()


if __name__ == '__main__':
    unittest.main()

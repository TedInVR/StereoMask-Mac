import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('deployment', Path(__file__).parents[1] / 'scripts/check_deployment.py')
deployment = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deployment)


class DeploymentTest(unittest.TestCase):
    def test_only_present_svg_can_be_deferred(self):
        log = 'ERROR: Cannot resolve rpath "@rpath/QtSvg.framework/Versions/A/QtSvg"\nERROR:  using QList("/opt/homebrew")'
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(RuntimeError):
                deployment.check(log, 1, folder)
            svg = Path(folder) / 'Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg'
            svg.parent.mkdir(parents=True)
            svg.write_bytes(b'fixture')
            deployment.check(log, 1, folder)
            with self.assertRaises(RuntimeError):
                deployment.check(log + '\nERROR: Cannot resolve rpath "@rpath/Other"', 1, folder)

    def test_unrelated_search_list_and_silent_failure_stop(self):
        for log, status in [('ERROR:  using QList("unknown")', 0), ('', 1)]:
            with self.assertRaises(RuntimeError):
                deployment.check(log, status, '.')

    def test_signature_only_is_deferred(self):
        deployment.check('ERROR: codesign verification error:\nERROR: "invalid signature (code or signature have been modified)"', 1, '.')

    def test_success(self):
        deployment.check('', 0, '.')


if __name__ == '__main__':
    unittest.main()

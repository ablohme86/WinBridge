import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import winbridge as app


class WinBridgeTests(unittest.TestCase):
    def test_external_library_and_custom_proton(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'Steam'
            external = Path(tmp) / 'Extra Library'
            (root / 'steamapps').mkdir(parents=True)
            proton = external / 'steamapps/common/Proton Test'
            proton.mkdir(parents=True)
            (proton / 'proton').touch()
            (root / 'steamapps/libraryfolders.vdf').write_text(f'"path" "{external}"')
            self.assertEqual(app.discover([root], app.libraries([root])), [proton])

    def test_runtime_resolution_and_missing_dependency(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proton = root / 'Proton'
            proton.mkdir()
            (proton / 'toolmanifest.vdf').write_text('"require_tool_appid" "123"')
            with self.assertRaises(RuntimeError):
                app.runtime_for(proton, [root])
            runtime = root / 'steamapps/common/Runtime/_v2-entry-point'
            runtime.parent.mkdir(parents=True)
            runtime.touch()
            (root / 'steamapps/appmanifest_123.acf').write_text('"installdir" "Runtime"')
            self.assertEqual(app.runtime_for(proton, [root]), runtime)

    def test_launch_preserves_arguments_and_persistent_prefix(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proton = root / 'Proton'
            proton.mkdir()
            script = proton / 'proton'
            script.write_text('#!/usr/bin/python3\nimport os,sys,json\nfrom pathlib import Path\nPath("captured.json").write_text(json.dumps([sys.argv[1:], os.environ["STEAM_COMPAT_DATA_PATH"]]))\n')
            script.chmod(0o755)
            exe = root / 'app $(never execute).exe'
            exe.touch()
            with patch.dict(os.environ, {'XDG_DATA_HOME': tmp, 'XDG_STATE_HOME': tmp, 'XDG_CONFIG_HOME': tmp}):
                app.launch(exe, proton, [root], [root], ['argument with spaces', '$literal'])
                import json
                args, prefix = json.loads((root / 'captured.json').read_text())
                self.assertEqual(args, ['run', str(exe), 'argument with spaces', '$literal'])
                other = root / 'standalone' / 'portable.exe'
                other.parent.mkdir()
                other.touch()
                app.launch(other, proton, [root], [root])
                self.assertEqual(json.loads((other.parent / 'captured.json').read_text())[1], prefix)

    def test_cancel_does_not_launch(self):
        with patch.object(app, 'dialog_tool', return_value='zenity'), patch.object(app.subprocess, 'run') as run:
            run.return_value.returncode = 1
            self.assertIsNone(app.choose([Path('/proton')], Path('/app.exe')))


if __name__ == '__main__':
    unittest.main()

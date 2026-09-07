import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from shortcuts import (
    desktop_quote, enclosing_prefix, group_shortcuts_by_program,
    import_shortcuts, read_shortcuts_config, save_shortcuts_config,
    shortcut_target, toggle_shortcut
)


class ShortcutTests(unittest.TestCase):
    def test_import_and_repeat(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prefix = root / 'prefix'
            source = prefix / 'pfx/drive_c/proton_shortcuts'
            source.mkdir(parents=True)
            target = prefix / 'pfx/drive_c/users/Public/Desktop/Example.lnk'
            target.parent.mkdir(parents=True)
            target.touch()
            win = r'C:\users\Public\Desktop\Example.lnk'
            (source / 'example.desktop').write_text('[Desktop Entry]\nName=Example\nExec=' + desktop_quote(win) + '\n')
            for _ in range(2):
                self.assertEqual(import_shortcuts(prefix, root / 'Proton', root / 'launcher.py', root / 'data', root / 'desktop'), ['Example'])
            apps = list((root / 'data/applications').glob('*.desktop'))
            self.assertEqual(len(apps), 1)
            content = apps[0].read_text()
            self.assertIn('--prefix', content)
            self.assertIn('--proton', content)
            self.assertIn(str(target), content)
            self.assertEqual(enclosing_prefix(target), prefix)
            self.assertEqual(len(list((root / 'desktop').glob('*.desktop'))), 1)

    def test_reject_command_and_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            prefix = Path(tmp)
            self.assertIsNone(shortcut_target('sh -c "touch /tmp/bad"', prefix))
            self.assertIsNone(shortcut_target(desktop_quote(r'C:\..\..\outside.exe'), prefix))

    def test_exec_escaping(self):
        self.assertEqual(desktop_quote('100% game'), '"100%% game"')
        self.assertIn('\\\\$', desktop_quote('$HOME'))

    def test_shortcuts_config_and_toggle(self):
        with tempfile.TemporaryDirectory() as tmp:
            conf_dir = Path(tmp) / 'config'
            with patch.dict(os.environ, {'XDG_CONFIG_HOME': str(conf_dir)}):
                self.assertEqual(read_shortcuts_config(), {})
                save_shortcuts_config({'sc1': {'desktop': False, 'menu': True}})
                self.assertEqual(read_shortcuts_config(), {'sc1': {'desktop': False, 'menu': True}})

                res = toggle_shortcut(tmp, 'sc1', desktop=True, menu=False, launcher=Path(tmp) / 'l.py', proton=None)
                self.assertTrue(res['desktop'])
                self.assertFalse(res['menu'])
                self.assertEqual(read_shortcuts_config()['sc1'], {'desktop': True, 'menu': False})

    def test_import_shortcuts_respects_visibility_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            conf_dir = root / 'config'
            prefix = root / 'prefix'
            source = prefix / 'pfx/drive_c/proton_shortcuts'
            source.mkdir(parents=True)
            target = prefix / 'pfx/drive_c/users/Public/Desktop/App.lnk'
            target.parent.mkdir(parents=True)
            target.touch()
            win = r'C:\users\Public\Desktop\App.lnk'
            (source / 'app.desktop').write_text('[Desktop Entry]\nName=App\nExec=' + desktop_quote(win) + '\n')

            with patch.dict(os.environ, {'XDG_CONFIG_HOME': str(conf_dir)}):
                # 1. Default: imported to both menu and desktop
                import_shortcuts(prefix, root / 'Proton', root / 'launcher.py', root / 'data', root / 'desktop')
                apps = list((root / 'data/applications').glob('*.desktop'))
                desks = list((root / 'desktop').glob('*.desktop'))
                self.assertEqual(len(apps), 1)
                self.assertEqual(len(desks), 1)
                sc_id = apps[0].stem.replace('winbridge-', '')

                # 2. Toggle desktop off
                toggle_shortcut(prefix, sc_id, desktop=False, launcher=root / 'launcher.py', proton=None)
                import_shortcuts(prefix, root / 'Proton', root / 'launcher.py', root / 'data', root / 'desktop')
                self.assertEqual(len(list((root / 'data/applications').glob('*.desktop'))), 1)
                self.assertEqual(len(list((root / 'desktop').glob('*.desktop'))), 0)

                # 3. Toggle menu off
                toggle_shortcut(prefix, sc_id, menu=False, launcher=root / 'launcher.py', proton=None)
                import_shortcuts(prefix, root / 'Proton', root / 'launcher.py', root / 'data', root / 'desktop')
                self.assertEqual(len(list((root / 'data/applications').glob('*.desktop'))), 0)
                self.assertEqual(len(list((root / 'desktop').glob('*.desktop'))), 0)

    def test_group_shortcuts_by_program(self):
        with tempfile.TemporaryDirectory() as tmp:
            prefix = Path(tmp)
            source = prefix / 'pfx/drive_c/proton_shortcuts'
            source.mkdir(parents=True)
            target = prefix / 'pfx/drive_c/Program Files/TestApp/test.exe'
            target.parent.mkdir(parents=True)
            target.touch()
            win = r'C:\Program Files\TestApp\test.exe'
            (source / 'test.desktop').write_text('[Desktop Entry]\nName=Test App\nExec=' + desktop_quote(win) + '\n')
            programs = [{'key': 'testapp_key', 'name': 'Test App'}]
            grouped = group_shortcuts_by_program(prefix, programs)
            self.assertEqual(len(grouped), 1)
            self.assertEqual(len(grouped[0]['shortcuts']), 1)
            self.assertEqual(grouped[0]['shortcuts'][0]['name'], 'Test App')


if __name__ == '__main__':
    unittest.main()


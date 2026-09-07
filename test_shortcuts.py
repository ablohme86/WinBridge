import tempfile
from pathlib import Path
import unittest
from shortcuts import desktop_quote, enclosing_prefix, import_shortcuts, shortcut_target


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


if __name__ == '__main__':
    unittest.main()

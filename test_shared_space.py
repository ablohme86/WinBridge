import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
from shared_space import read_settings, select_proton, shared_prefix
from shortcuts import import_shortcuts, desktop_quote


class SharedSpaceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        env = patch.dict(os.environ, {'XDG_DATA_HOME': self.tmp.name, 'XDG_CONFIG_HOME': self.tmp.name})
        env.start()
        self.addCleanup(env.stop)
        self.proton = self.root / 'Proton'
        self.proton.mkdir()
        (self.proton / 'proton').touch()

    def test_protonrun_settings_and_environment_are_preserved(self):
        import json
        legacy = self.root / 'protonrun/settings.json'
        legacy.parent.mkdir()
        prefix = self.root / 'existing-windows'
        legacy.write_text(json.dumps({'prefix': str(prefix), 'proton': str(self.proton)}))
        self.assertEqual(shared_prefix(), prefix)
        chooser = Mock()
        self.assertEqual(select_proton([self.proton], chooser, Mock()), self.proton)
        chooser.assert_not_called()

    def test_first_selection_is_remembered_and_can_change(self):
        chooser = Mock(return_value=self.proton)
        validate = Mock()
        self.assertEqual(select_proton([self.proton], chooser, validate), self.proton)
        self.assertEqual(select_proton([self.proton], chooser, validate), self.proton)
        chooser.assert_called_once()
        select_proton([self.proton], chooser, validate, change=True)
        self.assertEqual(chooser.call_count, 2)
        self.assertEqual(shared_prefix(), self.root / 'winbridge/shared')

    def test_cancel_and_invalid_runtime_do_not_save(self):
        self.assertIsNone(select_proton([self.proton], lambda _: None, Mock()))
        self.assertEqual(read_settings(), {})
        with self.assertRaises(RuntimeError):
            select_proton([self.proton], lambda _: self.proton, Mock(side_effect=RuntimeError('missing runtime')))
        self.assertEqual(read_settings(), {})

    def test_missing_proton_prompts_again(self):
        select_proton([self.proton], lambda _: self.proton, Mock())
        (self.proton / 'proton').unlink()
        replacement = self.root / 'Replacement'
        replacement.mkdir()
        (replacement / 'proton').touch()
        chooser = Mock(return_value=replacement)
        self.assertEqual(select_proton([replacement], chooser, Mock()), replacement)
        chooser.assert_called_once()

    def test_adopt_legacy_and_shortcuts_follow_global_choice(self):
        legacy = self.root / 'winbridge/prefixes/old'
        source = legacy / 'pfx/drive_c/proton_shortcuts'
        source.mkdir(parents=True)
        target = legacy / 'pfx/drive_c/app.exe'
        target.touch()
        (source / 'app.desktop').write_text('[Desktop Entry]\nName=App\nExec=' + desktop_quote(r'C:\app.exe') + '\n')
        self.assertEqual(shared_prefix(), legacy)
        select_proton([self.proton], lambda _: self.proton, Mock())
        import_shortcuts(legacy, self.proton, self.root / 'launcher.py', self.root / 'data')
        content = next((self.root / 'data/applications').glob('*.desktop')).read_text()
        self.assertNotIn('--proton', content)
        self.assertNotIn('--prefix', content)
        self.assertTrue(target.is_file())


if __name__ == '__main__':
    unittest.main()

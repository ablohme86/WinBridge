from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import manager_backend as manager


class ManagerTests(unittest.TestCase):
    def test_parse_programs_ignores_noise_and_keeps_unicode(self):
        self.assertEqual(manager.parse_programs('wine: warning\n{B}|||Øvelse\n{A}|||App\n{A}|||App\n'), [('{A}', 'App'), ('{B}', 'Øvelse')])

    def test_empty_environment_does_not_start_proton(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(manager, 'read_settings', return_value={}), patch.object(manager, 'shared_prefix', return_value=Path(tmp)), patch.object(manager, 'launch') as launch:
            self.assertFalse(manager.operate('list')['ready'])
            launch.assert_not_called()

    def test_proton_settings_validate_and_preserve_prefix(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proton = root / 'Proton'
            proton.mkdir()
            from unittest.mock import MagicMock
            original = {'prefix': '/existing/shared', 'proton': '/old/Proton'}
            with patch.object(manager, 'read_settings', return_value=original.copy()), patch.object(manager, 'steam_roots', return_value=[]), patch.object(manager, 'libraries', return_value=[]), patch.object(manager, 'discover', return_value=[proton]), patch.object(manager, 'runtime_for'), patch.object(manager, 'settings_lock', return_value=MagicMock()), patch.object(manager, 'save_settings') as save:
                result = manager.operate('settings')
                self.assertEqual(result['selected'], '/old/Proton')
                manager.operate('configure', selected_proton=str(proton))
                self.assertEqual(save.call_args.args[0]['prefix'], '/existing/shared')
                self.assertEqual(save.call_args.args[0]['proton'], str(proton))
                with self.assertRaises(RuntimeError):
                    manager.operate('configure', selected_proton='/not/installed')

    def test_translation_keys_and_placeholders_match(self):
        import json
        import re
        base = Path(__file__).parent / 'manager/i18n'
        english = json.loads((base / 'en-US.i18n').read_text())['strings']
        norwegian = json.loads((base / 'no-NB.i18n').read_text())['strings']
        self.assertEqual(english.keys(), norwegian.keys())
        for key in english:
            self.assertEqual(sorted(re.findall(r'%[0-9]+', english[key])), sorted(re.findall(r'%[0-9]+', norwegian[key])))

    def test_uninstall_validates_id_and_uses_shared_environment(self):
        for key in ('key', 'unknown'):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                exe = root / 'pfx/drive_c/windows/system32/uninstaller.exe'
                exe.parent.mkdir(parents=True)
                exe.touch()
                (root / 'proton').touch()
                with patch.object(manager, 'read_settings', return_value={'proton': tmp}), patch.object(manager, 'shared_prefix', return_value=root), patch.object(manager, 'steam_roots', return_value=[]), patch.object(manager, 'libraries', return_value=[]), patch.object(manager, 'launch', side_effect=['key|||App', 0, '']) as launch:
                    if key == 'unknown':
                        with self.assertRaises(RuntimeError):
                            manager.operate('uninstall', key)
                        self.assertEqual(launch.call_count, 1)
                    else:
                        self.assertTrue(manager.operate('uninstall', key)['removed'])
                        self.assertEqual(launch.call_args_list[1].args[4], ['--remove', 'key'])
                        self.assertEqual(launch.call_args_list[1].args[5], root)


if __name__ == '__main__':
    unittest.main()

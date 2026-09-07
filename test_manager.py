from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import manager_backend as manager


class ManagerTests(unittest.TestCase):
    def test_parse_programs_ignores_noise_and_keeps_unicode(self):
        self.assertEqual(manager.parse_programs('wine: warning\n{B}|||Øvelse\n{A}|||App\n{A}|||App\n'), [('{A}', 'App'), ('{B}', 'Øvelse')])

    def test_real_shortcut_icon_uses_largest_resolution(self):
        with tempfile.TemporaryDirectory() as tmp:
            prefix = Path(tmp)
            source = prefix / 'pfx/drive_c/proton_shortcuts'
            for size in (32, 256, 64):
                icon = source / f'icons/{size}x{size}/apps/real-icon.png'
                icon.parent.mkdir(parents=True)
                icon.touch()
            (source / 'App.desktop').write_text('[Desktop Entry]\nName=My App\nIcon=real-icon\n')
            self.assertEqual(manager.program_icons(prefix)['my app'], str(source / 'icons/256x256/apps/real-icon.png'))
            (source / 'App.desktop').write_text('[Desktop Entry]\nName=My App\nIcon=../../outside\n')
            self.assertEqual(manager.program_icons(prefix), {})

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
                self.assertEqual(result['prefix'], '/existing/shared')
                manager.operate('configure', selected_proton=str(proton))
                self.assertEqual(save.call_args.args[0]['prefix'], '/existing/shared')
                self.assertEqual(save.call_args.args[0]['proton'], str(proton))
                with self.assertRaises(RuntimeError):
                    manager.operate('configure', selected_proton='/not/installed')

    def test_prefix_settings_update_and_validate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            from unittest.mock import MagicMock
            original = {'prefix': '/existing/shared', 'proton': '/old/Proton'}
            with patch.object(manager, 'read_settings', return_value=original.copy()), patch.object(manager, 'steam_roots', return_value=[]), patch.object(manager, 'libraries', return_value=[]), patch.object(manager, 'discover', return_value=[]), patch.object(manager, 'settings_lock', return_value=MagicMock()), patch.object(manager, 'save_settings') as save:
                new_prefix = root / 'new_shared'
                manager.operate('configure', selected_prefix=str(new_prefix))
                self.assertEqual(save.call_args.args[0]['prefix'], str(new_prefix.resolve()))
                self.assertEqual(save.call_args.args[0]['proton'], '/old/Proton')
                self.assertTrue(new_prefix.is_dir())

                # Selecting pfx folder normalizes to parent
                pfx_dir = root / 'custom/pfx'
                (pfx_dir / 'drive_c').mkdir(parents=True)
                manager.operate('configure', selected_prefix=str(pfx_dir))
                self.assertEqual(save.call_args.args[0]['prefix'], str((root / 'custom').resolve()))

                # Pointing to a file raises error
                file_target = root / 'regular_file'
                file_target.touch()
                with self.assertRaises(RuntimeError):
                    manager.operate('configure', selected_prefix=str(file_target))

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

    def test_operate_toggle_shortcut(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with patch.object(manager, 'shared_prefix', return_value=root), \
                 patch.object(manager, 'toggle_shortcut', return_value={'id': 'sc1', 'desktop': False, 'menu': True}) as mock_toggle:
                res = manager.operate('toggle_shortcut', shortcut_id='sc1', desktop=False, menu=True)
                mock_toggle.assert_called_once_with(root, 'sc1', desktop=False, menu=True)
                self.assertEqual(res, {'id': 'sc1', 'desktop': False, 'menu': True})


if __name__ == '__main__':
    unittest.main()


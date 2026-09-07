#!/usr/bin/env python3
"""Install WinBridge for the current user and associate Windows executables."""
import os
from pathlib import Path
import shutil
import subprocess

MIMES = ['application/x-ms-dos-executable', 'application/x-msdownload', 'application/vnd.microsoft.portable-executable']


def main():
    source = Path(__file__).resolve().parent
    build = source / 'build/manager'
    subprocess.run(['cmake', '-S', str(source / 'manager'), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--parallel', '2'], check=True)
    data = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
    target = data / 'winbridge/winbridge.py'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(Path(__file__).with_name('winbridge.py'), target)
    target.chmod(0o755)
    for module in ('shortcuts.py', 'shared_space.py', 'manager.py', 'manager_backend.py'):
        shutil.copyfile(Path(__file__).with_name(module), target.with_name(module))
    manager_binary = target.with_name('winbridge-manager')
    replacement = manager_binary.with_suffix('.new')
    shutil.copyfile(build / 'winbridge-manager', replacement)
    replacement.chmod(0o755)
    replacement.replace(manager_binary)
    icon = target.with_name('winbridge.png')
    shutil.copyfile(source / 'assets/winbridge.png', icon)
    apps = data / 'applications'
    apps.mkdir(parents=True, exist_ok=True)
    # Desktop Exec quoting has two escaping layers, independent of shell quoting.
    escaped = str(target).replace('\\', '\\\\\\\\').replace('"', '\\\\"').replace('`', '\\\\`').replace('$', '\\\\$').replace('%', '%%')
    desktop = apps / 'winbridge.desktop'
    desktop.write_text('[Desktop Entry]\nType=Application\nName=WinBridge\n'
                       'Comment=Open Windows executables with an installed Proton version\n'
                       f'Exec=/usr/bin/python3 "{escaped}" %f\nIcon={icon}\n'
                       'Terminal=false\nCategories=Utility;\nActions=Configure;\nMimeType=' + ';'.join(MIMES) + ';\n'
                       '[Desktop Action Configure]\nName=Velg Proton-versjon\n'
                       f'Exec=/usr/bin/python3 "{escaped}" --configure\n')
    from shortcuts import desktop_quote
    (apps / 'winbridge-manager.desktop').write_text(
        '[Desktop Entry]\nType=Application\nName=WinBridge Manager\n'
        'Comment=View and uninstall Windows programs\n'
        'Exec=' + desktop_quote(target.with_name('winbridge-manager')) + '\n'
        f'Icon={icon}\nTerminal=false\nCategories=Utility;\n')
    # Preserve old launcher paths so existing shortcuts keep working during migration.
    legacy = data / 'protonrun/protonrun.py'
    if legacy.is_file():
        legacy.write_text('#!/usr/bin/env python3\nimport os, sys\nos.execv(sys.executable, [sys.executable, ' + repr(str(target)) + ', *sys.argv[1:]])\n')
    (apps / 'protonrun.desktop').unlink(missing_ok=True)
    from shared_space import read_settings, save_settings, shared_prefix
    from shortcuts import import_shortcuts, desktop_dir
    settings = read_settings()
    if settings:
        save_settings(settings)
    if settings.get('proton'):
        import_shortcuts(shared_prefix(settings), Path(settings['proton']), target, desktop=desktop_dir())
    # Remove only old counterparts of successfully generated renamed launchers.
    for folder in (apps, desktop_dir()):
        if folder:
            for old in folder.glob('protonrun-*.desktop'):
                if old.with_name(old.name.replace('protonrun-', 'winbridge-', 1)).is_file():
                    old.unlink()
    if shutil.which('update-desktop-database'):
        subprocess.run(['update-desktop-database', str(apps)], check=True)
    for mime in MIMES:
        subprocess.run(['xdg-mime', 'default', 'winbridge.desktop', mime], check=True)
    print(f'Installert: {target}\n.exe-filer åpnes nå med WinBridge.')


if __name__ == '__main__':
    main()

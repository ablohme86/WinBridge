"""Import Proton's generated Windows shortcuts as safe Linux launchers."""
import configparser
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
from shared_space import shared_prefix


def desktop_quote(value):
    value = str(value).replace('%', '%%')
    for char in ('\\', '"', '`', '$'):
        value = value.replace(char, '\\' + char)
    return '"' + value.replace('\\', '\\\\') + '"'


def field(value):
    return str(value).replace('\\', '\\\\').replace('\n', '\\n').replace('\r', '\\r')


def desktop_dir():
    try:
        result = subprocess.run(['xdg-user-dir', 'DESKTOP'], capture_output=True, text=True, check=True)
        path = Path(result.stdout.strip())
        if path.is_absolute() and path != Path.home():
            return path
    except (OSError, subprocess.CalledProcessError):
        pass
    return None


def enclosing_prefix(exe):
    for parent in exe.parents:
        if parent.name == 'drive_c' and parent.parent.name == 'pfx':
            return parent.parent.parent
    return None


def shortcut_target(value, prefix):
    # Decode Desktop Entry string escaping, then its Exec argument quoting.
    value = re.sub(r'\\([\\sntr])', lambda m: {'\\': '\\', 's': ' ', 'n': '\n', 't': '\t', 'r': '\r'}[m[1]], value)
    tokens = shlex.split(value)
    if len(tokens) != 1 or not re.match(r'^[cC]:[\\/]', tokens[0]):
        return None
    drive = (prefix / 'pfx/drive_c').resolve()
    target = drive.joinpath(*tokens[0][3:].replace('\\', '/').split('/')).resolve()
    if target.is_relative_to(drive) and target.suffix.lower() in ('.lnk', '.exe') and target.is_file():
        return target
    return None


def import_shortcuts(prefix, proton, launcher, data=None, desktop=None):
    prefix = Path(prefix).resolve()
    data = Path(data) if data else Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
    source = prefix / 'pfx/drive_c/proton_shortcuts'
    imported = []
    for shortcut in sorted(source.glob('*.desktop')):
        try:
            config = configparser.ConfigParser(interpolation=None)
            config.read(shortcut, encoding='utf-8')
            entry = config['Desktop Entry']
            target = shortcut_target(entry.get('Exec', ''), prefix)
            if target is None:
                continue
            name = entry.get('Name', shortcut.stem)
            identity = hashlib.sha256(os.fsencode(prefix) + b'\0' + os.fsencode(shortcut.name)).hexdigest()[:20]
            filename = f'winbridge-{identity}.desktop'
            command = ['/usr/bin/python3', str(launcher)]
            if prefix != shared_prefix().resolve():
                command += ['--prefix', str(prefix), '--proton', str(proton)]
            command += ['--', str(target)]
            icon = 'application-x-executable'
            icon_name = entry.get('Icon', '')
            if icon_name and Path(icon_name).name == icon_name:
                icons = sorted((source / 'icons').glob('*/apps/' + icon_name.replace('[', '[[]').replace('*', '[*]').replace('?', '[?]') + '.png'))
                if icons:
                    icon = str(icons[-1])
            content = ('[Desktop Entry]\nType=Application\nName=' + field(name) + '\nExec=' +
                       ' '.join(map(desktop_quote, command)) + '\nIcon=' + field(icon) +
                       '\nTerminal=false\nCategories=Game;\nComment=Start med WinBridge\n')
            destinations = [data / 'applications' / filename]
            if desktop:
                destinations.append(Path(desktop) / filename)
            for destination in destinations:
                destination.parent.mkdir(parents=True, exist_ok=True)
                if not destination.exists() or destination.read_text() != content:
                    with tempfile.NamedTemporaryFile(mode='w', dir=destination.parent, delete=False) as output:
                        output.write(content)
                        temporary = Path(output.name)
                    temporary.chmod(0o755)
                    temporary.replace(destination)
            imported.append(name)
        except (OSError, ValueError, configparser.Error, KeyError) as exc:
            print(f'Kunne ikke importere {shortcut}: {exc}')
    return imported

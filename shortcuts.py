import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
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


def shortcuts_config_path():
    return Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'winbridge/shortcuts.json'


def read_shortcuts_config():
    path = shortcuts_config_path()
    try:
        data = json.loads(path.read_text())
        if isinstance(data, dict):
            return data
    except (OSError, ValueError):
        pass
    return {}


def save_shortcuts_config(config):
    path = shortcuts_config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as output:
        json.dump(config, output, indent=2)
        temporary = Path(output.name)
    temporary.replace(path)


def shortcut_icon_path(source, icon_name):
    if not icon_name or Path(icon_name).name != icon_name:
        return ''
    icons = sorted((source / 'icons').glob('*/apps/' + icon_name.replace('[', '[[]').replace('*', '[*]').replace('?', '[?]') + '.png'))
    return str(icons[-1].resolve()) if icons else ''


def load_shortcuts(prefix, data=None, desktop=None):
    prefix = Path(prefix).resolve()
    data = Path(data) if data else Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
    desktop = Path(desktop) if desktop else desktop_dir()
    source = prefix / 'pfx/drive_c/proton_shortcuts'
    config = read_shortcuts_config()
    shortcuts = []
    for sc in sorted(source.glob('*.desktop')):
        try:
            cp = configparser.ConfigParser(interpolation=None)
            cp.read(sc, encoding='utf-8')
            entry = cp['Desktop Entry']
            target = shortcut_target(entry.get('Exec', ''), prefix)
            if target is None:
                continue
            name = entry.get('Name', sc.stem)
            identity = hashlib.sha256(os.fsencode(prefix) + b'\0' + os.fsencode(sc.name)).hexdigest()[:20]
            filename = f'winbridge-{identity}.desktop'
            icon_name = entry.get('Icon', '')
            icon = shortcut_icon_path(source, icon_name)
            pref = config.get(identity, {})
            app_file = data / 'applications' / filename
            desk_file = desktop / filename if desktop else None
            menu_enabled = pref.get('menu', app_file.is_file() if config else True)
            desktop_enabled = pref.get('desktop', desk_file.is_file() if (desktop and config) else True)
            shortcuts.append({
                'id': identity,
                'name': name,
                'file': sc.name,
                'icon': icon,
                'path': entry.get('Path', ''),
                'exec': entry.get('Exec', ''),
                'desktop': bool(desktop_enabled),
                'menu': bool(menu_enabled)
            })
        except (OSError, ValueError, configparser.Error, KeyError):
            continue
    return shortcuts


def get_program_registry_meta(prefix):
    prefix = Path(prefix).resolve()
    meta = {}
    for reg_path in (prefix / 'pfx/system.reg', prefix / 'pfx/user.reg'):
        if not reg_path.is_file():
            continue
        try:
            content = reg_path.read_text(errors='replace')
        except OSError:
            continue
        pattern = re.compile(r'\[Software\\\\(?:Wow6432Node\\\\)?Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\([^\]]+)\](.*?)(?=\n\[|\Z)', re.DOTALL)
        for m in pattern.finditer(content):
            key = m.group(1).strip()
            body = m.group(2)
            name_m = re.search(r'\"DisplayName\"=\"([^\"]+)\"', body)
            loc_m = re.search(r'\"(?:InstallLocation|Inno Setup: App Path)\"=\"([^\"]+)\"', body)
            grp_m = re.search(r'\"Inno Setup: Icon Group\"=\"([^\"]+)\"', body)
            icon_m = re.search(r'\"DisplayIcon\"=\"([^\"]+)\"', body)
            disp = name_m.group(1) if name_m else ''
            disp = re.sub(r'\\x([0-9a-fA-F]{4})', lambda x: chr(int(x.group(1), 16)), disp)
            l = loc_m.group(1).replace('\\\\', '/').strip('/').replace('C:', '').strip('/') if loc_m else ''
            g = grp_m.group(1).replace('\\\\', '/') if grp_m else ''
            ic = Path(icon_m.group(1).replace('\\\\', '/')).stem if icon_m else ''
            meta[key.casefold()] = {
                'key': key,
                'name': disp,
                'loc': l,
                'group': g,
                'icon': ic
            }
    return meta


def group_shortcuts_by_program(prefix, programs):
    meta = get_program_registry_meta(prefix)
    all_shortcuts = load_shortcuts(prefix)
    grouped = {p['key']: [] for p in programs}

    for sc in all_shortcuts:
        sc_name = sc['name'].casefold()
        sc_path = sc['path'].replace('\\', '/').casefold()
        sc_exec = sc['exec'].replace('\\', '/').casefold()
        sc_file = sc['file'].casefold()

        best_match = None
        best_score = 0
        for p in programs:
            k = p['key']
            m = meta.get(k.casefold(), {})
            p_name = (p.get('name') or m.get('name', '')).casefold()
            p_loc = m.get('loc', '').casefold()
            p_grp = m.get('group', '').casefold()
            p_icon = m.get('icon', '').casefold()

            score = 0
            if p_loc and p_loc in sc_path:
                score += 50 + len(p_loc)
            if p_grp and p_grp in sc_exec:
                score += 40 + len(p_grp)
            if p_name and p_name in sc_exec:
                score += 30 + len(p_name)
            if p_name and p_name in sc_path:
                score += 25 + len(p_name)
            if p_name and (p_name == sc_name or p_name in sc_name or sc_name in p_name):
                score += 20 + len(p_name)
            if p_icon and p_icon in sc_file:
                score += 15

            if score > best_score:
                best_score = score
                best_match = k

        if best_match and best_score > 0:
            grouped[best_match].append({
                'id': sc['id'],
                'name': sc['name'],
                'icon': sc['icon'],
                'desktop': sc['desktop'],
                'menu': sc['menu']
            })

    result = []
    for p in programs:
        item = dict(p)
        item['shortcuts'] = sorted(grouped.get(p['key'], []), key=lambda s: s['name'].casefold())
        result.append(item)
    return result


def toggle_shortcut(prefix, shortcut_id, desktop=None, menu=None, launcher=None, proton=None):
    prefix = Path(prefix).resolve()
    config = read_shortcuts_config()
    pref = config.setdefault(shortcut_id, {})
    if desktop is not None:
        pref['desktop'] = bool(desktop)
    if menu is not None:
        pref['menu'] = bool(menu)
    save_shortcuts_config(config)

    if launcher is None:
        candidates = [
            Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'winbridge/winbridge.py',
            Path(__file__).resolve().parent / 'winbridge.py'
        ]
        launcher = next((c for c in candidates if c.is_file()), candidates[0])
    if proton is None:
        from shared_space import read_settings
        settings = read_settings()
        proton = Path(settings['proton']) if settings.get('proton') else None

    if proton:
        import_shortcuts(prefix, proton, launcher, desktop=desktop_dir())

    return {
        'id': shortcut_id,
        'desktop': pref.get('desktop', True),
        'menu': pref.get('menu', True)
    }


def import_shortcuts(prefix, proton, launcher, data=None, desktop=None):
    prefix = Path(prefix).resolve()
    data = Path(data) if data else Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
    source = prefix / 'pfx/drive_c/proton_shortcuts'
    config = read_shortcuts_config()
    imported = []
    for shortcut in sorted(source.glob('*.desktop')):
        try:
            cp = configparser.ConfigParser(interpolation=None)
            cp.read(shortcut, encoding='utf-8')
            entry = cp['Desktop Entry']
            target = shortcut_target(entry.get('Exec', ''), prefix)
            if target is None:
                continue
            name = entry.get('Name', shortcut.stem)
            identity = hashlib.sha256(os.fsencode(prefix) + b'\0' + os.fsencode(shortcut.name)).hexdigest()[:20]
            filename = f'winbridge-{identity}.desktop'

            pref = config.get(identity, {})
            show_menu = pref.get('menu', True)
            show_desktop = pref.get('desktop', True)

            app_dest = data / 'applications' / filename
            desk_dest = Path(desktop) / filename if desktop else None

            if not show_menu and app_dest.exists():
                app_dest.unlink(missing_ok=True)
            if not show_desktop and desk_dest and desk_dest.exists():
                desk_dest.unlink(missing_ok=True)

            if not show_menu and not show_desktop:
                continue

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
            destinations = []
            if show_menu:
                destinations.append(app_dest)
            if show_desktop and desk_dest:
                destinations.append(desk_dest)
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
    if shutil.which('update-desktop-database'):
        try:
            subprocess.run(['update-desktop-database', str(data / 'applications')], check=False, capture_output=True)
        except OSError:
            pass
    return imported

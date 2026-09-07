#!/usr/bin/env python3
"""JSON interface to WinBridge's Proton integration, used by the native Qt UI."""
import argparse
import configparser
import contextlib
import json
from pathlib import Path
import sys
import os
import signal
import time
from shared_space import read_settings, shared_prefix, save_settings, settings_lock, default_prefix
from shortcuts import get_program_registry_meta, group_shortcuts_by_program, toggle_shortcut
from winbridge import launch, steam_roots, libraries, discover, runtime_for

SYSTEM_PROCESSES = {
    'wineserver', 'services.exe', 'winedevice.exe', 'plugplay.exe',
    'svchost.exe', 'rpcss.exe', 'conhost.exe', 'tabtip.exe', 'xalia.exe', 'explorer.exe'
}


def parse_programs(output):
    result = set()
    for line in output.splitlines():
        if '|||' in line:
            key, name = line.split('|||', 1)
            if key.strip() and name.strip():
                result.add((key.strip(), name.strip()))
    return sorted(result, key=lambda p: p[1].casefold())


def get_running_apps(prefix, programs=None):
    prefix = Path(prefix).resolve()
    pfx = (prefix / 'pfx').resolve()
    proc_dir = Path('/proc')
    if not proc_dir.is_dir():
        return {}
    meta = get_program_registry_meta(prefix)
    if programs is None:
        programs = [{'key': m['key'], 'name': m['name']} for m in meta.values()]

    children = {}
    proc_info = {}
    for p in proc_dir.iterdir():
        if not p.name.isdigit():
            continue
        pid = int(p.name)
        try:
            status_text = (p / 'status').read_text()
            ppid = 0
            for line in status_text.splitlines():
                if line.startswith('PPid:'):
                    ppid = int(line.split()[1])
                    break
            children.setdefault(ppid, []).append(pid)

            cmdline = [c.decode('utf-8', 'replace') for c in (p / 'cmdline').read_bytes().split(b'\0') if c]
            env_bytes = (p / 'environ').read_bytes()
            env = dict(item.decode('utf-8', 'replace').split('=', 1) for item in env_bytes.split(b'\0') if b'=' in item)
            data_path = env.get('STEAM_COMPAT_DATA_PATH')
            wine_prefix = env.get('WINEPREFIX')
            in_prefix = False
            if data_path and Path(data_path).resolve() == prefix:
                in_prefix = True
            elif wine_prefix and Path(wine_prefix).resolve() == pfx:
                in_prefix = True
            if not in_prefix:
                continue
            proc_info[pid] = {'pid': pid, 'ppid': ppid, 'cmdline': cmdline, 'env': env}
        except (PermissionError, ProcessLookupError, OSError):
            continue

    def get_descendants(pid):
        result = set()
        queue = [pid]
        while queue:
            curr = queue.pop(0)
            for child in children.get(curr, []):
                if child not in result:
                    result.add(child)
                    queue.append(child)
        return result

    running_by_key = {}
    for prog in programs:
        key = prog['key']
        name = prog.get('name', '')
        m = meta.get(key.casefold(), {})
        loc = m.get('loc', '').casefold()
        group = m.get('group', '').casefold()
        icon = m.get('icon', '').casefold()
        sc_names = [sc['name'].casefold() for sc in prog.get('shortcuts', [])]
        matched_pids = set()

        for pid, info in proc_info.items():
            cmdline = info['cmdline']
            if not cmdline:
                continue
            base = Path(cmdline[0].replace('\\\\', '/')).name.lower()
            if base in SYSTEM_PROCESSES:
                continue
            cmd_str = ' '.join(cmdline).lower().replace('\\\\', '/')
            matched = False
            if loc and loc in cmd_str:
                matched = True
            elif group and group in cmd_str:
                matched = True
            elif icon and len(icon) > 3 and icon in cmd_str:
                matched = True
            elif name and len(name) > 3 and name.casefold() in cmd_str:
                matched = True
            elif key and len(key) > 3 and key.casefold() in cmd_str:
                matched = True
            elif any(s and len(s) > 3 and s in cmd_str for s in sc_names):
                matched = True

            if matched:
                matched_pids.add(pid)
                for desc in get_descendants(pid):
                    if desc in proc_info:
                        desc_base = Path(proc_info[desc]['cmdline'][0].replace('\\\\', '/')).name.lower() if proc_info[desc]['cmdline'] else ''
                        if desc_base not in SYSTEM_PROCESSES:
                            matched_pids.add(desc)

        running_by_key[key] = sorted(matched_pids)

    return running_by_key


def kill_program(prefix, key, programs=None):
    prefix = Path(prefix).resolve()
    running_map = get_running_apps(prefix, programs)
    pids = running_map.get(key, [])
    if not pids:
        for k, v in running_map.items():
            if k.casefold() == (key or '').casefold():
                pids = v
                break
    if not pids:
        return {'killed': False, 'key': key, 'pids': [], 'error': 'Program is not running.'}
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    time.sleep(0.1)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    return {'killed': True, 'key': key, 'pids': pids}


def program_icons(prefix):
    """Reuse the real icon exported for a program's Windows shortcut."""
    source = prefix / 'pfx/drive_c/proton_shortcuts'
    result = {}
    for desktop in sorted(source.glob('*.desktop')):
        try:
            config = configparser.ConfigParser(interpolation=None)
            config.read(desktop, encoding='utf-8')
            entry = config['Desktop Entry']
            name = entry.get('Name', '').strip().casefold()
            icon = entry.get('Icon', '')
            if not name or not icon or Path(icon).name != icon:
                continue
            candidates = []
            for size in (source / 'icons').iterdir():
                path = size / 'apps' / (icon + '.png')
                if path.is_file() and path.resolve().is_relative_to(source.resolve()):
                    try:
                        resolution = int(size.name.split('x')[0])
                    except ValueError:
                        continue
                    candidates.append((resolution, str(path.resolve())))
            if candidates:
                result[name] = max(candidates)[1]
        except (OSError, ValueError, configparser.Error, KeyError):
            continue
    return result


def operate(action, key=None, selected_proton=None, selected_prefix=None, shortcut_id=None, desktop=None, menu=None):
    settings = read_settings()
    if action in ('settings', 'configure'):
        roots = steam_roots()
        libs = libraries(roots)
        versions = discover(roots, libs)
        if action == 'configure':
            with settings_lock():
                settings = read_settings()
                if selected_proton:
                    selected = Path(selected_proton).expanduser().resolve()
                    if selected not in versions:
                        raise RuntimeError('The selected Proton version is not installed.')
                    runtime_for(selected, libs)
                    settings['proton'] = str(selected)
                if selected_prefix is not None:
                    if selected_prefix.strip():
                        new_prefix = Path(selected_prefix).expanduser().resolve()
                        if new_prefix.is_file():
                            raise RuntimeError('The environment path must be a directory, not a file.')
                        if new_prefix.name == 'pfx' and (new_prefix / 'drive_c').is_dir():
                            new_prefix = new_prefix.parent
                        new_prefix.mkdir(parents=True, exist_ok=True)
                        settings['prefix'] = str(new_prefix)
                    else:
                        settings['prefix'] = str(default_prefix())
                elif 'prefix' not in settings:
                    settings['prefix'] = str(shared_prefix(settings))
                save_settings(settings)
        return {
            'selected': settings.get('proton', ''),
            'versions': [{'name': p.name, 'path': str(p)} for p in versions],
            'prefix': str(shared_prefix(settings)),
            'default_prefix': str(default_prefix()),
        }
    prefix = shared_prefix(settings)
    if action == 'toggle_shortcut':
        if not shortcut_id:
            raise RuntimeError('Shortcut ID is required.')
        return toggle_shortcut(prefix, shortcut_id, desktop=desktop, menu=menu)
    if action == 'running':
        running_map = get_running_apps(prefix)
        return {
            'running': {k: bool(v) for k, v in running_map.items()}
        }
    if action == 'kill':
        if not key:
            raise RuntimeError('Program key is required.')
        return kill_program(prefix, key)
    proton = Path(settings['proton']) if settings.get('proton') else None
    exe = prefix / 'pfx/drive_c/windows/system32/uninstaller.exe'
    result = {'prefix': str(prefix), 'proton': proton.name if proton else 'Not selected',
              'programs': [], 'ready': False}
    if not exe.is_file():
        if action != 'list':
            raise RuntimeError('Windows environment does not exist yet.')
        return result
    if not proton or not (proton / 'proton').is_file():
        raise RuntimeError('Select an installed Proton version in Settings first.')
    roots = steam_roots()
    libs = libraries(roots)
    def query():
        return parse_programs(launch(exe, proton, roots, libs, ['--list'], prefix, capture=True, verb='runinprefix'))
    programs = query()
    if action == 'uninstall':
        matches = [p for p in programs if p[0].casefold() == (key or '').casefold()]
        if len(matches) != 1:
            raise RuntimeError('The app no longer exists, or its ID is ambiguous. Refresh the library.')
        launch(exe, proton, roots, libs, ['--remove', matches[0][0]], prefix, verb='runinprefix')
        programs = query()
        result['removed'] = not any(p[0].casefold() == key.casefold() for p in programs)
    icons = program_icons(prefix)
    base_programs = [{'key': k, 'name': n, 'icon': icons.get(n.casefold(), '')} for k, n in programs]
    grouped_programs = group_shortcuts_by_program(prefix, base_programs)
    running_map = get_running_apps(prefix, grouped_programs)
    for p in grouped_programs:
        pids = running_map.get(p['key'], [])
        p['pids'] = pids
        p['running'] = len(pids) > 0
    result.update(ready=True, programs=grouped_programs)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['list', 'uninstall', 'settings', 'configure', 'toggle_shortcut', 'running', 'kill'])
    parser.add_argument('--key')
    parser.add_argument('--proton')
    parser.add_argument('--prefix')
    parser.add_argument('--id')
    parser.add_argument('--desktop', type=int)
    parser.add_argument('--menu', type=int)
    args = parser.parse_args()
    try:
        desktop = bool(args.desktop) if args.desktop is not None else None
        menu = bool(args.menu) if args.menu is not None else None
        with contextlib.redirect_stdout(sys.stderr):
            result = operate(args.action, args.key, args.proton, args.prefix, args.id, desktop, menu)
        print(json.dumps(result, ensure_ascii=False))
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(json.dumps({'error': str(exc)}, ensure_ascii=False))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())


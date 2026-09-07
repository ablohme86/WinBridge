#!/usr/bin/env python3
"""JSON interface to WinBridge's Proton integration, used by the native Qt UI."""
import argparse
import contextlib
import json
from pathlib import Path
import sys
from shared_space import read_settings, shared_prefix, save_settings, settings_lock
from winbridge import launch, steam_roots, libraries, discover, runtime_for


def parse_programs(output):
    result = set()
    for line in output.splitlines():
        if '|||' in line:
            key, name = line.split('|||', 1)
            if key.strip() and name.strip():
                result.add((key.strip(), name.strip()))
    return sorted(result, key=lambda p: p[1].casefold())


def operate(action, key=None, selected_proton=None):
    settings = read_settings()
    if action in ('settings', 'configure'):
        roots = steam_roots()
        libs = libraries(roots)
        versions = discover(roots, libs)
        if action == 'configure' and selected_proton:
            selected = Path(selected_proton).expanduser().resolve()
            if selected not in versions:
                raise RuntimeError('The selected Proton version is not installed.')
            runtime_for(selected, libs)
            with settings_lock():
                settings = read_settings()
                settings.update(prefix=str(shared_prefix(settings)), proton=str(selected))
                save_settings(settings)
        return {'selected': settings.get('proton', ''), 'versions': [{'name': p.name, 'path': str(p)} for p in versions]}
    prefix = shared_prefix(settings)
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
    result.update(ready=True, programs=[{'key': k, 'name': n} for k, n in programs])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['list', 'uninstall', 'settings', 'configure'])
    parser.add_argument('--key')
    parser.add_argument('--proton')
    args = parser.parse_args()
    try:
        with contextlib.redirect_stdout(sys.stderr):
            result = operate(args.action, args.key, args.proton)
        print(json.dumps(result, ensure_ascii=False))
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(json.dumps({'error': str(exc)}, ensure_ascii=False))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

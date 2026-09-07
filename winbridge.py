#!/usr/bin/env python3
"""Open Windows executables using an installed Proton, with a desktop chooser."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from shortcuts import desktop_dir, import_shortcuts
from shared_space import shared_prefix, select_proton


def pairs(path):
    try:
        return dict(re.findall(r'"([^"\n]+)"\s*"((?:\\.|[^"\\])*)"', path.read_text()))
    except (OSError, UnicodeError):
        return {}


def steam_roots():
    home = Path.home()
    candidates = [home / '.local/share/Steam', home / '.steam/steam', home / '.steam/root',
                  home / '.var/app/com.valvesoftware.Steam/.local/share/Steam',
                  home / '.var/app/com.valvesoftware.Steam/data/Steam']
    return list(dict.fromkeys(p.resolve() for p in candidates if p.is_dir()))


def libraries(roots):
    result = list(roots)
    for root in roots:
        try:
            content = (root / 'steamapps/libraryfolders.vdf').read_text()
        except OSError:
            continue
        for value in re.findall(r'"path"\s*"((?:\\.|[^"\\])*)"', content):
            result.append(Path(value.replace('\\\\', '\\').replace('\\"', '"')))
    return list(dict.fromkeys(p.resolve() for p in result if p.is_dir()))


def discover(roots, libs):
    folders = [p / 'steamapps/common' for p in libs]
    folders += [p / 'compatibilitytools.d' for p in roots]
    folders += [Path('/usr/share/steam/compatibilitytools.d')]
    folders += [Path(p).expanduser() for p in os.environ.get('WINBRIDGE_SEARCH_PATHS', os.environ.get('PROTONRUN_SEARCH_PATHS', '')).split(os.pathsep) if p]
    found = set()
    for folder in folders:
        if (folder / 'proton').is_file():
            found.add(folder.resolve())
        try:
            found.update(p.resolve() for p in folder.iterdir() if (p / 'proton').is_file())
        except OSError:
            pass
    return sorted(found, key=lambda p: p.name.casefold())


def dialog_tool():
    kde = 'kde' in os.environ.get('XDG_CURRENT_DESKTOP', '').lower()
    for tool in (['kdialog', 'zenity'] if kde else ['zenity', 'kdialog']):
        if shutil.which(tool):
            return tool
    raise RuntimeError('Installer kdialog (KDE) eller zenity (GNOME) for å vise dialogen.')


def error(message):
    print(message, file=sys.stderr)
    try:
        tool = dialog_tool()
        args = ['kdialog', '--title', 'WinBridge', '--error', message] if tool == 'kdialog' else [
            'zenity', '--error', '--no-markup', '--title=WinBridge', '--text=' + message]
        subprocess.run(args, check=False)
    except (RuntimeError, OSError):
        pass


def choose(versions, exe):
    tool = dialog_tool()
    prompt = 'Hvilken Proton-versjon vil du bruke i WinBridge?\nValget huskes for alle programmer.'
    if tool == 'kdialog':
        args = [tool, '--title', 'WinBridge', '--ok-label', 'Open', '--cancel-label', 'Avbryt', '--menu', prompt]
        for index, version in enumerate(versions):
            args += [str(index), f'{version.name} — {version}']
    else:
        args = [tool, '--list', '--title=WinBridge', '--text=' + prompt, '--no-markup',
                '--ok-label=Open', '--cancel-label=Avbryt', '--width=820', '--height=380',
                '--column=ID', '--column=Proton-versjon', '--column=Plassering',
                '--hide-column=1', '--print-column=1']
        for index, version in enumerate(versions):
            args += [str(index), version.name, str(version)]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode == 1:
        return None
    if result.returncode != 0:
        raise RuntimeError('Kunne ikke åpne versjonsvelgeren: ' + result.stderr.strip())
    if not result.stdout.strip():
        return None
    return versions[int(result.stdout.strip())]


def runtime_for(proton, libs):
    appid = pairs(proton / 'toolmanifest.vdf').get('require_tool_appid')
    if not appid or appid == '0':
        return None
    for lib in libs:
        install = pairs(lib / f'steamapps/appmanifest_{appid}.acf').get('installdir')
        if install:
            runtime = lib / 'steamapps/common' / install / '_v2-entry-point'
            if runtime.is_file():
                return runtime
    raise RuntimeError(f'{proton.name} trenger Steam Linux Runtime (Steam App ID {appid}). '
                       'Installer den fra Verktøy i Steam og prøv igjen.')


def launch(exe, proton, roots, libs, extra=(), prefix=None, capture=False, verb="run"):
    runtime = runtime_for(proton, libs)
    data = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'winbridge'
    # All default launches share one persistent Windows environment.
    key = hashlib.sha256(os.fsencode(exe.parent)).hexdigest()[:20]
    compat = Path(prefix).expanduser().resolve() if prefix else shared_prefix()
    compat.mkdir(parents=True, exist_ok=True)
    logs = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'winbridge/logs'
    logs.mkdir(parents=True, exist_ok=True)
    logfile = logs / f'{key}-{time.time_ns()}.log'
    env = os.environ.copy()
    env.update(STEAM_COMPAT_DATA_PATH=str(compat),
               STEAM_COMPAT_CLIENT_INSTALL_PATH=str(roots[0]) if roots else str(data / 'steam'),
               STEAM_COMPAT_INSTALL_PATH=str(exe.parent),
               STEAM_COMPAT_LIBRARY_PATHS=os.pathsep.join(map(str, libs)),
               STEAM_COMPAT_APP_ID='0', SteamAppId='0', SteamGameId='0',
               STEAM_COMPAT_TOOL_PATHS=os.pathsep.join([str(proton)] + ([str(runtime.parent)] if runtime else [])))
    target_args = ['start.exe', '/unix', str(exe)] if exe.suffix.lower() == '.lnk' else [str(exe)]
    command = [str(proton / 'proton'), verb, *target_args, *extra]
    if runtime:
        command = [str(runtime), '--verb=run', '--', *command]
    with logfile.open('w') as log:
        log.write(f'Proton: {proton}\nExecutable: {exe}\nPrefix: {compat}\n')
        log.flush()
        desktop = desktop_dir()
        with subprocess.Popen(command, cwd=exe.parent, env=env, stdout=log, stderr=subprocess.STDOUT) as process:
            while True:
                import_shortcuts(compat, proton, Path(__file__).resolve(), desktop=desktop)
                try:
                    process.wait(timeout=3)
                    break
                except subprocess.TimeoutExpired:
                    pass
            import_shortcuts(compat, proton, Path(__file__).resolve(), desktop=desktop)
        result = process
    if result.returncode:
        raise RuntimeError(f'Programmet avsluttet med feilkode {result.returncode}.\nLogg: {logfile}')
    return logfile.read_text(errors='replace') if capture else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--import-shortcuts', action='store_true', help='Import existing shortcuts; requires --prefix and --proton')
    parser.add_argument('--configure', action='store_true', help='Change the shared Proton version')
    parser.add_argument('--list', action='store_true', help='List installed Proton versions')
    parser.add_argument('--proton', type=Path, help='Use this Proton directory without a chooser')
    parser.add_argument('--prefix', help='Use a specific compatdata directory')
    parser.add_argument('exe', nargs='?', type=Path)
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    roots = steam_roots()
    libs = libraries(roots)
    versions = discover(roots, libs)
    if args.list:
        for version in versions:
            print(f'{version.name}\t{version}')
        return 0
    try:
        if args.configure:
            select_proton(versions, lambda items: choose(items, Path('WinBridge')), lambda p: runtime_for(p, libs), change=True)
            return 0
        if args.import_shortcuts:
            if not args.prefix or not args.proton:
                raise RuntimeError('--import-shortcuts krever --prefix og --proton.')
            print('Importert: ' + ', '.join(import_shortcuts(Path(args.prefix).expanduser(), args.proton.expanduser().resolve(), Path(__file__).resolve(), desktop=desktop_dir())))
            return 0
        if args.exe is None:
            tool = dialog_tool()
            command = ['kdialog', '--getopenfilename', str(Path.home()), '*.exe *.EXE|Windows-programmer'] if tool == 'kdialog' else [
                'zenity', '--file-selection', '--title=Velg en .exe-fil', '--file-filter=Windows | *.exe *.EXE']
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                return 0
            args.exe = Path(result.stdout.strip())
        exe = args.exe.expanduser().resolve()
        if not exe.is_file() or exe.suffix.lower() not in ('.exe', '.lnk'):
            raise RuntimeError('Velg en eksisterende .exe-fil.')
        if args.proton:
            proton = args.proton.expanduser().resolve()
            if not (proton / 'proton').is_file():
                raise RuntimeError('Denne mappen inneholder ikke Proton.')
        else:
            proton = select_proton(versions, lambda items: choose(items, exe), lambda p: runtime_for(p, libs))
        return launch(exe, proton, roots, libs, args.arguments, args.prefix) if proton else 0
    except (OSError, RuntimeError, ValueError) as exc:
        error(str(exc))
        return 1


if __name__ == '__main__':
    sys.exit(main())

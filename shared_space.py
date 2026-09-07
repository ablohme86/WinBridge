"""Persistent per-user WinBridge Windows environment and Proton selection."""
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import tempfile


def data_dir():
    return Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'winbridge'


def settings_path():
    return Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'winbridge/settings.json'


def read_settings():
    try:
        settings = json.loads(settings_path().read_text())
    except FileNotFoundError:
        legacy = settings_path().parent.parent / 'protonrun/settings.json'
        try:
            settings = json.loads(legacy.read_text())
        except FileNotFoundError:
            return {}
    if not isinstance(settings, dict) or any(not isinstance(v, str) for v in settings.values()):
        raise ValueError('Ugyldige WinBridge-innstillinger.')
    return settings


def save_settings(settings):
    path = settings_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, delete=False) as out:
        json.dump(settings, out, indent=2)
        temporary = Path(out.name)
    temporary.replace(path)


@contextmanager
def settings_lock():
    path = settings_path().with_suffix('.lock')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def shared_prefix(settings=None):
    settings = read_settings() if settings is None else settings
    if settings.get('prefix'):
        return Path(settings['prefix']).expanduser().resolve()
    # Adopt the sole legacy environment in place, preserving installed shortcuts and paths.
    old_data = data_dir().parent / 'protonrun'
    if (old_data / 'shared/pfx').is_dir():
        return (old_data / 'shared').resolve()
    old_prefixes = sorted(p.parent for p in (old_data / 'prefixes').glob('*/pfx') if p.is_dir())
    if len(old_prefixes) == 1:
        return old_prefixes[0].resolve()
    legacy = sorted(p.parent for p in (data_dir() / 'prefixes').glob('*/pfx') if p.is_dir())
    return legacy[0].resolve() if len(legacy) == 1 else (data_dir() / 'shared').resolve()


def select_proton(versions, chooser, validate, change=False):
    with settings_lock():
        settings = read_settings()
        saved = Path(settings['proton']) if settings.get('proton') else None
        if saved and (saved / 'proton').is_file() and not change:
            return saved
        if not versions:
            raise RuntimeError('Fant ingen Proton-versjoner. Installer Proton fra Verktøy i Steam.')
        selected = chooser(versions)
        if selected is None:
            return None
        validate(selected)
        settings.update(prefix=str(shared_prefix(settings)), proton=str(selected))
        save_settings(settings)
        return selected

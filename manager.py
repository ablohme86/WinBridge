#!/usr/bin/env python3
"""Compatibility entry point for the native WinBridge Manager."""
import os
from pathlib import Path
import sys

if __name__ == '__main__':
    base = Path(__file__).resolve().parent
    candidates = [base / 'winbridge-manager', base / 'build/manager/winbridge-manager', Path('/usr/bin/winbridge-manager')]
    binary = next((p for p in candidates if p.is_file() and os.access(p, os.X_OK)), None)
    if binary is None:
        raise SystemExit('Bygg Manager først med: cmake -S manager -B build/manager && cmake --build build/manager')
    os.execv(str(binary), [str(binary), *sys.argv[1:]])

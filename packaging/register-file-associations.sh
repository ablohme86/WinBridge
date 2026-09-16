#!/bin/sh
# Register the installed launcher with both XDG and GIO file managers.
set -eu

if [ "$(id -u)" = 0 ]; then
    if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
        # sudo make install must configure the invoking user, not root.
        exec sudo -H -u "$SUDO_USER" -- env -u XDG_CONFIG_HOME -u XDG_DATA_HOME \
            sh "$0" "$@"
    fi
    echo "No desktop user selected; run make install-user as your desktop user to set .exe defaults."
    exit 0
fi

user_mime=${XDG_DATA_HOME:-"$HOME/.local/share"}/mime
if [ "${1:-}" = --remove-mime ]; then
    rm -f "$user_mime/packages/winbridge-exe.xml"
    if [ -d "$user_mime" ] && command -v update-mime-database >/dev/null 2>&1; then
        update-mime-database "$user_mime"
    fi
    exit 0
fi

desktop_source=$1
WINBRIDGE_EXECUTABLE=$2
mime_source=${3:-"$(dirname "$0")/winbridge-exe.xml"}
export WINBRIDGE_EXECUTABLE
user_apps=${XDG_DATA_HOME:-"$HOME/.local/share"}/applications
mkdir -p "$user_apps" "${XDG_CONFIG_HOME:-"$HOME/.config"}"

# Install in the desktop user's XDG directory even for a custom/system PREFIX.
# The source can already be this file during make install-user.
if ! command -v update-mime-database >/dev/null 2>&1; then
    echo "Install shared-mime-info (update-mime-database) and retry." >&2
    exit 1
fi
mkdir -p "$user_mime/packages"
if ! cmp -s "$mime_source" "$user_mime/packages/winbridge-exe.xml"; then
    install -m 644 "$mime_source" "$user_mime/packages/winbridge-exe.xml"
fi
update-mime-database "$user_mime"

# A desktop session may not have ~/.local/bin on PATH. Use the installed
# executable's absolute path, including desktop-entry escaping for special chars.
# Write through a temporary file since the source can be the user entry itself.
desktop_tmp=$(mktemp "$user_apps/.winbridge.desktop.XXXXXX")
trap 'rm -f "$desktop_tmp"' EXIT HUP INT TERM
awk '
    BEGIN {
        path = ENVIRON["WINBRIDGE_EXECUTABLE"]
        quoted = "\""
        for (i = 1; i <= length(path); i++) {
            c = substr(path, i, 1)
            if (c == "\\") quoted = quoted "\\\\\\\\"
            else if (c == "\"" || c == "$" || c == "`") quoted = quoted "\\\\" c
            else if (c == "%") quoted = quoted "%%"
            else quoted = quoted c
        }
        quoted = quoted "\""
    }
    /^Exec=winbridge / {
        print "Exec=" quoted substr($0, length("Exec=winbridge") + 1)
        next
    }
    { print }
' "$desktop_source" > "$desktop_tmp"
chmod 644 "$desktop_tmp"
mv "$desktop_tmp" "$user_apps/winbridge.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$user_apps"
fi

mime_types=$(sed -n 's/^MimeType=//p' "$user_apps/winbridge.desktop" | tr ';' ' ')
for mime in $mime_types; do
    registered=false
    if command -v xdg-mime >/dev/null 2>&1; then
        if PATH="/usr/lib/qt6/bin:$PATH" xdg-mime default winbridge.desktop "$mime"; then
            registered=true
        fi
    fi
    # Nemo, Nautilus, Caja, Thunar and PCManFM use GLib/GIO associations.
    if command -v gio >/dev/null 2>&1; then
        if gio mime "$mime" winbridge.desktop; then
            registered=true
        fi
    fi
    if [ "$registered" = false ]; then
        echo "Could not set WinBridge as the default for $mime; install xdg-utils or GIO and retry." >&2
        exit 1
    fi
done

# Refresh Dolphin/KDE after changing the defaults.
if command -v kbuildsycoca6 >/dev/null 2>&1; then
    kbuildsycoca6 --noincremental || true
elif command -v kbuildsycoca5 >/dev/null 2>&1; then
    kbuildsycoca5 --noincremental || true
fi

# Installing and configuring WinBridge

For an overview of the app and its features, see [README.md](README.md).

## Install for your user

Requirements:

- CMake, a C++17 compiler, and Qt 6.2+ Core & Widgets development files
- Qt 6 Test development files for the unit test suite
- `xdg-utils` and `xdg-user-dirs`
- KDialog or Zenity for the `.exe` launcher's first-run dialog
- An installed Proton version and its matching Steam Linux Runtime

On Arch Linux, the build dependencies are available through:

```sh
sudo pacman -S --needed base-devel cmake qt6-base xdg-utils xdg-user-dirs zenity
```

On Debian, update the package index and install the build dependencies with:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake qt6-base-dev qt6-base-dev-tools xdg-utils xdg-user-dirs zenity
```

On Fedora (RPM-based), install the build dependencies with DNF:

```sh
sudo dnf install gcc-c++ make cmake qt6-qtbase-devel xdg-utils xdg-user-dirs zenity
```

Build and install from the project directory:

```sh
make
make install-user
```

This builds all native executables, installs WinBridge to `~/.local/bin`, adds **WinBridge** and **WinBridge Manager** to the application menu, and makes WinBridge your default `.exe` handler. Run `sudo make install` to install system-wide to `/usr/local`.

## Build with CMake

The executables can also be built directly with CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/manager/winbridge-manager
```

Generate an empty-library design preview without accessing a Windows environment:

```sh
QT_QPA_PLATFORM=offscreen ./build/manager/winbridge-manager \
  --screenshot /tmp/winbridge-manager.png
```

## Run Windows programs

Double-click an `.exe`, choose your installed Proton version on the first launch, and click **Open**. Subsequent launches reuse that choice.

Installers and standalone executables are both supported. Keep a portable application's supporting DLLs and data files alongside it as required by that application.

```sh
winbridge --list
winbridge '/path/to/application.exe'
winbridge --configure
```

Arguments following the executable are forwarded to the Windows application. Advanced per-launch overrides are available:

```sh
winbridge --proton '/path/to/Proton' '/path/to/application.exe'
winbridge --prefix '/path/to/compatdata' '/path/to/application.exe'
```

`WINBRIDGE_SEARCH_PATHS` accepts additional Proton search directories separated by colons. The older `PROTONRUN_SEARCH_PATHS` name remains supported.

## Shared environment and data

| Location | Purpose |
| --- | --- |
| `~/.local/share/winbridge/shared` | Shared Proton compatibility data |
| `~/.local/share/winbridge/shared/pfx/drive_c` | Windows `C:` drive |
| `~/.config/winbridge/settings.json` | Selected Proton and environment path |
| `~/.config/WinBridge/Manager.conf` | Manager interface preferences |
| `~/.local/state/winbridge/logs` | Launch and management logs |

The standard XDG data, config, and state directory overrides are supported. Apps in the shared environment use the same Windows registry and installed components.

Upgrades from ProtonRun preserve the old environment and settings in place. If a single older environment exists, WinBridge can adopt it. Multiple old environments are not merged automatically.

Flatpak Steam installations can be discovered, but applications run on the host and need working host/runtime dependencies.

## Desktop shortcuts

While a Windows program is running, WinBridge checks every three seconds for launchers Proton exports to `proton_shortcuts`. Supported shortcuts are imported into the Linux application menu and desktop folder, using the shared Proton preference.

There is no background desktop-folder watcher. Installers that do not export supported shortcuts are not automatically imported. GNOME needs desktop-icon support to display icons on the desktop itself; the application menu remains available. Your desktop may request **Allow Launching** the first time.

Existing exported shortcuts can be imported manually:

```sh
winbridge --import-shortcuts \
  --prefix '/path/to/compatdata' --proton '/path/to/Proton'
```

## Translations

The interface defaults to English. Translation dictionaries are UTF-8 JSON files with an `.i18n` extension:

- `manager/i18n/en-US.i18n` — English source strings
- `manager/i18n/no-NB.i18n` — Norwegian Bokmål

Each dictionary contains `locale`, `name`, and a `strings` object mapping English source strings to translations. Preserve placeholders such as `%1`. Missing translations fall back to the English source text. Dictionaries are compiled into the Qt application through `manager/i18n.qrc`.

To add another language, add a dictionary, include it in the resource file, and add its locale and display name to the language selector in `manager/settings.h`. Rebuild the app afterward. Windows programs and their own uninstall dialogs retain their own language settings.

## Release packages

```sh
./generate_release.sh --version 0.2.0
./generate_release.sh --version 0.2.0 --format arch
./generate_release.sh --version 0.2.0 --format deb
./generate_release.sh --version 0.2.0 --format rpm
```

The default builds all three formats into `dist/`, together with SHA-256 checksums. `--release` sets the package revision; `--output` selects another destination.

| Format | Additional build tools |
| --- | --- |
| DEB | `dpkg-deb` |
| RPM | `rpmbuild` |
| Arch | `makepkg`, `fakeroot` |

Run the build as a normal user. The script does not install packages or download missing tools. Packages include the compiled native executables (`winbridge`, `winbridge-backend`, `winbridge-manager`), desktop entries, and app icon. They are architecture-specific (`x86_64`/`amd64` or `aarch64`/`arm64`).

Build on the intended target distribution. A binary built against a newer Arch Qt/glibc stack is not generally compatible with older Debian or Fedora systems. Qt minimum-version metadata reflects the build machine. The Arch package has been built locally; DEB and RPM require their respective tools for validation.

`PACKAGER='Name <email>'` overrides maintainer metadata. `PACKAGE_LICENSE` accepts a license identifier; this project does not currently declare a distribution license, so package metadata defaults to `LicenseRef-Proprietary`.

## Tests

```sh
make test
# or directly with CTest:
ctest --test-dir build --output-on-failure
```

Tests cover core Proton discovery, Steam Linux Runtime resolution, shared-environment persistence, argument handling, shortcut importing and toggle synchronization, process tree scanning and termination, Manager UI searching, selection, uninstallation, translation fallback, and language persistence. Tests do not uninstall real Windows programs.

## Remove WinBridge

Choose another default `.exe` application through your file manager's **Open With** settings. From the project directory, remove a per-user installation with:

```sh
make uninstall PREFIX="$HOME/.local"
```

For a system-wide installation in `/usr/local`, use:

```sh
sudo make uninstall PREFIX=/usr/local
```

Use the same `PREFIX` as when installing if you chose a custom location. Older installations may also have app files in `~/.local/share/winbridge`.

The shared environment contains your installed Windows programs and data. Keep it if you still need those files. Removing WinBridge itself does not require deleting your Steam Proton installations.

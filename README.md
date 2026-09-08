# WinBridge

**Windows apps. At home on Linux.**

WinBridge lets you open Windows `.exe` files from your Linux desktop using Proton. Run installers and portable apps in one shared Windows environment, and access your Windows programs alongside your Linux applications.

**WinBridge Manager** brings your Windows programs together in a native Qt desktop app, with a searchable library, shortcut controls, and built-in access to each program's uninstaller.

![WinBridge Manager](assets/winbridge-manager.png)
![WinBridge Manager - Retro Dark](assets/winbridge-manager-dark.png)
For installation, build instructions, and configuration, see [INSTALL.md](INSTALL.md).

## What WinBridge does

- **Run Windows programs:** Open `.exe` files from KDE, GNOME, or another desktop environment. Choose Proton once and reuse that choice on later launches.
- **Run without Steam:** Download GE-Proton or UMU-Proton and its runtime from Settings using UMU.
- **Find your Proton versions:** Discover downloaded builds, Proton in Steam libraries, and custom builds such as GE-Proton.
- **Share one Windows environment:** Apps use the same Windows registry and installed components, and multiple programs can run at the same time.
- **Integrate with your desktop:** Import supported shortcuts exported by Proton into the Linux application menu and desktop.

By default, WinBridge reuses one shared Wine/Proton environment (a "prefix") for all apps. New apps reuse the existing Windows environment and shared components, reducing disk usage compared with creating a separate prefix for every app. They can also start faster when Wine services are already running in that environment. Startup gains depend on the app and whether the environment is already active; they are not guaranteed for every launch.

## WinBridge Manager

Open **WinBridge Manager** from your application menu to browse and manage your Windows programs.

- **Program library and search:** Browse registered programs with their icons and quickly filter the list.
- **Uninstall programs:** Launch a program's Windows uninstaller and automatically clean up its shortcuts and icons afterward.
- **Control shortcuts:** Enable or disable desktop and application menu entries independently for each app.
- **Manage running apps:** See which programs are running and stop frozen processes with **Kill app**.
- **Browse Windows files:** Open the shared **C:** drive in your desktop file manager.
- **Customize your setup:** Choose a Proton version, the Windows environment location, and English or Norwegian Bokmål in Settings.

## A familiar retro desktop

The Manager offers two Windows 9x-inspired themes: **Classic Light (Windows 98)** and **Retro Dark (Plus! Mystery)**, with live previews in Settings.

</details>

## Compatibility

WinBridge uses [umu-launcher](https://github.com/Open-Wine-Components/umu-launcher) to run Proton with its matching Steam Linux Runtime. Install UMU, then download Proton from Settings or select an existing build. The Steam client is not required. Existing Steam runtime installations remain usable when UMU is unavailable. Compatibility varies between Windows programs.

Both installers and portable applications are supported. Automatic shortcut import depends on the installer exporting supported shortcuts through Proton. Desktop icons also depend on your desktop environment's support; imported application menu entries remain available.

#!/usr/bin/env bash
# Build native packages without installing them or changing user settings.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
version=0.2.0
release=1
format=all
output_dir="$project_dir/dist"
usage() {
    cat <<'EOF'
Usage: ./generate_release.sh [--version 0.1.0] [--release 1]
                             [--format all|deb|rpm|arch] [--output DIR]
Builds unsigned native packages. Default: all formats into ./dist.
Tools: CMake, C++17 compiler, Qt6 Widgets development files; dpkg-deb (DEB), rpmbuild (RPM), makepkg + fakeroot (Arch).
Run as a normal user. No packages are installed by this script.
Optional metadata: PACKAGER='Name <email>', PACKAGE_LICENSE='SPDX identifier'.
Without an explicit project license, RPM/Arch metadata says LicenseRef-Proprietary.
EOF
}
while (($#)); do
    case "$1" in
        --version|--release|--format|--output)
            (($# >= 2)) || { usage >&2; exit 2; }
            case "$1" in
                --version) version=$2;;
                --release) release=$2;;
                --format) format=$2;;
                --output) output_dir=$2;;
            esac
            shift 2;;
        -h|--help) usage; exit 0;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; exit 2;;
    esac
done
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+([.][0-9]+)*$ ]] || { echo 'Version must be numeric, e.g. 0.1.0' >&2; exit 2; }
[[ $release =~ ^[1-9][0-9]*$ ]] || { echo 'Release must be a positive integer' >&2; exit 2; }
case "$format" in all) formats=(deb rpm arch);; deb|rpm|arch) formats=("$format");; *) usage >&2; exit 2;; esac
((EUID != 0)) || { echo 'Run as a normal user, without sudo.' >&2; exit 1; }
missing=()
for tool in cmake c++ pkg-config; do command -v "$tool" >/dev/null || missing+=("$tool"); done
for item in "${formats[@]}"; do
    case "$item" in deb) required=(dpkg-deb);; rpm) required=(rpmbuild);; arch) required=(makepkg fakeroot);; esac
    for tool in "${required[@]}"; do command -v "$tool" >/dev/null || missing+=("$tool"); done
done
if ((${#missing[@]})); then
    printf 'Missing build tools: %s\n' "${missing[*]}" >&2
    echo 'On Arch: sudo pacman -S --needed base-devel cmake qt6-base dpkg rpm-tools' >&2
    echo 'Or select an available format with --format deb|rpm|arch.' >&2
    exit 1
fi
maintainer=${PACKAGER:-WinBridge packager <packager@localhost>}
license=${PACKAGE_LICENSE:-LicenseRef-Proprietary}
[[ $maintainer != *$'\n'* && $maintainer != *$'\r'* ]] || exit 2
[[ $license =~ ^[A-Za-z0-9.+-]+$ ]] || { echo 'Invalid PACKAGE_LICENSE' >&2; exit 2; }
mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
work_dir=$(mktemp -d /tmp/winbridge-release.XXXXXXXX)
trap 'rm -rf -- "$work_dir"' EXIT
payload="$work_dir/payload"
install -dm755 "$payload/usr/bin" "$payload/usr/share/applications" "$payload/usr/share/doc/winbridge"
install -m644 "$project_dir/packaging/winbridge.desktop" "$payload/usr/share/applications/winbridge.desktop"
install -m644 "$project_dir/packaging/winbridge-manager.desktop" "$payload/usr/share/applications/winbridge-manager.desktop"
cmake -S "$project_dir" -B "$work_dir/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$work_dir/build" --parallel 2
install -m755 "$work_dir/build/winbridge" "$payload/usr/bin/winbridge"
install -m755 "$work_dir/build/winbridge-backend" "$payload/usr/bin/winbridge-backend"
install -m755 "$work_dir/build/manager/winbridge-manager" "$payload/usr/bin/winbridge-manager"
qt_version=$(pkg-config --modversion Qt6Widgets)
machine=$(uname -m)
case "$machine" in
    x86_64) deb_arch=amd64;;
    aarch64) deb_arch=arm64;;
    *) echo "Unsupported build architecture: $machine" >&2; exit 1;;
esac
install -Dm644 "$project_dir/assets/winbridge.png" "$payload/usr/share/pixmaps/winbridge.png"
install -m644 "$project_dir/README.md" "$payload/usr/share/doc/winbridge/README.md"
install -m644 "$project_dir/INSTALL.md" "$payload/usr/share/doc/winbridge/INSTALL.md"
if [[ -f $project_dir/LICENSE ]]; then
    install -Dm644 "$project_dir/LICENSE" "$payload/usr/share/licenses/winbridge/LICENSE"
fi
if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$payload/usr/share/applications/winbridge.desktop"
fi
artifacts=()
for item in "${formats[@]}"; do
    case "$item" in
        deb)
            root="$work_dir/deb"
            cp -a "$payload" "$root"
            mkdir -p "$root/DEBIAN"
            cat > "$root/DEBIAN/control" <<EOF
Package: winbridge
Version: $version-$release
Section: utils
Priority: optional
Architecture: $deb_arch
Maintainer: $maintainer
Depends: libqt6widgets6 (>= $qt_version), libqt6gui6 (>= $qt_version), libqt6core6 (>= $qt_version), libstdc++6, libc6, xdg-utils, xdg-user-dirs, zenity | kdialog
Recommends: desktop-file-utils
Description: Run Windows programs in a shared Proton environment
 Select an installed Proton version once, run Windows executables and
 import installer shortcuts into the Linux desktop and application menu.
EOF
            for hook in postinst postrm; do
                cat > "$root/DEBIAN/$hook" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
EOF
                chmod 755 "$root/DEBIAN/$hook"
            done
            artifact="winbridge_${version}-${release}_${deb_arch}.deb"
            dpkg-deb --root-owner-group -Zxz --build "$root" "$work_dir/$artifact"
            ;;
        rpm)
            top="$work_dir/rpm"
            mkdir -p "$top"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
            tar -C "$payload" -czf "$top/SOURCES/payload.tar.gz" .
            cat > "$top/SPECS/winbridge.spec" <<EOF
Name: winbridge
Version: $version
Release: $release
Summary: Run Windows programs in a shared Proton environment
License: $license
BuildArch: $machine
Source0: payload.tar.gz
Requires: qt6-qtbase >= $qt_version
Requires: xdg-utils
Requires: xdg-user-dirs
Requires: (zenity or kdialog)
AutoReqProv: yes

%description
Select an installed Proton version once, run Windows executables and
import installer shortcuts into the Linux desktop and application menu.

%prep
%setup -q -c -T

%build

%install
mkdir -p %{buildroot}
tar -xzf %{SOURCE0} -C %{buildroot}

%post
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || :
fi

%postun
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || :
fi

%files
%defattr(-,root,root,-)
/usr/bin/winbridge
/usr/bin/winbridge-backend
/usr/bin/winbridge-manager
/usr/share/applications/winbridge.desktop
/usr/share/applications/winbridge-manager.desktop
/usr/share/pixmaps/winbridge.png
%doc /usr/share/doc/winbridge/
EOF
            if [[ -f $project_dir/LICENSE ]]; then
                echo '%license /usr/share/licenses/winbridge/' >> "$top/SPECS/winbridge.spec"
            fi
            rpmbuild --define "_topdir $top" --define '_build_id_links none' --define '__os_install_post %{nil}' -bb "$top/SPECS/winbridge.spec"
            artifact="winbridge-${version}-${release}.${machine}.rpm"
            cp "$top/RPMS/$machine/$artifact" "$work_dir/$artifact"
            ;;
        arch)
            arch_dir="$work_dir/arch"
            mkdir -p "$arch_dir"
            tar -C "$payload" -czf "$arch_dir/payload.tar.gz" .
            checksum=$(sha256sum "$arch_dir/payload.tar.gz")
            checksum=${checksum%% *}
            cat > "$arch_dir/PKGBUILD" <<EOF
pkgname=winbridge
pkgver=$version
pkgrel=$release
pkgdesc='Run Windows programs in a shared Proton environment'
arch=('$machine')
license=('$license')
depends=('qt6-base>=$qt_version' 'gcc-libs' 'glibc' 'xdg-utils' 'xdg-user-dirs' 'zenity')
optdepends=('kdialog: native KDE dialogs' 'steam: install Proton and Steam Linux Runtime')
source=('payload.tar.gz')
noextract=('payload.tar.gz')
sha256sums=('$checksum')
options=('!strip' '!debug')
package() {
    tar -xzf "\$srcdir/payload.tar.gz" -C "\$pkgdir"
}
EOF
            (cd "$arch_dir" && PKGDEST="$work_dir" PKGEXT=.pkg.tar.zst makepkg --nodeps --noconfirm)
            artifact="winbridge-${version}-${release}-${machine}.pkg.tar.zst"
            ;;
    esac
    install -m644 "$work_dir/$artifact" "$output_dir/$artifact"
    artifacts+=("$artifact")
done
(cd "$output_dir" && sha256sum -- "${artifacts[@]}" > "SHA256SUMS-${version}-${release}-${format}")
printf '\nBuilt packages in %s:\n' "$output_dir"
printf '  %s\n' "${artifacts[@]}"

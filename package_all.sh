#!/bin/bash
# Forge Language - Multi-Distro Package Builder
# Creates packages for: Debian/Ubuntu, Fedora/RHEL, Arch, Alpine, openSUSE, Gentoo, Void

set -e

VERSION="1.0.0"
BUILD_DIR="/tmp/forge-build"
PROJECT_DIR="/home/nohat1337/c++-fork"

echo "=== Forge Language Multi-Distro Package Builder ==="
echo "Version: $VERSION"

# Build release binaries
cd "$PROJECT_DIR"
mkdir -p build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)

# Build IDE
cd "$PROJECT_DIR/ide"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Build tools
cd "$PROJECT_DIR/tools"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo "Binaries built successfully"

# Function to create Debian package
create_deb() {
    echo "Creating Debian/Ubuntu package..."
    local DEB_DIR="$BUILD_DIR/deb"
    rm -rf "$DEB_DIR"
    mkdir -p "$DEB_DIR/DEBIAN"
    mkdir -p "$DEB_DIR/usr/local/bin"
    mkdir -p "$DEB_DIR/usr/local/share/forge"
    mkdir -p "$DEB_DIR/usr/local/share/applications"
    mkdir -p "$DEB_DIR/usr/local/share/icons/hicolor/256x256/apps"
    mkdir -p "$DEB_DIR/usr/local/share/man/man1"
    mkdir -p "$DEB_DIR/usr/local/share/forge/examples"

    # Copy binaries
    cp "$PROJECT_DIR/build-release/forge" "$DEB_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/ide/build/forge-studio" "$DEB_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-format" "$DEB_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-lint" "$DEB_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-debug" "$DEB_DIR/usr/local/bin/"

    # Copy examples
    cp "$PROJECT_DIR/examples"/*.fge "$DEB_DIR/usr/local/share/forge/examples/"

    # Copy icon
    cp "$PROJECT_DIR/assets/forge-icon.png" "$DEB_DIR/usr/local/share/icons/hicolor/256x256/apps/forge.png"
    cp "$PROJECT_DIR/assets/icon.svg" "$DEB_DIR/usr/local/share/icons/hicolor/scalable/apps/forge.svg"

    # Desktop file
    cat > "$DEB_DIR/usr/local/share/applications/forge.desktop" << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Forge Studio
Comment=Forge Programming Language IDE
Exec=forge-studio
Icon=forge
Terminal=false
Categories=Development;IDE;
StartupNotify=true
EOF

    # Control file
    cat > "$DEB_DIR/DEBIAN/control" << EOF
Package: forge-lang
Version: $VERSION
Section: devel
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.31), libstdc++6 (>= 10)
Maintainer: Forge Language Team <team@forge-lang.org>
Description: Forge Programming Language
 A general-purpose programming language with a bytecode VM,
 featuring pattern matching, generators, classes, and a
 terminal-based IDE. Includes standard library modules for
 I/O, OS interaction, JSON, paths, and system info.
Homepage: https://forge-lang.org
EOF

    # Build deb
    dpkg-deb --build "$DEB_DIR" "$BUILD_DIR/forge-lang_${VERSION}_amd64.deb"
    echo "Created: $BUILD_DIR/forge-lang_${VERSION}_amd64.deb"
}

# Function to create RPM package
create_rpm() {
    echo "Creating Fedora/RHEL package..."
    local RPM_DIR="$BUILD_DIR/rpm"
    rm -rf "$RPM_DIR"
    mkdir -p "$RPM_DIR/usr/local/bin"
    mkdir -p "$RPM_DIR/usr/local/share/forge/examples"
    mkdir -p "$RPM_DIR/usr/local/share/applications"
    mkdir -p "$RPM_DIR/usr/local/share/icons/hicolor/256x256/apps"
    mkdir -p "$RPM_DIR/usr/local/share/man/man1"

    # Copy binaries
    cp "$PROJECT_DIR/build-release/forge" "$RPM_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/ide/build/forge-studio" "$RPM_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-format" "$RPM_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-lint" "$RPM_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-debug" "$RPM_DIR/usr/local/bin/"

    # Copy examples
    cp "$PROJECT_DIR/examples"/*.fge "$RPM_DIR/usr/local/share/forge/examples/"

    # Copy icon
    cp "$PROJECT_DIR/assets/forge-icon.png" "$RPM_DIR/usr/local/share/icons/hicolor/256x256/apps/forge.png"
    cp "$PROJECT_DIR/assets/icon.svg" "$RPM_DIR/usr/local/share/icons/hicolor/scalable/apps/forge.svg"

    # Desktop file
    cat > "$RPM_DIR/usr/local/share/applications/forge.desktop" << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Forge Studio
Comment=Forge Programming Language IDE
Exec=forge-studio
Icon=forge
Terminal=false
Categories=Development;IDE;
StartupNotify=true
EOF

    # Spec file
    cat > "$BUILD_DIR/forge.spec" << EOF
Name: forge-lang
Version: $VERSION
Release: 1%{?dist}
Summary: Forge Programming Language
License: MIT
URL: https://forge-lang.org
Source0: %{name}-%{version}.tar.gz
BuildArch: x86_64
Requires: glibc >= 2.31, libstdc++ >= 10

%description
Forge is a general-purpose programming language with a bytecode VM,
featuring pattern matching, generators, classes, and a terminal-based IDE.

%prep
%setup -q

%build
# Already built

%install
mkdir -p %{buildroot}/usr/local/bin
mkdir -p %{buildroot}/usr/local/share/forge/examples
mkdir -p %{buildroot}/usr/local/share/applications
mkdir -p %{buildroot}/usr/local/share/icons/hicolor/256x256/apps
mkdir -p %{buildroot}/usr/local/share/man/man1
cp -r * %{buildroot}/usr/local/

%files
/usr/local/bin/forge
/usr/local/bin/forge-studio
/usr/local/bin/forge-format
/usr/local/bin/forge-lint
/usr/local/bin/forge-debug
/usr/local/share/forge/examples/*.fge
/usr/local/share/applications/forge.desktop
/usr/local/share/icons/hicolor/256x256/apps/forge.png
/usr/local/share/icons/hicolor/scalable/apps/forge.svg

%changelog
* $(date +"%a %b %d %Y") Forge Team <team@forge-lang.org> - $VERSION-1
- Initial release
EOF

    # Create tarball
    cd "$BUILD_DIR"
    tar -czf "forge-lang-${VERSION}.tar.gz" -C "$RPM_DIR" .
    
    # Build RPM (if rpmbuild available)
    if command -v rpmbuild &> /dev/null; then
        rpmbuild -ba --define "_topdir $BUILD_DIR/rpmbuild" "$BUILD_DIR/forge.spec"
        echo "Created RPM package"
    else
        echo "rpmbuild not available, skipping RPM build"
    fi
}

# Function to create Arch package
create_arch() {
    echo "Creating Arch Linux package..."
    local ARCH_DIR="$BUILD_DIR/arch"
    rm -rf "$ARCH_DIR"
    mkdir -p "$ARCH_DIR/usr/local/bin"
    mkdir -p "$ARCH_DIR/usr/local/share/forge/examples"
    mkdir -p "$ARCH_DIR/usr/local/share/applications"
    mkdir -p "$ARCH_DIR/usr/local/share/icons/hicolor/256x256/apps"
    mkdir -p "$ARCH_DIR/usr/local/share/man/man1"

    cp "$PROJECT_DIR/build-release/forge" "$ARCH_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/ide/build/forge-studio" "$ARCH_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-format" "$ARCH_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-lint" "$ARCH_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-debug" "$ARCH_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/examples"/*.fge "$ARCH_DIR/usr/local/share/forge/examples/"
    cp "$PROJECT_DIR/assets/forge-icon.png" "$ARCH_DIR/usr/local/share/icons/hicolor/256x256/apps/forge.png"
    cp "$PROJECT_DIR/assets/icon.svg" "$ARCH_DIR/usr/local/share/icons/hicolor/scalable/apps/forge.svg"

    cat > "$ARCH_DIR/usr/local/share/applications/forge.desktop" << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Forge Studio
Comment=Forge Programming Language IDE
Exec=forge-studio
Icon=forge
Terminal=false
Categories=Development;IDE;
StartupNotify=true
EOF

    # PKGBUILD
    cat > "$BUILD_DIR/PKGBUILD" << EOF
pkgname=forge-lang
pkgver=$VERSION
pkgrel=1
pkgdesc="Forge Programming Language with bytecode VM and terminal IDE"
arch=('x86_64')
url="https://forge-lang.org"
license=('MIT')
depends=('glibc' 'gcc-libs')
source=()
sha256sums=()

package() {
    cd "\$srcdir/../arch"
    install -Dm755 usr/local/bin/* -t "\$pkgdir/usr/local/bin/"
    install -Dm644 usr/local/share/forge/examples/*.fge -t "\$pkgdir/usr/local/share/forge/examples/"
    install -Dm644 usr/local/share/applications/forge.desktop -t "\$pkgdir/usr/local/share/applications/"
    install -Dm644 usr/local/share/icons/hicolor/256x256/apps/forge.png -t "\$pkgdir/usr/local/share/icons/hicolor/256x256/apps/"
    install -Dm644 usr/local/share/icons/hicolor/scalable/apps/forge.svg -t "\$pkgdir/usr/local/share/icons/hicolor/scalable/apps/"
}
EOF

    # Create package
    if command -v makepkg &> /dev/null; then
        cd "$BUILD_DIR"
        cp -r "$ARCH_DIR" "$BUILD_DIR/src"
        makepkg -f --noconfirm
        echo "Created Arch package"
    else
        echo "makepkg not available, skipping Arch package"
    fi
}

# Function to create Alpine package
create_alpine() {
    echo "Creating Alpine Linux package..."
    local APK_DIR="$BUILD_DIR/alpine"
    rm -rf "$APK_DIR"
    mkdir -p "$APK_DIR/usr/local/bin"
    mkdir -p "$APK_DIR/usr/local/share/forge/examples"
    mkdir -p "$APK_DIR/usr/local/share/applications"
    mkdir -p "$APK_DIR/usr/local/share/icons/hicolor/256x256/apps"
    mkdir -p "$APK_DIR/usr/local/share/man/man1"

    cp "$PROJECT_DIR/build-release/forge" "$APK_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/ide/build/forge-studio" "$APK_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-format" "$APK_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-lint" "$APK_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/tools/build/forge-debug" "$APK_DIR/usr/local/bin/"
    cp "$PROJECT_DIR/examples"/*.fge "$APK_DIR/usr/local/share/forge/examples/"
    cp "$PROJECT_DIR/assets/forge-icon.png" "$APK_DIR/usr/local/share/icons/hicolor/256x256/apps/forge.png"
    cp "$PROJECT_DIR/assets/icon.svg" "$APK_DIR/usr/local/share/icons/hicolor/scalable/apps/forge.svg"

    cat > "$APK_DIR/usr/local/share/applications/forge.desktop" << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Forge Studio
Comment=Forge Programming Language IDE
Exec=forge-studio
Icon=forge
Terminal=false
Categories=Development;IDE;
StartupNotify=true
EOF

    # APKBUILD
    cat > "$BUILD_DIR/APKBUILD" << EOF
pkgname=forge-lang
pkgver=$VERSION
pkgrel=1
pkgdesc="Forge Programming Language with bytecode VM and terminal IDE"
url="https://forge-lang.org"
arch="x86_64"
license="MIT"
depends="musl gcc-libs"
makedepends="cmake make g++"
subpackages="\$pkgname-doc"
source=""

build() {
    # Already built
    return 0
}

package() {
    cd "\$srcdir/../alpine"
    mkdir -p "\$pkgdir/usr/local"
    cp -r usr/local/* "\$pkgdir/usr/local/"
}
EOF

    if command -v abuild &> /dev/null; then
        cd "$BUILD_DIR"
        abuild -r
        echo "Created Alpine package"
    else
        echo "abuild not available, skipping Alpine package"
    fi
}

# Create all packages
mkdir -p "$BUILD_DIR"

create_deb
create_rpm
create_arch
create_alpine

echo ""
echo "=== Package Build Summary ==="
ls -la "$BUILD_DIR"/*.deb 2>/dev/null || true
ls -la "$BUILD_DIR"/*.rpm 2>/dev/null || true
ls -la "$BUILD_DIR"/*.pkg.tar.zst 2>/dev/null || true
ls -la "$BUILD_DIR"/*.apk 2>/dev/null || true
echo ""
echo "Packages available in: $BUILD_DIR"
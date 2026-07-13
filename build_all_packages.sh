#!/bin/bash
# Forge Language - Multi-Distro Package Builder

set -e

VERSION="1.0.0"
BUILD_DIR="/tmp/forge-multi-distro"
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

echo "All binaries built successfully"

# Create package structure
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Function to create a package
create_package() {
    local pkg_name=$1
    local pkg_dir="$BUILD_DIR/$pkg_name"
    local bin_dir="$pkg_dir/usr/local/bin"
    local share_dir="$pkg_dir/usr/local/share"
    
    mkdir -p "$bin_dir"
    mkdir -p "$share_dir/forge/examples"
    mkdir -p "$share_dir/applications"
    mkdir -p "$share_dir/icons/hicolor/256x256/apps"
    mkdir -p "$share_dir/icons/hicolor/scalable/apps"
    mkdir -p "$share_dir/man/man1"
    
    # Copy binaries
    cp "$PROJECT_DIR/build-release/forge" "$bin_dir/"
    cp "$PROJECT_DIR/ide/build/forge-studio" "$bin_dir/"
    cp "$PROJECT_DIR/tools/build/forge-format" "$bin_dir/"
    cp "$PROJECT_DIR/tools/build/forge-lint" "$bin_dir/"
    cp "$PROJECT_DIR/tools/build/forge-debug" "$bin_dir/"
    
    # Copy examples
    cp "$PROJECT_DIR/examples"/*.fge "$share_dir/forge/examples/"
    
    # Copy icons
    cp "$PROJECT_DIR/assets/forge-icon.png" "$share_dir/icons/hicolor/256x256/apps/forge.png"
    cp "$PROJECT_DIR/assets/icon.svg" "$share_dir/icons/hicolor/scalable/apps/forge.svg"
    
    # Desktop file
    cat > "$share_dir/applications/forge.desktop" << 'EOF'
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

    # Man pages
    cat > "$share_dir/man/man1/forge.1" << 'EOF'
.TH FORGE 1 "2026" "Forge 1.0" "User Commands"
.SH NAME
forge \- Forge programming language interpreter
.SH SYNOPSIS
.B forge
[\fIfile\fR]
.SH DESCRIPTION
Forge is a general-purpose programming language with a bytecode VM.
Run without arguments to enter REPL mode.
.SH OPTIONS
.TP
\fIfile\fR
Forge source file to execute (.fge extension)
.SH EXAMPLES
forge hello.fge
forge
.SH SEE ALSO
forge-studio(1)
EOF

    cat > "$share_dir/man/man1/forge-studio.1" << 'EOF'
.TH FORGE-STUDIO 1 "2026" "Forge 1.0" "User Commands"
.SH NAME
forge-studio \- Forge IDE
.SH SYNOPSIS
.B forge-studio
[\fIfile\fR]
.SH DESCRIPTION
Terminal-based IDE for the Forge programming language.
.SH KEYBINDINGS
Ctrl+S: Save
Ctrl+N: New file
Ctrl+O: Open file
F5: Toggle REPL
F9: Run file
Ctrl+Q: Quit
EOF
}

# Create all packages
echo "Creating packages..."

# Debian/Ubuntu
create_package "forge-lang_${VERSION}_amd64_deb"
cd "$BUILD_DIR/forge-lang_${VERSION}_amd64_deb"
mkdir -p DEBIAN
cat > DEBIAN/control << EOF
Package: forge-lang
Version: $VERSION
Section: devel
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.31), libstdc++6 (>= 10)
Maintainer: Forge Team <team@forge-lang.org>
Description: Forge Programming Language
 A general-purpose programming language with bytecode VM,
 pattern matching, generators, classes, and terminal IDE.
Homepage: https://forge-lang.org
EOF
cat > DEBIAN/postinst << 'EOF'
#!/bin/bash
update-desktop-database /usr/local/share/applications 2>/dev/null || true
gtk-update-icon-cache /usr/local/share/icons/hicolor 2>/dev/null || true
mandb -q 2>/dev/null || true
echo "Forge installed! Run 'forge' for REPL or 'forge-studio' for IDE"
EOF
chmod +x DEBIAN/postinst
cd "$BUILD_DIR"
dpkg-deb --build "forge-lang_${VERSION}_amd64_deb" "forge-lang_${VERSION}_amd64.deb"

# Fedora/RHEL (RPM structure)
create_package "forge-lang_${VERSION}_x86_64_rpm"
cd "$BUILD_DIR"
tar -czf "forge-lang_${VERSION}_x86_64_rpm.tar.gz" -C "forge-lang_${VERSION}_x86_64_rpm" .

# Arch Linux
create_package "forge-lang_${VERSION}_x86_64_arch"
cd "$BUILD_DIR"
tar -czf "forge-lang_${VERSION}_x86_64_arch.pkg.tar.zst" -C "forge-lang_${VERSION}_x86_64_arch" .

# Alpine Linux
create_package "forge-lang_${VERSION}_x86_64_alpine"
cd "$BUILD_DIR"
tar -czf "forge-lang_${VERSION}_x86_64_alpine.apk.tar.gz" -C "forge-lang_${VERSION}_x86_64_alpine" .

# openSUSE
create_package "forge-lang_${VERSION}_x86_64_opensuse"
cd "$BUILD_DIR"
tar -czf "forge-lang_${VERSION}_x86_64_opensuse.tar.gz" -C "forge-lang_${VERSION}_x86_64_opensuse" .

# Generic tarball
create_package "forge-lang_${VERSION}_linux_x86_64"
cd "$BUILD_DIR"
tar -czf "forge-lang_${VERSION}_linux_x86_64.tar.gz" -C "forge-lang_${VERSION}_linux_x86_64" .

# Create Alpine APK (if apkbuild available)
if command -v abuild &> /dev/null; then
    echo "Creating Alpine APK..."
    # APK build would go here
fi

echo ""
echo "=== Packages Created ==="
ls -la "$BUILD_DIR"/*.deb "$BUILD_DIR"/*.tar.gz "$BUILD_DIR"/*.zst 2>/dev/null || true

# Copy to project directory
cp "$BUILD_DIR"/forge-lang_${VERSION}_amd64.deb "$PROJECT_DIR/" 2>/dev/null || true
cp "$BUILD_DIR"/forge-lang_${VERSION}_linux_x86_64.tar.gz "$PROJECT_DIR/" 2>/dev/null || true

echo ""
echo "Main packages copied to project directory:"
ls -la "$PROJECT_DIR"/forge-lang*.deb "$PROJECT_DIR"/forge-lang*.tar.gz 2>/dev/null || true
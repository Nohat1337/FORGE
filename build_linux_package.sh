#!/bin/bash
# Forge Language - Linux Installer
# Creates a deb package and installs forge

set -e

FORGE_VERSION="1.0.0"
BUILD_DIR="/tmp/forge-pkg"
INSTALL_PREFIX="/usr/local"

echo "=== Forge Language Installer ==="
echo "Building version $FORGE_VERSION"

# Create package structure
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/DEBIAN"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/bin"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/forge"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/applications"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/icons/hicolor/256x256/apps"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/icons/hicolor/scalable/apps"
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/man/man1"

# Build forge
cd /home/nohat1337/c++-fork
mkdir -p build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
make -j$(nproc)

# Install binaries
cp forge "$BUILD_DIR$INSTALL_PREFIX/bin/forge"
cp ../ide/build/forge-studio "$BUILD_DIR$INSTALL_PREFIX/bin/forge-studio"

# Install standard library
cp -r ../src/*.hpp ../src/*.cpp "$BUILD_DIR$INSTALL_PREFIX/share/forge/src/" 2>/dev/null || true

# Install examples
mkdir -p "$BUILD_DIR$INSTALL_PREFIX/share/forge/examples"
cp ../examples/*.fge "$BUILD_DIR$INSTALL_PREFIX/share/forge/examples/"

# Install icon
cp ../assets/forge-icon.png "$BUILD_DIR$INSTALL_PREFIX/share/icons/hicolor/256x256/apps/forge.png"
cp ../assets/icon.svg "$BUILD_DIR$INSTALL_PREFIX/share/icons/hicolor/scalable/apps/forge.svg"

# Install desktop file
cat > "$BUILD_DIR$INSTALL_PREFIX/share/applications/forge.desktop" << 'DESKTOP'
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
DESKTOP

# Install man page
cat > "$BUILD_DIR$INSTALL_PREFIX/share/man/man1/forge.1" << 'MAN'
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
MAN

cat > "$BUILD_DIR$INSTALL_PREFIX/share/man/man1/forge-studio.1" << 'MAN'
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
MAN

# Create DEBIAN/control
cat > "$BUILD_DIR/DEBIAN/control" << CONTROL
Package: forge-lang
Version: $FORGE_VERSION
Section: devel
Priority: optional
Architecture: amd64
Maintainer: Forge Team <forge@forge-lang.org>
Description: Forge Programming Language
 A modern, expressive programming language with a bytecode VM,
 standard library, REPL, and integrated terminal IDE.
 Includes forge (interpreter) and forge-studio (IDE).
CONTROL

# Create postinst
cat > "$BUILD_DIR/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
set -e
update-desktop-database /usr/local/share/applications 2>/dev/null || true
update-icon-caches /usr/local/share/icons/hicolor 2>/dev/null || true
mandb -q 2>/dev/null || true
echo "Forge installed successfully!"
echo "Run 'forge' for REPL or 'forge-studio' for IDE"
POSTINST
chmod +x "$BUILD_DIR/DEBIAN/postinst"

# Build .deb
dpkg-deb --build "$BUILD_DIR" "/home/nohat1337/c++-fork/forge-lang_${FORGE_VERSION}_amd64.deb"

echo "=== Package created ==="
ls -la "/home/nohat1337/c++-fork/forge-lang_${FORGE_VERSION}_amd64.deb"

# Install if requested
if [ "$1" = "--install" ]; then
    sudo dpkg -i "/home/nohat1337/c++-fork/forge-lang_${FORGE_VERSION}_amd64.deb"
fi
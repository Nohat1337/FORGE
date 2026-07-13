#!/bin/bash
# Forge Language - Universal Linux Installer
# Works on: Debian/Ubuntu, Fedora/RHEL, Arch, Alpine, openSUSE, Gentoo, Void, NixOS

set -e

FORGE_VERSION="1.0.0"
INSTALL_PREFIX="${FORGE_PREFIX:-/usr/local}"

echo "=== Forge Language Universal Linux Installer ==="
echo "Version: $FORGE_VERSION"
echo "Install prefix: $INSTALL_PREFIX"

# Detect package manager and install dependencies
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/alpine-release ]; then
        echo "alpine"
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
echo "Detected distribution: $DISTRO"

install_deps() {
    case "$DISTRO" in
        ubuntu|debian|linuxmint|pop|kali)
            apt-get update && apt-get install -y build-essential cmake git
            ;;
        fedora|rhel|centos|rocky|almalinux)
            dnf install -y gcc-c++ cmake make git || yum install -y gcc-c++ cmake make git
            ;;
        arch|manjaro|endeavouros)
            pacman -S --needed --noconfirm base-devel cmake git
            ;;
        alpine)
            apk add build-base cmake git
            ;;
        opensuse*|sles)
            zypper install -y gcc-c++ cmake make git
            ;;
        gentoo)
            emerge --ask sys-devel/gcc dev-util/cmake dev-vcs/git
            ;;
        void)
            xbps-install -S gcc cmake git
            ;;
        nixos)
            echo "On NixOS, use: nix-shell -p gcc cmake git"
            exit 1
            ;;
        *)
            echo "Unknown distro. Please install: g++, cmake, make, git manually"
            ;;
    esac
}

# Check if we need to install deps (only if not root and not in container)
if [ "$EUID" -ne 0 ] && [ ! -f /.dockerenv ]; then
    if ! command -v cmake &> /dev/null || ! command -v g++ &> /dev/null; then
        echo "Installing build dependencies..."
        install_deps
    fi
fi

# Build forge
echo "Building Forge..."
cd /home/nohat1337/c++-fork
mkdir -p build-release
cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
make -j$(nproc)

# Install
echo "Installing to $INSTALL_PREFIX..."
make install

# Install IDE if built
if [ -f /home/nohat1337/c++-fork/ide/build/forge-studio ]; then
    cp /home/nohat1337/c++-fork/ide/build/forge-studio "$INSTALL_PREFIX/bin/forge-studio"
fi

# Install standard library
mkdir -p "$INSTALL_PREFIX/share/forge/examples"
cp /home/nohat1337/c++-fork/examples/*.fge "$INSTALL_PREFIX/share/forge/examples/" 2>/dev/null || true

# Install icons
mkdir -p "$INSTALL_PREFIX/share/icons/hicolor/256x256/apps"
mkdir -p "$INSTALL_PREFIX/share/icons/hicolor/scalable/apps"
cp /home/nohat1337/c++-fork/assets/forge-icon.png "$INSTALL_PREFIX/share/icons/hicolor/256x256/apps/forge.png"
cp /home/nohat1337/c++-fork/assets/icon.svg "$INSTALL_PREFIX/share/icons/hicolor/scalable/apps/forge.svg"

# Desktop entry
mkdir -p "$INSTALL_PREFIX/share/applications"
cat > "$INSTALL_PREFIX/share/applications/forge.desktop" << 'DESKTOP'
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

# Update desktop database and icon cache
update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true

# Verify installation
if [ -f "$INSTALL_PREFIX/bin/forge" ]; then
    echo ""
    echo "=== Installation Successful! ==="
    echo ""
    echo "Run 'forge' to start REPL"
    echo "Run 'forge file.fge' to execute a file"
    echo "Run 'forge-studio' to open the IDE"
    echo ""
    "$INSTALL_PREFIX/bin/forge" --version 2>/dev/null || "$INSTALL_PREFIX/bin/forge" -e 'print("Forge v1.0.0 ready!")'
else
    echo "ERROR: Installation failed - forge binary not found"
    exit 1
fi
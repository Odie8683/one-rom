#!/bin/sh
set -e

# Builds One ROM CLI deb packages for Linux (x86_64 and arm64).
#
# Note: The deps section currently only installs cross-compilation toolchains,
# as nusb is a pure Rust USB implementation with no system library dependencies.
# If probe-rs support is added in future, libudev-dev and libusb-1.0-0-dev will
# be required for both architectures, following the same pattern as the Studio
# build script.
#
# Pre-requisites:
# - Rust:
#
# ```sh
#   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# ```

# Check we're running on Linux
if [ "$(uname -s)" != "Linux" ]; then
    echo "Error: This script must be run on Linux" >&2
    exit 1
fi

# Parse arguments
CLEAN=true
DEPS=true

for arg in "$@"; do
    case "$arg" in
        noclean)
            echo "!!! WARNING: Not cleaning cargo artifacts" >&2
            CLEAN=false
            ;;
        nodeps)
            echo "!!! WARNING: Skipping dependencies installation step" >&2
            DEPS=false
            ;;
        *)
            echo "Error: Unknown argument '$arg'" >&2
            echo "Usage: $0 [noclean] [nodeps]" >&2
            exit 1
            ;;
    esac
done

#
# Variables
#

VERSION=$(grep "^version" Cargo.toml | head -1 | sed 's/.*"\(.*\)".*/\1/')

#
# Dependencies
#

if [ "$DEPS" = true ]; then
    echo "Installing dependencies..."

    # Clean up any Ubuntu repo files from previous runs
    sudo rm -f /etc/apt/sources.list.d/ubuntu-ports.list
    sudo rm -f /etc/apt/sources.list.d/ubuntu-amd64.sources
    sudo rm -f /etc/apt/sources.list.d/ubuntu-archive.list

    sudo apt update && sudo apt install -y gcc-aarch64-linux-gnu

    # Detect OS and host architecture
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS_ID="$ID"
    else
        echo "ERROR: Cannot detect OS" >&2
        exit 1
    fi
    HOST_ARCH=$(dpkg --print-architecture)
    echo "Detected OS: $OS_ID, host architecture: $HOST_ARCH"

    if [ "$OS_ID" = "debian" ]; then
        sudo dpkg --add-architecture arm64
        sudo dpkg --add-architecture amd64
        sudo apt update

    elif [ "$OS_ID" = "ubuntu" ]; then
        CODENAME=$(lsb_release -sc)

        if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then
            sudo cp /etc/apt/sources.list.d/ubuntu.sources /tmp/ubuntu.sources.backup
            sudo sed -i '/^Architectures:/d' /etc/apt/sources.list.d/ubuntu.sources
            sudo sed -i "/^Types:/a Architectures: ${HOST_ARCH}" /etc/apt/sources.list.d/ubuntu.sources
        fi

        if [ "$HOST_ARCH" = "amd64" ]; then
            sudo dpkg --add-architecture arm64
            echo "Configuring ports.ubuntu.com for arm64 packages"
            echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME} main universe" | sudo tee /etc/apt/sources.list.d/ubuntu-ports.list
            echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-updates main universe" | sudo tee -a /etc/apt/sources.list.d/ubuntu-ports.list
            echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports ${CODENAME}-security main universe" | sudo tee -a /etc/apt/sources.list.d/ubuntu-ports.list
            sudo apt update

        elif [ "$HOST_ARCH" = "arm64" ]; then
            sudo dpkg --add-architecture amd64
            echo "Configuring archive.ubuntu.com for amd64 packages"
            echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu ${CODENAME} main universe" | sudo tee /etc/apt/sources.list.d/ubuntu-archive.list
            echo "deb [arch=amd64] http://archive.ubuntu.com/ubuntu ${CODENAME}-updates main universe" | sudo tee -a /etc/apt/sources.list.d/ubuntu-archive.list
            echo "deb [arch=amd64] http://security.ubuntu.com/ubuntu ${CODENAME}-security main universe" | sudo tee -a /etc/apt/sources.list.d/ubuntu-archive.list
        fi

    else
        echo "ERROR: Unsupported OS: $OS_ID" >&2
        exit 1
    fi

    # Install the Rust targets
    rustup target add x86_64-unknown-linux-gnu
    rustup target add aarch64-unknown-linux-gnu

    # Install cargo-deb
    # Forced to ensure it builds against glibc version on this system
    cargo install cargo-deb --locked --force
else
    echo "Skipping dependencies installation step."
fi

#
# Clean previous builds
#

if [ "$CLEAN" = true ]; then
    echo "Cleaning previous build artifacts..."
    cargo clean --target x86_64-unknown-linux-gnu
    cargo clean --target aarch64-unknown-linux-gnu
    rm -f dist/*.deb
else
    echo "Skipping cleaning of previous build artifacts."
fi

mkdir -p dist

#
# Intel (x86_64)
#

TARGET="x86_64-unknown-linux-gnu"
export CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_LINKER=x86_64-linux-gnu-gcc
echo "Building One ROM CLI version: $VERSION for $TARGET"
cargo build --bin onerom --release --target $TARGET

echo "Packaging deb for $TARGET"
cargo deb -v --target $TARGET
echo "Linux x86_64 build complete."

#
# ARM (aarch64)
#

TARGET="aarch64-unknown-linux-gnu"
export CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc
echo "Building One ROM CLI version: $VERSION for $TARGET"
cargo build --bin onerom --release --target $TARGET

echo "Packaging deb for $TARGET"
cargo deb -v --target $TARGET
echo "Linux arm64 build complete."

# Copy debs to dist/
cp ../target/x86_64-unknown-linux-gnu/debian/*.deb dist/
cp ../target/aarch64-unknown-linux-gnu/debian/*.deb dist/

echo "Build complete. Artifacts in dist/:"
ls -lh dist/*.deb
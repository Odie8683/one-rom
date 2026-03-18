#!/bin/sh
set -e

# Builds One ROM CLI for macOS (universal binary: arm64 + x86_64)
#
# Required env variables:
# - CODESIGN_IDENTITY: The codesign identity to use for signing.
#   Only required if not building with "nosign" option.
#
# The notarytool uses a keychain profile named "onerom-notarization".
# Set this up in advance with:
#
# ```sh
# xcrun notarytool store-credentials "onerom-notarization" \
#  --apple-id your@email.com \
#  --team-id XXXXXXXXXX \
#  --password xxxx-xxxx-xxxx-xxxx
# ```
#
# Args:
# - nosign: Build without code signing or notarization.
# - noclean: Skip the cargo clean step.
# - nodeps: Skip the dependencies installation step.

#
# Checks
#

if [ "$(uname -s)" != "Darwin" ]; then
    echo "Error: This script must be run on macOS" >&2
    exit 1
fi

SIGN=true
CLEAN=true
DEPS=true

for arg in "$@"; do
    case "$arg" in
        nosign)
            echo "!!! WARNING: Building without code signing or notarization" >&2
            SIGN=false
            ;;
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
            echo "Usage: $0 [nosign] [noclean] [nodeps]" >&2
            exit 1
            ;;
    esac
done

if [ "$SIGN" = true ] && [ -z "${CODESIGN_IDENTITY:-}" ]; then
    echo "Error: CODESIGN_IDENTITY environment variable must be set"
    exit 1
fi

#
# Variables
#

TARGET_ARM="aarch64-apple-darwin"
TARGET_X86="x86_64-apple-darwin"
DIST_DIR="dist"
BINARY="${DIST_DIR}/onerom"
VERSION=$(grep "^version" Cargo.toml | head -1 | sed 's/.*"\(.*\)".*/\1/')

#
# Dependencies
#

if [ "$DEPS" = true ]; then
    echo "Installing dependencies..."
    rustup target add aarch64-apple-darwin
    rustup target add x86_64-apple-darwin
else
    echo "Skipping dependencies installation step."
fi

#
# Clean
#

if [ "$CLEAN" = true ]; then
    echo "Running cargo clean..."
    cargo clean --target $TARGET_ARM
    cargo clean --target $TARGET_X86
else
    echo "Skipping cargo clean step."
fi

mkdir -p "$DIST_DIR"
rm -f "$BINARY" "$DIST_DIR/onerom.zip"

#
# Build
#

echo "Building One ROM CLI version: $VERSION"

echo "Building for $TARGET_ARM..."
cargo build --release --target $TARGET_ARM

echo "Building for $TARGET_X86..."
cargo build --release --target $TARGET_X86

echo "Creating universal binary..."
lipo -create \
    "../target/$TARGET_ARM/release/onerom" \
    "../target/$TARGET_X86/release/onerom" \
    -output "$BINARY"

#
# Sign and notarise
#

if [ "$SIGN" = true ]; then
    echo "Signing binary..."
    codesign --verify --options runtime --timestamp --sign "$CODESIGN_IDENTITY" "$BINARY"

    echo "Verifying signature..."
    codesign -dvv "$BINARY" 2>&1 | grep "Authority=Developer ID Application"
    if [ $? -ne 0 ]; then
        echo "Error: Binary signature verification failed" >&2
        exit 1
    fi

    echo "Notarizing binary..."
    zip -j "$DIST_DIR/onerom-cli-mac-${VERSION}.zip" "$BINARY"
    xcrun notarytool submit "$DIST_DIR/onerom-cli-mac-${VERSION}.zip" \
        --keychain-profile "onerom-notarization" \
        --wait

    echo "Built and notarized: $DIST_DIR/onerom.zip"
else
    echo "Built without signing: $BINARY"
fi
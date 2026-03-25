#!/usr/bin/env bash
#
# Build all One ROM plugins and stage them for release.
#
# Must be run from the plugins/ directory.
#
# Usage:
#   scripts/build-release-all.sh [--noclean]
#
# Options:
#   --noclean   Skip wiping dist/ and skip make distclean for each plugin
#
# Output:
#   dist/{type}/{name}/plugin.bin

set -euo pipefail

# Constants
DIST_DIR="dist"
META_FILENAME="plugin-meta.json"

# Helpers
error() {
    echo "ERROR: $1" >&2
    exit 1
}

info() {
    echo "$1"
}

# Discover all plugins by finding plugin-meta.json files
# Expected layout: {type}/{name}/plugin-meta.json
discover_plugins() {
    find . -name "$META_FILENAME" \
        -not -path "./dist/*" \
        -not -path "./scripts/*" \
        | sort
}

# Verify we're in the right place
[ -d "scripts" ] || error "Must be run from the plugins/ directory"

# Verify dependencies
command -v python3 >/dev/null 2>&1 || error "python3 is required but not found"

# Parse arguments
NOCLEAN=0

for arg in "$@"; do
    case "$arg" in
        --noclean) NOCLEAN=1 ;;
        *) error "Unknown option: $arg" ;;
    esac
done

# Clean entire dist before building all, unless --noclean
if [ "$NOCLEAN" -eq 0 ]; then
    info "Cleaning $DIST_DIR/..."
    rm -rf "$DIST_DIR"
fi

meta_files=$(discover_plugins)
[ -n "$meta_files" ] || error "No plugin-meta.json files found"

plugin_count=0

while IFS= read -r meta_path; do
    # meta_path is e.g. ./system/usb/plugin-meta.json
    plugin_dir=$(dirname "$meta_path")      # ./system/usb
    type_dir=$(dirname "$plugin_dir")       # ./system
    plugin_type=$(basename "$type_dir")     # system
    plugin_name=$(basename "$plugin_dir")   # usb

    if [ "$NOCLEAN" -eq 1 ]; then
        scripts/build-release.sh --noclean "$plugin_type/$plugin_name"
    else
        scripts/build-release.sh "$plugin_type/$plugin_name"
    fi
    plugin_count=$((plugin_count + 1))
done <<< "$meta_files"

info ""
info "Built $plugin_count plugin(s). Output in $DIST_DIR/"
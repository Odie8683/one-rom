#!/usr/bin/env bash
#
# Build a single One ROM plugin and stage it for release.
#
# Must be run from the plugins/ directory.
#
# Usage:
#   scripts/build-release.sh [--noclean] <type>/<n>
#
# Options:
#   --noclean   Skip make distclean and skip wiping dist/{type}/{name}/
#
# Example:
#   scripts/build-release.sh system/usb
#   scripts/build-release.sh --noclean system/usb
#
# Output:
#   dist/{type}/{name}/plugin.bin

set -euo pipefail

# Constants
DIST_DIR="dist"
META_FILENAME="plugin-meta.json"
DIST_FILENAME="plugin.bin"

# Helpers
error() {
    echo "ERROR: $1" >&2
    exit 1
}

info() {
    echo "$1"
}

# Parse a single field from a JSON file using Python
json_field() {
    local file=$1
    local field=$2
    python3 -c "
import json, sys
with open('$file') as f:
    d = json.load(f)
if '$field' not in d:
    sys.exit(1)
print(d['$field'])
" 2>/dev/null || error "Field '$field' not found in $file"
}

# Verify we're in the right place
[ -d "scripts" ] || error "Must be run from the plugins/ directory"

# Verify dependencies
command -v python3 >/dev/null 2>&1 || error "python3 is required but not found"

# Parse arguments
NOCLEAN=0
PLUGIN_PATH=""

for arg in "$@"; do
    case "$arg" in
        --noclean) NOCLEAN=1 ;;
        -*) error "Unknown option: $arg" ;;
        *)
            [ -z "$PLUGIN_PATH" ] || error "Usage: scripts/build-release.sh [--noclean] <type>/<n>"
            PLUGIN_PATH="$arg"
            ;;
    esac
done

[ -n "$PLUGIN_PATH" ] || error "Usage: scripts/build-release.sh [--noclean] <type>/<n>"

# Derive type and name from path
plugin_type=$(dirname "$PLUGIN_PATH")
plugin_dir_name=$(basename "$PLUGIN_PATH")

[ -d "$PLUGIN_PATH" ] || error "Plugin directory not found: $PLUGIN_PATH"

meta_path="$PLUGIN_PATH/$META_FILENAME"
[ -f "$meta_path" ] || error "No $META_FILENAME found in $PLUGIN_PATH"

plugin_name=$(json_field "$meta_path" "name")
build_output=$(json_field "$meta_path" "build_output")

# Cross-check name vs directory
[ "$plugin_name" = "$plugin_dir_name" ] || \
    error "Plugin name '$plugin_name' in $meta_path does not match directory '$plugin_dir_name'"

info "Building $plugin_type/$plugin_name..."

# Clean and build
if [ "$NOCLEAN" -eq 0 ]; then
    make -C "$PLUGIN_PATH" distclean || error "distclean failed for $plugin_type/$plugin_name"
fi
make -C "$PLUGIN_PATH" || error "Build failed for $plugin_type/$plugin_name"

# Verify binary exists
bin_path="$PLUGIN_PATH/$build_output"
[ -f "$bin_path" ] || error "Expected binary not found: $bin_path"

# Stage to dist
dest_dir="$DIST_DIR/$plugin_type/$plugin_name"
if [ "$NOCLEAN" -eq 0 ]; then
    rm -rf "$dest_dir"
fi
mkdir -p "$dest_dir"
cp "$bin_path" "$dest_dir/$DIST_FILENAME"
cp "$meta_path" "$dest_dir/$META_FILENAME"

info "  Staged to $dest_dir/$DIST_FILENAME"
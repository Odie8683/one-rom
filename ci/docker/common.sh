#!/bin/bash

CONT_NAME="onerom-build"
REPO="ghcr.io/piersfinlayson"

get_git_hash() {
  git rev-parse --short HEAD 2>/dev/null || echo "unknown"
}

get_build_date() {
  date -u +%Y-%m-%d
}

validate_version() {
  local version=$1
  local allow_dev=$2
  
  if [[ "$version" == "dev" && "$allow_dev" != "true" ]]; then
    echo "Error: 'dev' version not allowed for release builds"
    exit 1
  fi
  
  if [[ -z "$version" ]]; then
    echo "Error: Version cannot be empty"
    exit 1
  fi
}
#!/usr/bin/env bash
set -e

rustup target add thumbv8m.main-none-eabihf
cargo build --no-default-features --features "fire-40-a" --release
cargo build --no-default-features --features "fire-32-a" --release
cargo build --no-default-features --features "fire-28-a" --release
cargo build --no-default-features --features "fire-24-e" --release
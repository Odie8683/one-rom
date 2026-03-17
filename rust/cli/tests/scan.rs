// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

mod common;
use crate::common::{fails, onerom, succeeds};

#[test]
fn scan_help_succeeds() {
    succeeds(onerom().args(["scan", "--help"]));
}

#[test]
fn scan_with_serial_wildcard_succeeds() {
    // No match expected, but shouldn't error
    succeeds(onerom().args(["scan", "--serial", "*"]));
}

#[test]
fn scan_with_valid_vid_pid_succeeds() {
    succeeds(onerom().args(["scan", "--vid-pid", "1234:5678"]));
}

#[test]
fn scan_with_invalid_vid_pid_fails() {
    fails(onerom().args(["scan", "--vid-pid", "notvalid"]));
}

#[test]
fn scan_with_duplicate_vid_pid_fails() {
    fails(onerom().args(["scan", "--vid-pid", "1234:5678", "--vid-pid", "1234:5678"]));
}

#[test]
fn scan_with_unrecognised_flag_succeeds() {
    succeeds(onerom().args(["--unrecognised", "scan"]));
}

#[test]
fn scan_no_devices_output() {
    let out = onerom().args(["scan"]).output().unwrap();
    assert!(out.status.success());
    let stdout = String::from_utf8_lossy(&out.stdout);
    // Handle both no devices (CI) and at least one device (local dev)
    assert!(
        stdout.contains("No matching One ROM devices found") || stdout.contains("connected device"),
        "unexpected output: {stdout}"
    );
}

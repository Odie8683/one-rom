// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

mod common;
use common::{fails, onerom, succeeds};

#[test]
fn help_succeeds() {
    succeeds(onerom().arg("--help"));
}

#[test]
fn version_succeeds() {
    succeeds(onerom().arg("--version"));
}

#[test]
fn no_subcommand_fails() {
    fails(&mut onerom());
}

#[test]
fn invalid_vid_pid_format_fails() {
    fails(onerom().args(["--vid-pid", "notvalid", "scan"]));
}

#[test]
fn duplicate_vid_pid_fails() {
    fails(onerom().args(["--vid-pid", "1234:5678", "--vid-pid", "1234:5678", "scan"]));
}

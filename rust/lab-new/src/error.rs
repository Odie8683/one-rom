// Copyright (c) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT licence

//! One ROM Lab - Error handling

#[allow(dead_code)]
#[derive(Debug, Copy, Clone)]
pub enum Error {
    /// Invalid address
    Address,
    /// Buffer size too small
    Buffer,
}

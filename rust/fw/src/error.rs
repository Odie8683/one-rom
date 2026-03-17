// Copyright (C) 2025 Piers Finlayson <piers@piers.rocks>
//
// MIT License

use reqwest::Error as ReqwestError;
use serde_json::Error as SerdeJsonError;
use zip::result::ZipError;

use onerom_config::Error as ConfigError;
use onerom_gen::Error as GenError;

#[derive(Debug)]
pub enum Error {
    Config {
        details: String,
    },
    Read {
        file: String,
        error: std::io::Error,
    },
    Parse {
        error: GenError,
    },
    Build {
        error: GenError,
    },
    License {
        error: GenError,
    },
    FirmwareVersion {
        error: ConfigError,
    },
    Network {
        url: String,
        error: ReqwestError,
    },
    Http {
        url: String,
        status: u16,
    },
    Json {
        error: SerdeJsonError,
    },
    ReleaseNotFound,
    TooLarge {
        portion: String,
        size: usize,
        max: usize,
    },
    FileWrite {
        file: String,
        error: std::io::Error,
    },
    LicenseNotAccepted,
    Zip {
        file: String,
        error: ZipError,
    },
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Config { details } => write!(f, "Configuration error:\n  {}", details),
            Error::Read { file, error } => write!(f, "Read error for file {}:\n  {}", file, error),
            Error::Parse { error } => write!(f, "Parsing error:\n  {:?}", error),
            Error::Build { error } => write!(f, "Build error:\n  {:?}", error),
            Error::License { error } => write!(f, "License acceptance error:\n  {:?}", error),
            Error::FirmwareVersion { error } => write!(f, "Firmware version error:\n  {:?}", error),
            Error::Network { url, error } => write!(f, "Network error for URL {}:\n  {}", url, error),
            Error::Http { url, status } => write!(f, "HTTP error for URL {}: Status code {}", url, status),
            Error::Json { error } => write!(f, "JSON parsing error:\n  {}", error),
            Error::ReleaseNotFound => write!(f, "Requested firmware release not found"),
            Error::TooLarge { portion, size, max } => {
                write!(f, "{} size {} exceeds maximum of {}", portion, size, max)
            }
            Error::FileWrite { file, error } => write!(f, "File write error for file {}:\n  {}", file, error),
            Error::LicenseNotAccepted => write!(f, "License not accepted by user"),
            Error::Zip { file, error } => write!(f, "Zip extraction error for file {}:\n  {}", file, error),
        }
    }
}

impl Error {
    pub fn config(details: String) -> Self {
        Self::Config { details }
    }
    pub fn read(file: String, error: std::io::Error) -> Self {
        Self::Read { file, error }
    }
    pub fn parse(error: GenError) -> Self {
        Self::Parse { error }
    }
    pub fn build(error: GenError) -> Self {
        Self::Build { error }
    }
    pub fn license(error: GenError) -> Self {
        Self::License { error }
    }
    pub fn firmware_version(error: ConfigError) -> Self {
        Self::FirmwareVersion { error }
    }
    pub fn network(url: String, error: ReqwestError) -> Self {
        Self::Network { url, error }
    }
    pub fn json(error: SerdeJsonError) -> Self {
        Self::Json { error }
    }
    pub fn release_not_found() -> Self {
        Self::ReleaseNotFound
    }
    pub fn too_large(portion: String, size: usize, max: usize) -> Self {
        Self::TooLarge { portion, size, max }
    }
    pub fn write(file: String, error: std::io::Error) -> Self {
        Self::FileWrite { file, error }
    }
    pub fn license_not_accepted() -> Self {
        Self::LicenseNotAccepted
    }
    pub fn zip(file: String, error: ZipError) -> Self {
        Self::Zip { file, error }
    }
}

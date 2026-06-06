// Copyright 2024, The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! This module implements zram setup functionality.
//!
//! The setup implemented in this module assumes that the zram kernel module has been loaded early on init with only 1 zram device (`zram0`).
//!
//! zram kernel documentation https://docs.kernel.org/admin-guide/blockdev/zram.html

#[cfg(test)]
mod tests;

use std::io;

use crate::os::get_page_count;
use crate::os::get_page_size;
use crate::zram::SysfsZramApi;

const MKSWAP_BIN_PATH: &str = "/system/bin/mkswap";
const ZRAM_DEVICE_PATH: &str = "/dev/block/zram0";
const PROC_SWAPS_PATH: &str = "/proc/swaps";

const MAX_ZRAM_PERCENTAGE_ALLOWED: u64 = 500;

/// [SetupApi] is the mockable interface for swap operations.
#[cfg_attr(test, mockall::automock)]
pub trait SetupApi {
    /// Set up zram swap device, returning whether the command succeeded and its output.
    fn mkswap(device_path: &str) -> io::Result<std::process::Output>;
    /// Specify the zram swap device.
    fn swapon(device_path: &std::ffi::CStr) -> io::Result<()>;
    /// Read swaps areas in use.
    fn read_swap_areas() -> io::Result<String>;
}

/// The implementation of [SetupApi].
pub struct SetupApiImpl;

impl SetupApi for SetupApiImpl {
    fn mkswap(device_path: &str) -> io::Result<std::process::Output> {
        std::process::Command::new(MKSWAP_BIN_PATH).arg(device_path).output()
    }

    fn swapon(device_path: &std::ffi::CStr) -> io::Result<()> {
        // SAFETY: device_path is a nul-terminated string.
        let res = unsafe { libc::swapon(device_path.as_ptr(), 0) };
        if res == 0 {
            Ok(())
        } else {
            Err(std::io::Error::last_os_error())
        }
    }

    fn read_swap_areas() -> io::Result<String> {
        std::fs::read_to_string(PROC_SWAPS_PATH)
    }
}

/// Whether or not zram is already set up on the device.
pub fn is_zram_swap_activated<S: SetupApi>() -> io::Result<bool> {
    let swaps = S::read_swap_areas()?;
    // Skip the first line which is header.
    let swap_lines = swaps.lines().skip(1);
    // Swap is turned on if swap file contains entry with zram keyword.
    for line in swap_lines {
        if line.contains("zram") {
            return Ok(true);
        }
    }
    Ok(false)
}

/// Error from [parse_zram_size_spec].
#[derive(Debug, thiserror::Error)]
pub enum ZramSpecError {
    /// Zram size was not specified
    #[error("zram size is not specified")]
    EmptyZramSizeSpec,
    /// Zram size percentage needs to be between 1 and 500%
    #[error(
        "zram size percentage {0} is out of range (expected the between 1 and {})",
        MAX_ZRAM_PERCENTAGE_ALLOWED
    )]
    ZramPercentageOutOfRange(u64),
    /// Parsing zram size error
    #[error("zram size is not an int: {0}")]
    ParseZramSize(#[from] std::num::ParseIntError),
}

/// Parse zram size that can be specified by a percentage or an absolute value.
pub fn parse_zram_size_spec(spec: &str) -> Result<u64, ZramSpecError> {
    parse_size_spec_with_page_info(spec, get_page_size(), get_page_count())
}

fn parse_size_spec_with_page_info(
    spec: &str,
    system_page_size: u64,
    system_page_count: u64,
) -> Result<u64, ZramSpecError> {
    if spec.is_empty() {
        return Err(ZramSpecError::EmptyZramSizeSpec);
    }

    if let Some(percentage_str) = spec.strip_suffix('%') {
        let percentage = percentage_str.parse::<u64>()?;
        if percentage == 0 || percentage > MAX_ZRAM_PERCENTAGE_ALLOWED {
            return Err(ZramSpecError::ZramPercentageOutOfRange(percentage));
        }
        return Ok(system_page_count * percentage / 100 * system_page_size);
    }

    let zram_size = spec.parse::<u64>()?;
    Ok(zram_size)
}

/// Error from [activate].
#[derive(Debug, thiserror::Error)]
pub enum ZramActivationError {
    /// Failed to update zram disk size
    #[error("failed to write zram disk size: {0}")]
    UpdateZramDiskSize(std::io::Error),
    /// Failed to swapon
    #[error("swapon failed: {0}")]
    SwapOn(std::io::Error),
    /// Mkswap command failed
    #[error("failed to execute mkswap: {0}")]
    ExecuteMkSwap(std::io::Error),
    /// Mkswap command failed
    #[error("mkswap failed: {0:?}")]
    MkSwap(std::process::Output),
}

/// Set up a zram device with provided parameters.
pub fn activate_zram<Z: SysfsZramApi, S: SetupApi>(
    zram_size: u64,
) -> Result<(), ZramActivationError> {
    Z::write_disksize(&zram_size.to_string()).map_err(ZramActivationError::UpdateZramDiskSize)?;

    let output = S::mkswap(ZRAM_DEVICE_PATH).map_err(ZramActivationError::ExecuteMkSwap)?;
    if !output.status.success() {
        return Err(ZramActivationError::MkSwap(output));
    }

    let zram_device_path_cstring = std::ffi::CString::new(ZRAM_DEVICE_PATH)
        .expect("device path should have no nul characters");
    S::swapon(&zram_device_path_cstring).map_err(ZramActivationError::SwapOn)?;

    Ok(())
}

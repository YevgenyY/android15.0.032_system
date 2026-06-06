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

//! This module provides os layer utilities.

use std::io;

use nix::unistd::sysconf;
use nix::unistd::SysconfVar;

const MEMINFO_PATH: &str = "/proc/meminfo";

/// [MeminfoApi] is a mockable interface for access to "/proc/meminfo".
#[cfg_attr(test, mockall::automock)]
pub trait MeminfoApi {
    /// read "/proc/meminfo".
    fn read_meminfo() -> io::Result<String>;
}

/// The implementation of [MeminfoApi].
pub struct MeminfoApiImpl;

impl MeminfoApi for MeminfoApiImpl {
    fn read_meminfo() -> io::Result<String> {
        std::fs::read_to_string(MEMINFO_PATH)
    }
}

/// Mutex to synchronize tests using [MeminfoApi].
///
/// mockall for static functions requires synchronization.
///
/// https://docs.rs/mockall/latest/mockall/#static-methods
#[cfg(test)]
pub static MEMINFO_API_MTX: std::sync::Mutex<()> = std::sync::Mutex::new(());

/// Returns the page size of the system.
pub fn get_page_size() -> u64 {
    // SAFETY: `sysconf` simply returns an integer.
    unsafe { libc::sysconf(libc::_SC_PAGESIZE) as u64 }
}

/// Returns the page count of the system.
pub fn get_page_count() -> u64 {
    sysconf(SysconfVar::_PHYS_PAGES)
        .expect("PHYS_PAGES should be a valid sysconf variable")
        .expect("PHYS_PAGES variable should be supported") as u64
}

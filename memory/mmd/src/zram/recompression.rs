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

//! This module provides policy to manage zram recompression feature.
//!
//! See this kernel document for the details of recompression.
//!
//! https://docs.kernel.org/admin-guide/blockdev/zram.html#recompression

#[cfg(test)]
mod tests;

use std::time::Duration;
use std::time::Instant;

use crate::os::MeminfoApi;
use crate::zram::idle::calculate_idle_time;
use crate::zram::idle::set_zram_idle_time;
use crate::zram::SysfsZramApi;

/// Error from [ZramRecompression].
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// recompress too frequently
    #[error("recompress too frequently")]
    BackoffTime,
    /// failure on setting zram idle
    #[error("calculate zram idle {0}")]
    CalculateIdle(#[from] crate::zram::idle::CalculateError),
    /// failure on setting zram idle
    #[error("set zram idle {0}")]
    MarkIdle(std::io::Error),
    /// failure on writing to /sys/block/zram0/recompress
    #[error("recompress: {0}")]
    Recompress(std::io::Error),
}

type Result<T> = std::result::Result<T, Error>;

/// Check whether zram recompression is activated by checking "/sys/block/zram0/recomp_algorithm".
pub fn is_zram_recompression_activated<Z: SysfsZramApi>() -> std::io::Result<bool> {
    match Z::read_recomp_algorithm() {
        Ok(recomp_algorithm) => Ok(!recomp_algorithm.is_empty()),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(false),
        Err(e) => Err(e),
    }
}

/// The parameters for zram recompression.
pub struct Params {
    /// The backoff time since last recompression.
    pub backoff_duration: Duration,
    /// The minimum idle duration to recompression. This is used for [calculate_idle_time].
    pub min_idle: Duration,
    /// The maximum idle duration to recompression. This is used for [calculate_idle_time].
    pub max_idle: Duration,
    /// Whether recompress huge and idle pages or not.
    pub huge_idle: bool,
    /// Whether recompress idle pages or not.
    pub idle: bool,
    /// Whether recompress huge pages or not.
    pub huge: bool,
    /// Maximum size in MiB to recompress.
    pub max_mib: u64,
}

impl Default for Params {
    fn default() -> Self {
        Self {
            // 30 minutes
            backoff_duration: Duration::from_secs(1800),
            // 2 hours
            min_idle: Duration::from_secs(2 * 3600),
            // 4 hours
            max_idle: Duration::from_secs(4 * 3600),
            huge_idle: true,
            idle: true,
            huge: true,
            // 1 GiB
            max_mib: 1024,
        }
    }
}

enum Mode {
    HugeIdle,
    Idle,
    Huge,
}

/// [ZramRecompression] manages zram recompression policies.
pub struct ZramRecompression {
    last_recompress_at: Option<Instant>,
}

impl ZramRecompression {
    /// Creates a new [ZramRecompression].
    pub fn new() -> Self {
        Self { last_recompress_at: None }
    }

    /// Recompress idle or huge zram pages.
    pub fn mark_and_recompress<Z: SysfsZramApi, M: MeminfoApi>(
        &mut self,
        params: &Params,
        now: Instant,
    ) -> Result<()> {
        if let Some(last_at) = self.last_recompress_at {
            if now - last_at < params.backoff_duration {
                return Err(Error::BackoffTime);
            }
        }

        if params.huge_idle {
            self.initiate_recompress::<Z, M>(params, Mode::HugeIdle, now)?;
        }
        if params.idle {
            self.initiate_recompress::<Z, M>(params, Mode::Idle, now)?;
        }
        if params.huge {
            self.initiate_recompress::<Z, M>(params, Mode::Huge, now)?;
        }

        Ok(())
    }

    fn initiate_recompress<Z: SysfsZramApi, M: MeminfoApi>(
        &mut self,
        params: &Params,
        mode: Mode,
        now: Instant,
    ) -> Result<()> {
        match mode {
            Mode::HugeIdle | Mode::Idle => {
                let idle_age = calculate_idle_time::<M>(params.min_idle, params.max_idle)?;
                // TODO: adjust the idle_age by suspend duration.
                set_zram_idle_time::<Z>(idle_age).map_err(Error::MarkIdle)?;
            }
            Mode::Huge => {}
        }

        let mode = match mode {
            Mode::HugeIdle => "huge_idle",
            Mode::Idle => "idle",
            Mode::Huge => "huge",
        };

        let trigger = if params.max_mib > 0 {
            format!("type={} threshold={}", mode, params.max_mib)
        } else {
            format!("type={mode}")
        };

        Z::recompress(&trigger).map_err(Error::Recompress)?;

        self.last_recompress_at = Some(now);

        Ok(())
    }
}

// This is to suppress clippy::new_without_default.
impl Default for ZramRecompression {
    fn default() -> Self {
        Self::new()
    }
}

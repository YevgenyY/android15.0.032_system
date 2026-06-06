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

use std::sync::Mutex;
use std::time::Instant;

use anyhow::Context;
use binder::Interface;
use binder::Result as BinderResult;
use log::error;
use mmd_aidl_interface::aidl::android::os::IMmd::IMmd;

use mmd::os::MeminfoApiImpl;
use mmd::zram::recompression::Error as ZramRecompressionError;
use mmd::zram::recompression::ZramRecompression;
use mmd::zram::writeback::Error as ZramWritebackError;
use mmd::zram::writeback::ZramWriteback;
use mmd::zram::SysfsZramApiImpl;

use crate::properties::BoolProp;
use crate::properties::SecondsProp;
use crate::properties::U64Prop;

struct ZramContext {
    zram_writeback: Option<ZramWriteback>,
    zram_recompression: Option<ZramRecompression>,
}

pub struct MmdService {
    ctx: Mutex<ZramContext>,
}

impl MmdService {
    pub fn new(
        zram_writeback: Option<ZramWriteback>,
        zram_recompression: Option<ZramRecompression>,
    ) -> Self {
        Self { ctx: Mutex::new(ZramContext { zram_writeback, zram_recompression }) }
    }
}

impl Interface for MmdService {}

impl IMmd for MmdService {
    fn doZramMaintenance(&self) -> BinderResult<()> {
        let mut ctx = self.ctx.lock().expect("mmd aborts on panics");

        // Execute recompression before writeback.
        if let Some(zram_recompression) = ctx.zram_recompression.as_mut() {
            let params = load_zram_recompression_params();
            match zram_recompression
                .mark_and_recompress::<SysfsZramApiImpl, MeminfoApiImpl>(&params, Instant::now())
            {
                Ok(_) | Err(ZramRecompressionError::BackoffTime) => {}
                Err(e) => error!("failed to zram recompress: {e:?}"),
            }
        }

        if let Some(zram_writeback) = ctx.zram_writeback.as_mut() {
            let params = load_zram_writeback_params();
            let stats = match load_zram_writeback_stats() {
                Ok(v) => v,
                Err(e) => {
                    error!("failed to load zram writeback stats: {e:?}");
                    return Ok(());
                }
            };
            match zram_writeback.mark_and_flush_pages::<SysfsZramApiImpl, MeminfoApiImpl>(
                &params,
                &stats,
                Instant::now(),
            ) {
                Ok(_) | Err(ZramWritebackError::BackoffTime) | Err(ZramWritebackError::Limit) => {}
                Err(e) => error!("failed to zram writeback: {e:?}"),
            }
        }

        Ok(())
    }
}

fn load_zram_writeback_params() -> mmd::zram::writeback::Params {
    let mut params = mmd::zram::writeback::Params::default();
    params.backoff_duration = SecondsProp::ZramWritebackBackoff.get(params.backoff_duration);
    params.min_idle = SecondsProp::ZramWritebackMinIdle.get(params.min_idle);
    params.max_idle = SecondsProp::ZramWritebackMaxIdle.get(params.max_idle);
    params.huge_idle = BoolProp::ZramWritebackHugeIdleEnabled.get(params.huge_idle);
    params.idle = BoolProp::ZramWritebackIdleEnabled.get(params.idle);
    params.huge = BoolProp::ZramWritebackHugeEnabled.get(params.huge);
    params.min_bytes = U64Prop::ZramWritebackMinBytes.get(params.min_bytes);
    params.max_bytes = U64Prop::ZramWritebackMaxBytes.get(params.max_bytes);
    params.max_bytes_per_day = U64Prop::ZramWritebackMaxBytesPerDay.get(params.max_bytes_per_day);
    params
}

fn load_zram_writeback_stats() -> anyhow::Result<mmd::zram::writeback::Stats> {
    let mm_stat =
        mmd::zram::stats::ZramMmStat::load::<SysfsZramApiImpl>().context("load mm_stat")?;
    let bd_stat =
        mmd::zram::stats::ZramBdStat::load::<SysfsZramApiImpl>().context("load bd_stat")?;
    Ok(mmd::zram::writeback::Stats {
        orig_data_size: mm_stat.orig_data_size,
        current_writeback_pages: bd_stat.bd_count_pages,
    })
}

fn load_zram_recompression_params() -> mmd::zram::recompression::Params {
    let mut params = mmd::zram::recompression::Params::default();
    params.backoff_duration = SecondsProp::ZramRecompressionBackoff.get(params.backoff_duration);
    params.min_idle = SecondsProp::ZramRecompressionMinIdle.get(params.min_idle);
    params.max_idle = SecondsProp::ZramRecompressionMaxIdle.get(params.max_idle);
    params.huge_idle = BoolProp::ZramRecompressionHugeIdleEnabled.get(params.huge_idle);
    params.idle = BoolProp::ZramRecompressionIdleEnabled.get(params.idle);
    params.huge = BoolProp::ZramRecompressionHugeEnabled.get(params.huge);
    params.max_mib = U64Prop::ZramRecompressionThresholdMib.get(params.max_mib);
    params
}

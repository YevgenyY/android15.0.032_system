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

//! mmd is a native daemon managing memory task:
//!
//! * zram

mod properties;
mod service;

use binder::BinderFeatures;
use log::error;
use log::info;
use log::warn;
use log::LevelFilter;
use mmd::zram::recompression::is_zram_recompression_activated;
use mmd::zram::recompression::ZramRecompression;
use mmd::zram::setup::activate_zram;
use mmd::zram::setup::is_zram_swap_activated;
use mmd::zram::setup::parse_zram_size_spec;
use mmd::zram::setup::SetupApiImpl;
use mmd::zram::stats::load_total_zram_size;
use mmd::zram::writeback::is_zram_writeback_activated;
use mmd::zram::writeback::ZramWriteback;
use mmd::zram::SysfsZramApiImpl;
use mmd_aidl_interface::aidl::android::os::IMmd::BnMmd;
use rustutils::system_properties;

use crate::properties::BoolProp;
use crate::properties::StringProp;

// In Android zram writeback file is always "/data/per_boot/zram_swap".
const ZRAM_WRITEBACK_FILE_PATH: &str = "/data/per_boot/zram_swap";

fn setup_zram() -> anyhow::Result<()> {
    let zram_activated = is_zram_swap_activated::<SetupApiImpl>()?;
    if zram_activated {
        info!("zram is already on, skipping zram setup");
        return Ok(());
    }

    let zram_size_spec = StringProp::ZramSize.get("50%");
    let zram_size = parse_zram_size_spec(&zram_size_spec)?;
    activate_zram::<SysfsZramApiImpl, SetupApiImpl>(zram_size)?;
    Ok(())
}

fn main() {
    // "mmd --set-property" command copies the AConfig flag to "mmd.enabled_aconfig" system
    // property as either "true" or "false".
    // This is the workaround for init language which does not support AConfig integration.
    // TODO: b/380365026 - Remove "--set-property" command when init language supports AConfig
    // integration.
    if std::env::args().nth(1).map(|s| &s == "--set-property").unwrap_or(false) {
        let value = if mmd_flags::mmd_enabled() { "true" } else { "false" };
        system_properties::write("mmd.enabled_aconfig", value).expect("set system property");
        return;
    }

    let _init_success = logger::init(
        logger::Config::default().with_tag_on_device("mmd").with_max_level(LevelFilter::Trace),
    );

    if !mmd_flags::mmd_enabled() {
        // It is mmd.rc responsibility to start mmd process only if AConfig flag is enabled.
        // This is a safe guard to ensure that mmd runs only when AConfig flag is enabled.
        warn!("mmd is disabled");
        return;
    }

    if BoolProp::ZramEnabled.get(false) {
        setup_zram().expect("zram setup");
    }

    let total_zram_size = match load_total_zram_size::<SysfsZramApiImpl>() {
        Ok(v) => v,
        Err(e) => {
            error!("failed to load total zram size: {e:?}");
            std::process::exit(1);
        }
    };
    let zram_writeback = if BoolProp::ZramWritebackEnabled.get(true) {
        match load_zram_writeback_disk_size() {
            Ok(Some(zram_writeback_disk_size)) => {
                info!("zram writeback is activated");
                Some(ZramWriteback::new(total_zram_size, zram_writeback_disk_size))
            }
            Ok(None) => {
                info!("zram writeback is not activated");
                None
            }
            Err(e) => {
                error!("failed to load zram writeback file size: {e:?}");
                None
            }
        }
    } else {
        info!("zram writeback is disabled");
        None
    };

    let zram_recompression = if BoolProp::ZramRecompressionEnabled.get(true) {
        match is_zram_recompression_activated::<SysfsZramApiImpl>() {
            Ok(is_activated) => {
                if is_activated {
                    info!("zram recompression is activated");
                    Some(ZramRecompression::new())
                } else {
                    info!("zram recompression is not activated");
                    None
                }
            }
            Err(e) => {
                error!("failed to check zram recompression is activated: {e:?}");
                None
            }
        }
    } else {
        info!("zram recompression is disabled");
        None
    };

    let mmd_service = service::MmdService::new(zram_writeback, zram_recompression);
    let mmd_service_binder = BnMmd::new_binder(mmd_service, BinderFeatures::default());
    binder::add_service("mmd", mmd_service_binder.as_binder()).expect("register service");

    info!("mmd started");

    binder::ProcessState::join_thread_pool();
}

/// Loads the zram writeback disk size.
///
/// If zram writeback is not enabled, this returns `Ok(None)`.
pub fn load_zram_writeback_disk_size() -> std::io::Result<Option<u64>> {
    if is_zram_writeback_activated::<SysfsZramApiImpl>()? {
        Ok(Some(std::fs::metadata(ZRAM_WRITEBACK_FILE_PATH)?.len()))
    } else {
        Ok(None)
    }
}

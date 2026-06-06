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

use super::*;
use mockall::predicate::*;
use mockall::Sequence;
use std::os::unix::process::ExitStatusExt;
use std::sync::LockResult;
use std::sync::Mutex;
use std::sync::MutexGuard;

use crate::zram::MockSysfsZramApi;
use crate::zram::ZRAM_API_MTX;

const PROC_SWAP_HEADER: &str = "Filename                                Type            Size            Used            Priority\n";
const DEFAULT_PAGE_SIZE: u64 = 4096;
const DEFAULT_PAGE_COUNT: u64 = 998875;
const DEFAULT_ZRAM_SIZE: u64 = 1000000;

fn success_command_output() -> std::process::Output {
    std::process::Output {
        status: std::process::ExitStatus::from_raw(0),
        stderr: "".to_owned().into_bytes(),
        stdout: "".to_owned().into_bytes(),
    }
}

fn failure_command_output() -> std::process::Output {
    std::process::Output {
        status: std::process::ExitStatus::from_raw(1),
        stderr: "".to_owned().into_bytes(),
        stdout: "".to_owned().into_bytes(),
    }
}

/// Mutex to synchronize tests using [MockSetupApi].
///
/// mockall for static functions requires synchronization.
///
/// https://docs.rs/mockall/latest/mockall/#static-methods
pub static SETUP_API_MTX: Mutex<()> = Mutex::new(());

struct MockContext<'a> {
    write_disksize: crate::zram::__mock_MockSysfsZramApi_SysfsZramApi::__write_disksize::Context,
    read_swap_areas: crate::zram::setup::__mock_MockSetupApi_SetupApi::__read_swap_areas::Context,
    mkswap: crate::zram::setup::__mock_MockSetupApi_SetupApi::__mkswap::Context,
    swapon: crate::zram::setup::__mock_MockSetupApi_SetupApi::__swapon::Context,
    // Lock will be released after mock contexts are dropped.
    _setup_lock: LockResult<MutexGuard<'a, ()>>,
    _zram_lock: LockResult<MutexGuard<'a, ()>>,
}

impl<'a> MockContext<'a> {
    fn new() -> Self {
        let _zram_lock = ZRAM_API_MTX.lock();
        let _setup_lock = SETUP_API_MTX.lock();
        Self {
            write_disksize: MockSysfsZramApi::write_disksize_context(),
            read_swap_areas: MockSetupApi::read_swap_areas_context(),
            mkswap: MockSetupApi::mkswap_context(),
            swapon: MockSetupApi::swapon_context(),
            _setup_lock,
            _zram_lock,
        }
    }
}

#[test]
fn is_zram_swap_activated_zram_off() {
    let mock = MockContext::new();
    mock.read_swap_areas.expect().returning(|| Ok(PROC_SWAP_HEADER.to_string()));

    assert!(!is_zram_swap_activated::<MockSetupApi>().unwrap());
}

#[test]
fn is_zram_swap_activated_zram_on() {
    let mock = MockContext::new();
    let zram_area = "/dev/block/zram0                        partition       7990996         87040           -2\n";
    mock.read_swap_areas.expect().returning(|| Ok(PROC_SWAP_HEADER.to_owned() + zram_area));

    assert!(is_zram_swap_activated::<MockSetupApi>().unwrap());
}

#[test]
fn parse_zram_spec_invalid() {
    assert!(parse_size_spec_with_page_info("", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).is_err());
    assert!(
        parse_size_spec_with_page_info("not_int%", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).is_err()
    );
    assert!(
        parse_size_spec_with_page_info("not_int", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).is_err()
    );
}

#[test]
fn parse_zram_spec_percentage_out_of_range() {
    assert!(parse_size_spec_with_page_info("0%", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).is_err());
    assert!(parse_size_spec_with_page_info("501%", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).is_err());
}

#[test]
fn parse_zram_spec_percentage() {
    assert_eq!(parse_size_spec_with_page_info("33%", 4096, 5).unwrap(), 4096);
    assert_eq!(parse_size_spec_with_page_info("50%", 4096, 5).unwrap(), 8192);
    assert_eq!(parse_size_spec_with_page_info("100%", 4096, 5).unwrap(), 20480);
    assert_eq!(parse_size_spec_with_page_info("200%", 4096, 5).unwrap(), 40960);
    assert_eq!(parse_size_spec_with_page_info("100%", 4096, 3995500).unwrap(), 16365568000);
}

#[test]
fn parse_zram_spec_bytes() {
    assert_eq!(
        parse_size_spec_with_page_info("1234567", DEFAULT_PAGE_SIZE, DEFAULT_PAGE_COUNT).unwrap(),
        1234567
    );
}

#[test]
fn activate_success() {
    let mock = MockContext::new();
    let zram_size = 123456;
    let mut seq = Sequence::new();
    mock.write_disksize
        .expect()
        .with(eq("123456"))
        .times(1)
        .returning(|_| Ok(()))
        .in_sequence(&mut seq);
    mock.mkswap
        .expect()
        .with(eq(ZRAM_DEVICE_PATH))
        .times(1)
        .returning(|_| Ok(success_command_output()))
        .in_sequence(&mut seq);
    mock.swapon
        .expect()
        .with(eq(std::ffi::CString::new(ZRAM_DEVICE_PATH).unwrap()))
        .times(1)
        .returning(|_| Ok(()))
        .in_sequence(&mut seq);

    assert!(activate_zram::<MockSysfsZramApi, MockSetupApi>(zram_size).is_ok());
}

#[test]
fn activate_failed_update_size() {
    let mock = MockContext::new();
    mock.write_disksize.expect().returning(|_| Err(std::io::Error::other("error")));

    assert!(matches!(
        activate_zram::<MockSysfsZramApi, MockSetupApi>(DEFAULT_ZRAM_SIZE),
        Err(ZramActivationError::UpdateZramDiskSize(_))
    ));
}

#[test]
fn activate_failed_mkswap() {
    let mock = MockContext::new();
    mock.write_disksize.expect().returning(|_| Ok(()));
    mock.mkswap.expect().returning(|_| Ok(failure_command_output()));

    assert!(matches!(
        activate_zram::<MockSysfsZramApi, MockSetupApi>(DEFAULT_ZRAM_SIZE),
        Err(ZramActivationError::MkSwap(_))
    ));
}

#[test]
fn activate_failed_swapon() {
    let mock = MockContext::new();
    mock.write_disksize.expect().returning(|_| Ok(()));
    mock.mkswap.expect().returning(|_| Ok(success_command_output()));
    mock.swapon.expect().returning(|_| Err(std::io::Error::other("error")));

    assert!(matches!(
        activate_zram::<MockSysfsZramApi, MockSetupApi>(DEFAULT_ZRAM_SIZE),
        Err(ZramActivationError::SwapOn(_))
    ));
}

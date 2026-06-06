//
// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include "nfa_ce_main.cc"

typedef unsigned int NFA_HANDLE;

class MockNfcOperations {
public:
    MOCK_METHOD(void, nfa_ce_restart_listen_check, (), ());
    MOCK_METHOD(void, nfa_dm_delete_rf_discover, (NFA_HANDLE), ());
};

// Tests for nfa_ce_evt_2_str
TEST(NfaCeEvtTest, EventToString) {
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_API_CFG_LOCAL_TAG_EVT), "NFA_CE_API_CFG_LOCAL_TAG_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_API_REG_LISTEN_EVT), "NFA_CE_API_REG_LISTEN_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_API_DEREG_LISTEN_EVT), "NFA_CE_API_DEREG_LISTEN_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_API_CFG_ISODEP_TECH_EVT), "NFA_CE_API_CFG_ISODEP_TECH_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_ACTIVATE_NTF_EVT), "NFA_CE_ACTIVATE_NTF_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(NFA_CE_DEACTIVATE_NTF_EVT), "NFA_CE_DEACTIVATE_NTF_EVT");
    EXPECT_EQ(nfa_ce_evt_2_str(0x9999), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0x0000), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0xFFFF), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0x0100), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0x01FF), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0x1000), "Unknown");
    EXPECT_EQ(nfa_ce_evt_2_str(0x2000), "Unknown");

}

// Tests for nfa_ce_proc_nfcc_power_mode
TEST(NfaCeProcNfccPowerModeTest, ProcessPowerMode) {
    MockNfcOperations mock_ops;
    nfa_ce_proc_nfcc_power_mode(NFA_DM_PWR_MODE_FULL);
    nfa_ce_proc_nfcc_power_mode(0);
    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(0xFF);
}

TEST(NfaCeProcNfccPowerModeTest, EdgeCases) {
    MockNfcOperations mock_ops;
    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(NFA_DM_PWR_MODE_FULL);
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    memset(p_cb->listen_info, 0, sizeof(p_cb->listen_info));
    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(0);
    memset(p_cb->listen_info, 0xFF, sizeof(p_cb->listen_info));
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, NoListenInfo) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    memset(p_cb->listen_info, 0, sizeof(p_cb->listen_info));
    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, SingleActiveListenEntry) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    p_cb->listen_info[0].flags |= NFA_CE_LISTEN_INFO_IN_USE;
    p_cb->listen_info[0].rf_disc_handle = 1;
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, TwoActiveListenEntries) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    p_cb->listen_info[0].flags |= NFA_CE_LISTEN_INFO_IN_USE;
    p_cb->listen_info[0].rf_disc_handle = 1;
    p_cb->listen_info[1].flags |= NFA_CE_LISTEN_INFO_IN_USE;
    p_cb->listen_info[1].rf_disc_handle = 2;
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, NoActiveListenEntries) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    memset(p_cb->listen_info, 0, sizeof(p_cb->listen_info));
    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, SingleDeactivatedListenEntry) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    memset(p_cb->listen_info, 0, sizeof(p_cb->listen_info));
    p_cb->listen_info[0].flags &= ~NFA_CE_LISTEN_INFO_IN_USE;

    EXPECT_CALL(mock_ops, nfa_dm_delete_rf_discover(::testing::_)).Times(0);
    nfa_ce_proc_nfcc_power_mode(0);
}

TEST(NfaCeProcNfccPowerModeTest, MixedActiveAndInactiveListenEntries) {
    MockNfcOperations mock_ops;
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    p_cb->listen_info[0].flags |= NFA_CE_LISTEN_INFO_IN_USE;
    p_cb->listen_info[0].rf_disc_handle = 1;
    p_cb->listen_info[1].flags &= ~NFA_CE_LISTEN_INFO_IN_USE;

    nfa_ce_proc_nfcc_power_mode(0);
}
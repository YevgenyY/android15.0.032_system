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
#include <cstring>
#include "nfa_ce_act.cc"
#include "nfa_api.h"
#include "nfa_ce_int.h"
#include "nfa_mem_co.h"
#include "android-base/logging.h"
#include "android-base/stringprintf.h"

extern "C" tNFA_STATUS (*nfa_ce_start_listening_ptr)();
typedef unsigned int NFA_HANDLE;

class MockCallback {
public:
    MOCK_METHOD(void, HandleEvent, (tNFA_EE_EVT event, tNFA_CONN_EVT_DATA* p_data), ());
    MOCK_METHOD(tNFA_STATUS, CE_T3tSetLocalNDEFMsg,(bool read_only, uint16_t cur_size,
            uint16_t max_size,uint8_t* p_data, uint8_t* p_uid));
};

class MockNfaDm {
public:
    MOCK_METHOD(void, nfa_dm_rf_deactivate, (tNFA_DEACTIVATE_TYPE deactivate_type), ());
    MOCK_METHOD(void, nfa_dm_delete_rf_discover, (NFA_HANDLE), ());
    MOCK_METHOD(void, nfa_dm_conn_cback_event_notify, (tNFA_EE_EVT event,
            tNFA_CONN_EVT_DATA* p_data), ());
};

class MockNfcUtils {
public:
    MOCK_METHOD(uint8_t, NFC_GetNCIVersion, (), ());
    MOCK_METHOD(void, nfa_dm_check_set_config, (uint8_t, uint8_t*, bool), ());
};

class MockMemoryAlloc {
public:
    MOCK_METHOD(void*, nfa_mem_co_alloc, (uint32_t num_bytes), ());
    MOCK_METHOD(void, nfa_ce_free_scratch_buf, (), ());
};

class NfaCeHandleEvtTest : public ::testing::Test {
protected:
    static MockCallback mock_callback;
    static void CallbackFunction(tNFA_EE_EVT event, tNFA_CONN_EVT_DATA* p_data) {
        mock_callback.HandleEvent(event, p_data);
    }
    void SetUp() override {
        tNFA_CE_CB mock_ce_cb;
        mock_ce_cb.p_active_conn_cback = CallbackFunction;
        mock_ce_cb.listen_info[0].p_conn_cback = CallbackFunction;
        mock_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_T4T_ACTIVATE_PND;
        mock_ce_cb.idx_cur_active = 0;
        nfa_ce_cb = mock_ce_cb;
    }
    void TearDown() override {
        testing::Mock::VerifyAndClearExpectations(&mock_callback);
        testing::Mock::AllowLeak(reinterpret_cast<const void*>(&mock_callback));
    }
};

MockCallback NfaCeHandleEvtTest::mock_callback;

// tests for NfaCeHandle_T3t

TEST_F(NfaCeHandleEvtTest, CallbackInvoked) {
    tNFA_CE_CB mock_ce_cb;
    mock_ce_cb.p_active_conn_cback = CallbackFunction;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_START_EVT, testing::_)).Times(1);
    mock_ce_cb.p_active_conn_cback(NFA_CE_NDEF_WRITE_START_EVT, nullptr);
}

TEST_F(NfaCeHandleEvtTest, HandleNdefUpdateStartEvt) {
    tNFA_CE_CB mock_ce_cb;
    mock_ce_cb.p_active_conn_cback = CallbackFunction;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_START_EVT, testing::_)).Times(1);
    tNFA_CONN_EVT_DATA mock_data;
    mock_data.ndef_write_cplt.p_data = nullptr;
    mock_ce_cb.p_active_conn_cback(NFA_CE_NDEF_WRITE_START_EVT, &mock_data);
}

TEST_F(NfaCeHandleEvtTest, HandleNdefUpdateCompleteEvt) {
    tNFA_CE_CB mock_ce_cb;
    mock_ce_cb.p_active_conn_cback = CallbackFunction;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_CPLT_EVT, testing::_)).Times(1);
    tNFA_CONN_EVT_DATA mock_data;
    mock_data.ndef_write_cplt.p_data = nullptr;
    mock_ce_cb.p_active_conn_cback(NFA_CE_NDEF_WRITE_CPLT_EVT, &mock_data);
}

TEST_F(NfaCeHandleEvtTest, HandleRawFrameEvt) {
    tNFA_CE_CB mock_ce_cb;
    mock_ce_cb.p_active_conn_cback = CallbackFunction;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_DATA_EVT, testing::_)).Times(1);
    tNFA_CONN_EVT_DATA mock_data;
    mock_data.ce_data.p_data = nullptr;
    mock_ce_cb.p_active_conn_cback(NFA_CE_DATA_EVT, &mock_data);
}

// tests for NfaCeHandle_T4t

TEST_F(NfaCeHandleEvtTest, HandleT4tNdefUpdateStartEvt) {
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_START_EVT, testing::_)).Times(1);
    tCE_EVENT event = CE_T4T_NDEF_UPDATE_START_EVT;
    tCE_DATA ce_data;
    nfa_ce_handle_t4t_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleT4tNdefUpdateCpltEvt) {
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_CPLT_EVT, testing::_)).Times(1);
    tCE_EVENT event = CE_T4T_NDEF_UPDATE_CPLT_EVT;
    tCE_DATA ce_data;
    ce_data.update_info.length = 5;
    uint8_t mock_data[5] = {1, 2, 3, 4, 5};
    ce_data.update_info.p_data = mock_data;
    nfa_ce_handle_t4t_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleT4tNdefUpdateCpltEvtFailure) {
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_CPLT_EVT, testing::_)).Times(1);
    tCE_EVENT event = CE_T4T_NDEF_UPDATE_CPLT_EVT;
    tCE_DATA ce_data;
    ce_data.update_info.length = 5;
    uint8_t invalid_data[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ce_data.update_info.p_data = invalid_data;
    nfa_ce_handle_t4t_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleT4tNdefUpdateAbortEvt) {
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(NFA_CE_NDEF_WRITE_CPLT_EVT, testing::_)).Times(1);
    tCE_EVENT event = CE_T4T_NDEF_UPDATE_ABORT_EVT;
    tCE_DATA ce_data;
    ce_data.update_info.length = 0;
    ce_data.update_info.p_data = nullptr;nfa_ce_handle_t4t_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleT4tUnhandledEvt) {
    tNFA_CE_CB* p_cb = &nfa_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    tCE_EVENT event = static_cast<tCE_EVENT>(0xFF);
    tCE_DATA ce_data;
    nfa_ce_handle_t4t_evt(event, &ce_data);
}

//tests for nfa_ce_handle_t4t_aid

TEST_F(NfaCeHandleEvtTest, HandleValidAidEvent) {
    tCE_EVENT event = CE_T4T_RAW_FRAME_EVT;
    tCE_DATA ce_data;
    ce_data.raw_frame.aid_handle = 0x34;
    nfa_ce_handle_t4t_aid_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleInvalidEventType) {
    tNFA_CE_CB mock_ce_cb;
    tCE_EVENT event = CE_T4T_RAW_FRAME_EVT + 1;
    tCE_DATA ce_data;
    mock_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE | NFA_CE_LISTEN_INFO_T4T_AID;
    mock_ce_cb.listen_info[0].t4t_aid_handle = ce_data.raw_frame.aid_handle;
    mock_ce_cb.listen_info[0].p_conn_cback = CallbackFunction;
    nfa_ce_cb = mock_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    nfa_ce_handle_t4t_aid_evt(event, &ce_data);
}

TEST_F(NfaCeHandleEvtTest, HandleEventWithActivatePendingFlag) {
    tCE_EVENT event = CE_T4T_RAW_FRAME_EVT;
    tCE_DATA ce_data;
    ce_data.raw_frame.aid_handle = 0x34;
    nfa_ce_handle_t4t_aid_evt(event, &ce_data);
    LOG(INFO) << "Test complete: Callback should have been invoked.";
}

TEST_F(NfaCeHandleEvtTest, HandleInvalidListenInfo) {
    tNFA_CE_CB mock_ce_cb;
    tCE_EVENT event = CE_T4T_RAW_FRAME_EVT;
    tCE_DATA ce_data;
    ce_data.raw_frame.aid_handle = 0x34;
    mock_ce_cb.listen_info[0].flags = 0;
    mock_ce_cb.listen_info[0].t4t_aid_handle = ce_data.raw_frame.aid_handle;
    nfa_ce_cb = mock_ce_cb;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    nfa_ce_handle_t4t_aid_evt(event, &ce_data);
}

class NfaCeDiscoveryCbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&nfa_ce_cb, 0, sizeof(nfa_ce_cb));
    }
    void TearDown() override {
    }
    static MockCallback mock_callback;
};

MockCallback NfaCeDiscoveryCbackTest::mock_callback;

TEST_F(NfaCeDiscoveryCbackTest, DiscoveryStartEvent) {
    tNFC_DISCOVER p_data;
    p_data.start = NFC_STATUS_OK;
    nfa_ce_discovery_cback(NFA_DM_RF_DISC_START_EVT, &p_data);
}

TEST_F(NfaCeDiscoveryCbackTest, DiscoveryActivatedEvent) {
    tNFC_DISCOVER p_data;
    p_data.activate = {};
    EXPECT_CALL(mock_callback, HandleEvent(static_cast<unsigned char>(NFA_CE_ACTIVATE_NTF_EVT),
                                           testing::_)).Times(1);
    nfa_ce_discovery_cback(NFA_DM_RF_DISC_ACTIVATED_EVT, &p_data);
}

TEST_F(NfaCeDiscoveryCbackTest, DiscoveryDeactivatedEventActiveListen) {
    tNFC_DISCOVER p_data;
    p_data.deactivate.type = NFC_DEACTIVATE_TYPE_IDLE;
    nfa_ce_cb.flags |= NFA_CE_FLAGS_LISTEN_ACTIVE_SLEEP;
    EXPECT_CALL(mock_callback, HandleEvent(static_cast<unsigned char>(NFA_CE_DEACTIVATE_NTF_EVT),
                                           testing::_)).Times(1);
    nfa_ce_discovery_cback(NFA_DM_RF_DISC_DEACTIVATED_EVT, &p_data);
}

TEST_F(NfaCeDiscoveryCbackTest, DiscoveryDeactivatedEventInactiveListen) {
    tNFC_DISCOVER p_data;
    p_data.deactivate.type = NFC_DEACTIVATE_TYPE_IDLE;
    nfa_ce_cb.flags &= ~NFA_CE_FLAGS_LISTEN_ACTIVE_SLEEP;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    nfa_ce_discovery_cback(NFA_DM_RF_DISC_DEACTIVATED_EVT, &p_data);
}

TEST_F(NfaCeDiscoveryCbackTest, UnexpectedEvent) {
    tNFC_DISCOVER p_data;
    memset(&p_data, 0, sizeof(p_data));
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    nfa_ce_discovery_cback(static_cast<tNFA_DM_RF_DISC_EVT>(0xFF), &p_data);
}

class MockNfaCe {
public:
    MOCK_METHOD(tNFA_STATUS, nfa_ce_start_listening, (), ());
};

class NfaCeRestartListenCheckTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        testing::Mock::AllowLeak(reinterpret_cast<const void*>(&mock_nfa_ce));
    }
    static MockNfaCe mock_nfa_ce;
};

MockNfaCe NfaCeRestartListenCheckTest::mock_nfa_ce;

TEST_F(NfaCeRestartListenCheckTest, ActiveListenInfoEntries) {
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    EXPECT_CALL(mock_nfa_ce, nfa_ce_start_listening()).Times(1).WillOnce(::testing::Return(
            NFA_STATUS_OK));
    bool result = nfa_ce_restart_listen_check();
    EXPECT_TRUE(result);
}

TEST_F(NfaCeRestartListenCheckTest, NoActiveListenInfoEntries) {
    for (int i = 0; i < NFA_CE_LISTEN_INFO_MAX; i++) {
        nfa_ce_cb.listen_info[i].flags = 0;
    }
    bool result = nfa_ce_restart_listen_check();
    EXPECT_FALSE(result);
}

TEST_F(NfaCeRestartListenCheckTest, MultipleActiveListenInfoEntries) {
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[1].flags = NFA_CE_LISTEN_INFO_IN_USE;
    EXPECT_CALL(mock_nfa_ce, nfa_ce_start_listening()).Times(1).WillOnce(::testing::Return(
            NFA_STATUS_OK));
    bool result = nfa_ce_restart_listen_check();
    EXPECT_TRUE(result);
}

TEST_F(NfaCeRestartListenCheckTest, AllListenInfoEntriesInactive) {
    memset(nfa_ce_cb.listen_info, 0, sizeof(nfa_ce_cb.listen_info));
    bool result = nfa_ce_restart_listen_check();
    EXPECT_FALSE(result);
}

TEST_F(NfaCeRestartListenCheckTest, OneActiveListenInfoEntry) {
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[1].flags = 0;
    nfa_ce_cb.listen_info[2].flags = 0;
    EXPECT_CALL(mock_nfa_ce, nfa_ce_start_listening()).Times(1).WillOnce(::testing::Return(
            NFA_STATUS_OK));
    bool result = nfa_ce_restart_listen_check();
    EXPECT_TRUE(result);
}

class NfaCeSetContentTest : public ::testing::Test {
protected:
    static MockCallback mock_callback;

    void SetUp() override {
        memset(&nfa_ce_cb, 0, sizeof(nfa_ce_cb));
    }

    void TearDown() override {
        testing::Mock::VerifyAndClearExpectations(&mock_callback);
        testing::Mock::AllowLeak(reinterpret_cast<const void*>(&mock_callback));
    }
};

MockCallback NfaCeSetContentTest::mock_callback;

TEST_F(NfaCeSetContentTest, NotListeningForNDEF) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = 0;
    EXPECT_EQ(nfa_ce_set_content(), NFA_STATUS_OK);
}

TEST_F(NfaCeSetContentTest, SetNDEFContentType3TAndIsoDepProtocols) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].protocol_mask =
            NFA_PROTOCOL_MASK_T3T | NFA_PROTOCOL_MASK_ISO_DEP;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nfa_ce_set_content(), NFA_STATUS_OK);
}

TEST_F(NfaCeSetContentTest, NoProtocolsUsed) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].protocol_mask = 0;
    EXPECT_EQ(nfa_ce_set_content(), NFA_STATUS_OK);
}

TEST_F(NfaCeSetContentTest, ScratchBufferSuccessNoProtocolMask) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].protocol_mask = 0;
    EXPECT_CALL(mock_callback, HandleEvent(testing::_, testing::_)).Times(0);
    EXPECT_EQ(nfa_ce_set_content(), NFA_STATUS_OK);
}

class NfaCeDisableLocalTagTest : public ::testing::Test {
protected:
    static MockNfaDm mock_nfa_dm;
    void SetUp() override {
        memset(&nfa_ce_cb, 0, sizeof(nfa_ce_cb));
    }
    void TearDown() override {
    }
};

MockNfaDm NfaCeDisableLocalTagTest::mock_nfa_dm;

TEST_F(NfaCeDisableLocalTagTest, DisableNdefTagWithActiveSleep) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.flags = NFA_CE_FLAGS_LISTEN_ACTIVE_SLEEP;
    nfa_ce_cb.idx_cur_active = NFA_CE_LISTEN_INFO_IDX_NDEF;
    EXPECT_CALL(mock_nfa_dm, nfa_dm_rf_deactivate(NFA_DEACTIVATE_TYPE_IDLE)).Times(1);
    nfa_ce_disable_local_tag();
}

TEST_F(NfaCeDisableLocalTagTest, DisableNdefTagWithoutActiveSleep) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.flags = 0;
    nfa_ce_cb.idx_cur_active = NFA_CE_LISTEN_INFO_IDX_NDEF;
    EXPECT_CALL(mock_nfa_dm, nfa_dm_delete_rf_discover(nfa_ce_cb.listen_info[
            NFA_CE_LISTEN_INFO_IDX_NDEF].rf_disc_handle)).Times(1);
    EXPECT_CALL(mock_nfa_dm, nfa_dm_conn_cback_event_notify(
            NFA_CE_LOCAL_TAG_CONFIGURED_EVT, testing::_)).Times(1);
}

TEST_F(NfaCeDisableLocalTagTest, NdefTagInUseWithInvalidRfDiscHandle) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].rf_disc_handle = NFA_HANDLE_INVALID;
    EXPECT_CALL(mock_nfa_dm, nfa_dm_conn_cback_event_notify(
            NFA_CE_LOCAL_TAG_CONFIGURED_EVT, testing::_)).Times(1);
}

TEST_F(NfaCeDisableLocalTagTest, NdefTagNotInUseWithNoRfDiscHandle) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = 0;
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].rf_disc_handle = NFA_HANDLE_INVALID;
    EXPECT_CALL(mock_nfa_dm, nfa_dm_conn_cback_event_notify(
            NFA_CE_LOCAL_TAG_CONFIGURED_EVT, testing::_)).Times(1);
}

MockMemoryAlloc mock_mem_alloc;

class NfaCeReallocScratchBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        nfa_ce_cb.p_scratch_buf = nullptr;
        nfa_ce_cb.scratch_buf_size = 0;
        nfa_ce_cb.ndef_max_size = 128;
        nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = 0;
    }

    void TearDown() override {
    }
};

TEST_F(NfaCeReallocScratchBufferTest, TestAllocateScratchBufferWhenNoneAllocated) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = 0;
    EXPECT_CALL(mock_mem_alloc, nfa_mem_co_alloc(nfa_ce_cb.ndef_max_size)).WillOnce(
            testing::Return(reinterpret_cast<void*>(0x1234)));
    EXPECT_CALL(mock_mem_alloc, nfa_ce_free_scratch_buf()).Times(1);
    tNFA_STATUS result = nfa_ce_realloc_scratch_buffer();
    EXPECT_EQ(result, NFA_STATUS_OK);
    EXPECT_NE(nfa_ce_cb.p_scratch_buf, nullptr);
    EXPECT_EQ(nfa_ce_cb.scratch_buf_size, nfa_ce_cb.ndef_max_size);
}

TEST_F(NfaCeReallocScratchBufferTest, TestNoAllocationWhenBufferSizeMatches) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags = 0;
    nfa_ce_cb.p_scratch_buf = reinterpret_cast<uint8_t*>(0x1234);
    nfa_ce_cb.scratch_buf_size = nfa_ce_cb.ndef_max_size;
    EXPECT_CALL(mock_mem_alloc, nfa_mem_co_alloc(testing::_)).Times(0);
    EXPECT_CALL(mock_mem_alloc, nfa_ce_free_scratch_buf()).Times(0);
    tNFA_STATUS result = nfa_ce_realloc_scratch_buffer();
    EXPECT_EQ(result, NFA_STATUS_OK);
}

TEST_F(NfaCeReallocScratchBufferTest, TestFreeScratchBufferWhenReadOnly) {
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].flags |= NFC_CE_LISTEN_INFO_READONLY_NDEF;
    EXPECT_CALL(mock_mem_alloc, nfa_ce_free_scratch_buf()).Times(1);
    tNFA_STATUS result = nfa_ce_realloc_scratch_buffer();
    EXPECT_EQ(result, NFA_STATUS_OK);
    EXPECT_EQ(nfa_ce_cb.p_scratch_buf, nullptr);
    EXPECT_EQ(nfa_ce_cb.scratch_buf_size, 0);
}

MockNfcUtils mock_nfc_utils;

class NfcCeT3tSetListenParamsTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&nfa_ce_cb, 0, sizeof(nfa_ce_cb));
    }

    void TearDown() override {
    }
};

TEST_F(NfcCeT3tSetListenParamsTest, TestNfcVersionLessThan2_0WithValidListenInfo) {
    EXPECT_CALL(mock_nfc_utils, NFC_GetNCIVersion()).WillOnce(testing::Return(NCI_VERSION_1_0));
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[0].protocol_mask = NFA_PROTOCOL_MASK_T3T;
    nfa_ce_cb.listen_info[0].t3t_system_code = 0x1234;
    nfa_ce_cb.listen_info[0].t3t_nfcid2[0] = 0x01;
    nfa_ce_cb.listen_info[0].t3t_pmm[0] = 0x01;
    EXPECT_CALL(mock_nfc_utils, nfa_dm_check_set_config(
            testing::_ , testing::_ , testing::_)).Times(1);
    nfc_ce_t3t_set_listen_params();
    EXPECT_EQ(nfa_ce_cb.listen_info[0].t3t_system_code, 0x1234);
}

TEST_F(NfcCeT3tSetListenParamsTest, TestNfcVersion2_0WithValidListenInfo) {
    EXPECT_CALL(mock_nfc_utils, NFC_GetNCIVersion()).WillOnce(testing::Return(NCI_VERSION_2_0));
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[0].protocol_mask = NFA_PROTOCOL_MASK_T3T;
    nfa_ce_cb.listen_info[0].t3t_system_code = 0x5678;
    nfa_ce_cb.listen_info[0].t3t_nfcid2[0] = 0x02;
    nfa_ce_cb.listen_info[0].t3t_pmm[0] = 0x02;
    EXPECT_CALL(mock_nfc_utils, nfa_dm_check_set_config(
            testing::_ , testing::_ , testing::_)).Times(1);
    nfc_ce_t3t_set_listen_params();
    EXPECT_EQ(nfa_ce_cb.listen_info[0].t3t_system_code, 0x5678);
}

TEST_F(NfcCeT3tSetListenParamsTest, TestNoListenInfoInUse) {
    for (int i = 0; i < NFA_CE_LISTEN_INFO_MAX; i++) {
        nfa_ce_cb.listen_info[i].flags = 0;
    }
    EXPECT_CALL(mock_nfc_utils, nfa_dm_check_set_config(
            testing::_ , testing::_ , testing::_)).Times(0);
    nfc_ce_t3t_set_listen_params();
}

TEST_F(NfcCeT3tSetListenParamsTest, TestProtocolMaskDoesNotMatch) {
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[0].protocol_mask = 0;
    EXPECT_CALL(mock_nfc_utils, nfa_dm_check_set_config(
            testing::_ , testing::_ , testing::_)).Times(0);
    nfc_ce_t3t_set_listen_params();
}

TEST_F(NfcCeT3tSetListenParamsTest, TestDtaModeFlag) {
    appl_dta_mode_flag = 0x01;
    EXPECT_CALL(mock_nfc_utils, NFC_GetNCIVersion()).WillOnce(testing::Return(NCI_VERSION_1_0));
    nfa_ce_cb.listen_info[0].flags = NFA_CE_LISTEN_INFO_IN_USE;
    nfa_ce_cb.listen_info[0].protocol_mask = NFA_PROTOCOL_MASK_T3T;
    EXPECT_CALL(mock_nfc_utils, nfa_dm_check_set_config(
            testing::_ , testing::_ , testing::_)).Times(1);
    nfc_ce_t3t_set_listen_params();
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nfa_ce_api.h"
#include "nfa_ce_int.h"
#include "nfa_api.h"
#include <android-base/logging.h>
#include "nfa_ce_api.cc"

class MockSystemFunctions {
public:
    MOCK_METHOD(void*, GKI_getbuf, (uint16_t size), ());
    MOCK_METHOD(void, nfa_sys_sendmsg, (tNFA_CE_MSG* p_msg), ());
    MOCK_METHOD(tNFA_STATUS, nfa_ce_api_deregister_listen, (tNFA_HANDLE handle,
            uint16_t listen_type), ());

};

class NfaCeApiTest : public ::testing::Test {
protected:
    MockSystemFunctions mock_sys_funcs;

    tNFA_STATUS (*configure_local_tag)(tNFA_PROTOCOL_MASK protocol_mask,
                                       uint8_t* p_ndef_data,
                                       uint16_t ndef_cur_size,
                                       uint16_t ndef_max_size, bool read_only,
                                       uint8_t uid_len, uint8_t* p_uid);

    void SetUp() override {
        // Pointing to the original function for testing
        configure_local_tag = &NFA_CeConfigureLocalTag;
    }

    void TearDown() override {
        // No cleanup needed for this test case
    }
};

TEST_F(NfaCeApiTest, InvalidProtocolMaskWithNonNullNDEFData) {
    tNFA_PROTOCOL_MASK invalid_protocol_mask = 0xFF;
    uint8_t valid_ndef_data[] = {0x01, 0x02, 0x03};
    uint16_t ndef_cur_size = 3;
    uint16_t ndef_max_size = 1024;
    bool read_only = false;
    uint8_t uid_len = 0;
    uint8_t* p_uid = nullptr;
    tNFA_STATUS status = configure_local_tag(invalid_protocol_mask, valid_ndef_data, ndef_cur_size,
                                             ndef_max_size, read_only, uid_len, p_uid);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, NullNDEFDataWithProtocolMask) {
    tNFA_PROTOCOL_MASK protocol_mask = NFA_PROTOCOL_MASK_ISO_DEP;
    uint8_t* p_ndef_data = nullptr;
    uint16_t ndef_cur_size = 3;
    uint16_t ndef_max_size = 1024;
    bool read_only = false;
    uint8_t uid_len = 0;
    uint8_t* p_uid = nullptr;
    tNFA_STATUS status = configure_local_tag(protocol_mask, p_ndef_data, ndef_cur_size,
                                             ndef_max_size, read_only, uid_len, p_uid);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, InvalidProtocolMaskForType1Type2) {
    tNFA_PROTOCOL_MASK protocol_mask = NFA_PROTOCOL_MASK_T1T;
    uint8_t valid_ndef_data[] = {0x01, 0x02, 0x03};
    uint16_t ndef_cur_size = 3;
    uint16_t ndef_max_size = 1024;
    bool read_only = false;
    uint8_t uid_len = 0;
    uint8_t* p_uid = nullptr;
    tNFA_STATUS status = configure_local_tag(protocol_mask, valid_ndef_data, ndef_cur_size,
                                             ndef_max_size, read_only, uid_len, p_uid);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, NonZeroUIDLengthWithProtocolMask) {
    tNFA_PROTOCOL_MASK protocol_mask = NFA_PROTOCOL_MASK_ISO_DEP;
    uint8_t valid_ndef_data[] = {0x01, 0x02, 0x03};
    uint16_t ndef_cur_size = 3;
    uint16_t ndef_max_size = 1024;
    bool read_only = false;
    uint8_t uid_len = 4;
    uint8_t p_uid[] = {0x01, 0x02, 0x03, 0x04};
    tNFA_STATUS status = configure_local_tag(protocol_mask, valid_ndef_data, ndef_cur_size,
                                             ndef_max_size, read_only, uid_len, p_uid);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, InvalidParamNullCallback) {
    uint8_t aid[NFC_MAX_AID_LEN] = {0x01, 0x02};
    uint8_t aid_len = 2;
    tNFA_CONN_CBACK* p_conn_cback = nullptr;
    tNFA_STATUS status = NFA_CeRegisterAidOnDH(aid, aid_len, p_conn_cback);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, InvalidParamAidLenZero) {
    uint8_t aid[NFC_MAX_AID_LEN] = {0x01, 0x02};
    uint8_t aid_len = 0;
    tNFA_CONN_CBACK* p_conn_cback = reinterpret_cast<tNFA_CONN_CBACK*>(0x1234);
    tNFA_STATUS status = NFA_CeRegisterAidOnDH(aid, aid_len, p_conn_cback);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, InvalidParamNullAid) {
    uint8_t* aid = nullptr;
    uint8_t aid_len = 2;
    tNFA_CONN_CBACK* p_conn_cback = reinterpret_cast<tNFA_CONN_CBACK*>(0x1234);
    tNFA_STATUS status = NFA_CeRegisterAidOnDH(aid, aid_len, p_conn_cback);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, NullFelicaCallback) {
    uint16_t system_code = 0x1234;
    uint8_t nfcid2[NCI_RF_F_UID_LEN] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    uint8_t t3tPmm[NCI_T3T_PMM_LEN] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
    tNFA_STATUS status = NFA_CeRegisterFelicaSystemCodeOnDH(system_code, nfcid2, t3tPmm, nullptr);
    EXPECT_EQ(status, NFA_STATUS_INVALID_PARAM);
}

TEST_F(NfaCeApiTest, DeregisterFelicaSystemCodeOnDH_InvalidListenInfo) {
    tNFA_HANDLE valid_handle = 0x1234;
    uint32_t invalid_listen_info = 0x9999;
    EXPECT_CALL(mock_sys_funcs, nfa_sys_sendmsg(testing::_)).Times(0);
    tNFA_STATUS status = nfa_ce_api_deregister_listen(valid_handle, invalid_listen_info);
    EXPECT_EQ(status, NFA_STATUS_BAD_HANDLE);
}

TEST_F(NfaCeApiTest, DeregisterFelicaSystemCodeOnDH_InvalidHandleAndListenInfo) {
    tNFA_HANDLE invalid_handle = 0x4321;
    uint32_t invalid_listen_info = 0x9999;
    EXPECT_CALL(mock_sys_funcs, nfa_sys_sendmsg(testing::_)).Times(0);
    tNFA_STATUS status = nfa_ce_api_deregister_listen(invalid_handle, invalid_listen_info);
    EXPECT_EQ(status, NFA_STATUS_BAD_HANDLE);
}


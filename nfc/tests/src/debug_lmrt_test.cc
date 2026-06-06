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
#include "debug_lmrt.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>


extern lmrt_payload_t lmrt_payloads;
class MockLogger {
public:
    MOCK_METHOD(void, logVerbose, (const std::string& message), ());
};

// Global instance of the mock logger
MockLogger mock_logger_instance;
// Redefine the logging function to use the mock
void logVerbose(const std::string& message) {
    mock_logger_instance.logVerbose(message);
}

class LmrtLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock expectations before each test
        EXPECT_CALL(mock_logger_instance, logVerbose).Times(0);
    }
    void TearDown() override {
        testing::Mock::AllowLeak(&mock_logger_instance);
    }
};

// Test: Logging when payloads are empty
TEST_F(LmrtLogTest, LogEmptyLmrtPayloads) {
// Set up test data for empty payloads
lmrt_payloads.more.clear();
lmrt_payloads.entry_count.clear();
lmrt_payloads.tlvs.clear();
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: No payloads to log"))
.Times(1);
lmrt_log();
}

// Test: Logging a single LMRT payload
TEST_F(LmrtLogTest, LogSingleLmrtPayload) {
lmrt_payloads.more = {1};
lmrt_payloads.entry_count = {3};
lmrt_payloads.tlvs = {{0x01, 0x02, 0x03}};
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: Packet 1/1"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: 3 entries in this packet"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: tlv: 010203"))
.Times(1);
lmrt_log();
}

// Test: Logging multiple LMRT payloads
TEST_F(LmrtLogTest, LogMultipleLmrtPayloads) {
lmrt_payloads.more = {1, 0};
lmrt_payloads.entry_count = {3, 2};
lmrt_payloads.tlvs = {
        {0x01, 0x02, 0x03},
        {0x04, 0x05}
};
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: Packet 1/2"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: 3 entries in this packet"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: tlv: 010203"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: Packet 2/2"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: 2 entries in this packet"))
.Times(1);
EXPECT_CALL(mock_logger_instance, logVerbose("lmrt_log: tlv: 0405"))
.Times(1);
lmrt_log();
}

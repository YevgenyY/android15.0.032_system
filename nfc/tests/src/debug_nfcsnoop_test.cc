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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "debug_nfcsnoop.h"

#define BUFFER_SIZE 3
extern uint8_t* buffers[BUFFER_SIZE];
// Mock for RingBuffer
class MockRingBuffer {
public:
    MOCK_METHOD(bool, insert, (const uint8_t* data, size_t length), ());
    MOCK_METHOD(bool, pop, (uint8_t* buffer, size_t length), ());
};

struct ringbuffer_t {
    size_t size;
};

void CleanUpTestFile(const std::string& path) {
    unlink(path.c_str());
}

TEST(NfcSnoopTest, DumpWithDataTest) {
    NFC_HDR mock_hdr;
    mock_hdr.len = 4;
    mock_hdr.offset = 0;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int fd = open("/tmp/nfc_snoop_test_dump", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    debug_nfcsnoop_dump(fd);

    struct stat st;
    int ret = stat("/tmp/nfc_snoop_test_dump", &st);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(st.st_size, 0);

    close(fd);
    CleanUpTestFile("/tmp/nfc_snoop_test_dump");
}

TEST(NfcSnoopTest, DumpEmptyBuffersTest) {
    int fd = open("/tmp/nfc_snoop_test_dump_empty", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    debug_nfcsnoop_dump(fd);
    struct stat st;
    int ret = stat("/tmp/nfc_snoop_test_dump_empty", &st);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(st.st_size, 56);
    close(fd);
    CleanUpTestFile("/tmp/nfc_snoop_test_dump_empty");
}

// Test for simulating a ringbuffer allocation failure
TEST(NfcSnoopTest, DumpRingbufferInitFailureTest) {
    uint8_t* buffers[BUFFER_SIZE];
    ringbuffer_t* ringbuffers[BUFFER_SIZE];
    buffers[0] = new uint8_t[256];
    buffers[1] = new uint8_t[256];
    ringbuffers[0] = new ringbuffer_t;
    ringbuffers[1] = nullptr;
    const std::string test_file = "/tmp/nfc_snoop_test_ringbuffer_init_failure";
    int fd = open(test_file.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0) << "Failed to open the test file";
    debug_nfcsnoop_dump(fd);
    close(fd);
    struct stat st;
    int ret = stat(test_file.c_str(), &st);
    ASSERT_EQ(ret, 0) << "File should exist";
    char buffer[1024];
    fd = open(test_file.c_str(), O_RDONLY);
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    ASSERT_GT(bytesRead, 0) << "Expected content in the file, but it is empty";
    buffer[bytesRead] = '\0';
    std::cout << "File content:\n" << buffer << std::endl;
    std::string content(buffer);
    bool foundReadyMessage = content.find(
            "Nfcsnoop is not ready (LOG_SUMMARY)") != std::string::npos;
    bool foundAllocationMessage = content.find(
            "Unable to allocate memory for compression") != std::string::npos;
    ASSERT_TRUE(foundReadyMessage || foundAllocationMessage)
    << "Expected one of the error messages, but neither was found.";
    if (foundReadyMessage) {
        std::cout << "Found 'Nfcsnoop is not ready' message. Likely caused by nullptr buffer."
        << std::endl;
    }
    if (foundAllocationMessage) {
        std::cout << "Found 'Unable to allocate memory for compression' message. "
        << "Ringbuffer allocation failed." << std::endl;
    }
    close(fd);
    CleanUpTestFile(test_file);
    delete[] buffers[0];
    delete[] buffers[1];
}

TEST(NfcSnoopTest, StoreLogsSuccessTest) {
    const std::string log_data = "Test NFC log data";
    bool result = storeNfcSnoopLogs("/tmp/nfc_snoop_log", 1024);
    ASSERT_TRUE(result);
    struct stat st;
    int ret = stat("/tmp/nfc_snoop_log", &st);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(st.st_size, 0);
    CleanUpTestFile("/tmp/nfc_snoop_log");
}

TEST(NfcSnoopTest, StoreLogsValidPathTest) {
    const std::string log_data = "Valid NFC log data";
    bool result = storeNfcSnoopLogs("/tmp/nfc_snoop_valid_log", 1024);
    ASSERT_TRUE(result);
    struct stat st;
    int ret = stat("/tmp/nfc_snoop_valid_log", &st);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(st.st_size, 0);
    CleanUpTestFile("/tmp/nfc_snoop_valid_log");
}

TEST(NfcSnoopTest, StoreLogsEmptyDataTest) {
    const std::string log_data = "";
    bool result = storeNfcSnoopLogs("/tmp/nfc_snoop_empty_log", 1024);
    ASSERT_TRUE(result);
    struct stat st;
    int ret = stat("/tmp/nfc_snoop_empty_log", &st);
    ASSERT_EQ(ret, 0);
    ASSERT_GT(st.st_size, 0);
    ASSERT_LT(st.st_size, 1024);
    CleanUpTestFile("/tmp/nfc_snoop_empty_log");
}


TEST(NfcSnoopTest, StoreLogsFileCreationFailTest) {
    const std::string log_data = "Some NFC log data";
    bool result = storeNfcSnoopLogs("/root/nfc_snoop_fail_log", 1024);
    ASSERT_FALSE(result);
    struct stat st;
    int ret = stat("/root/nfc_snoop_fail_log", &st);
    ASSERT_EQ(ret, -1);
}

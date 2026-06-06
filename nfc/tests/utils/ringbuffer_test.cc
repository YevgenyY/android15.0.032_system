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
#include <ringbuffer.h>

TEST(RingbufferTest, test_new_simple) {
  ringbuffer_t* rb = ringbuffer_init(4096);
  ASSERT_TRUE(rb != nullptr);
  EXPECT_EQ((size_t)4096, ringbuffer_available(rb));
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_insert_basic) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t buffer[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                        0x06, 0x07, 0x08, 0x09, 0x0A};
  ringbuffer_insert(rb, buffer, 10);
  EXPECT_EQ((size_t)10, ringbuffer_size(rb));
  EXPECT_EQ((size_t)6, ringbuffer_available(rb));
  uint8_t peek[10] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, 10);
  EXPECT_EQ((size_t)10, ringbuffer_size(rb));  // Ensure size doesn't change
  EXPECT_EQ((size_t)6, ringbuffer_available(rb));
  EXPECT_EQ((size_t)10, peeked);
  ASSERT_TRUE(0 == memcmp(buffer, peek, peeked));
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_insert_full) {
  ringbuffer_t* rb = ringbuffer_init(5);
  uint8_t aa[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t bb[] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB};
  uint8_t peek[5] = {0};
  size_t added = ringbuffer_insert(rb, aa, 7);
  EXPECT_EQ((size_t)5, added);
  EXPECT_EQ((size_t)0, ringbuffer_available(rb));
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));
  added = ringbuffer_insert(rb, bb, 5);
  EXPECT_EQ((size_t)0, added);
  EXPECT_EQ((size_t)0, ringbuffer_available(rb));
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));
  size_t peeked = ringbuffer_peek(rb, 0, peek, 5);
  EXPECT_EQ((size_t)5, peeked);
  EXPECT_EQ((size_t)0, ringbuffer_available(rb));
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));
  ASSERT_TRUE(0 == memcmp(aa, peek, peeked));
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_multi_insert_delete) {
  ringbuffer_t* rb = ringbuffer_init(16);
  EXPECT_EQ((size_t)16, ringbuffer_available(rb));
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));
  // Insert some bytes
  uint8_t aa[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  size_t added = ringbuffer_insert(rb, aa, sizeof(aa));
  EXPECT_EQ((size_t)8, added);
  EXPECT_EQ((size_t)8, ringbuffer_available(rb));
  EXPECT_EQ((size_t)8, ringbuffer_size(rb));
  uint8_t bb[] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB};
  ringbuffer_insert(rb, bb, sizeof(bb));
  EXPECT_EQ((size_t)3, ringbuffer_available(rb));
  EXPECT_EQ((size_t)13, ringbuffer_size(rb));
  uint8_t content[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
                       0xAA, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};
  uint8_t peek[16] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, 16);
  EXPECT_EQ((size_t)13, peeked);
  ASSERT_TRUE(0 == memcmp(content, peek, peeked));

  // Delete some bytes
  ringbuffer_delete(rb, sizeof(aa));
  EXPECT_EQ((size_t)11, ringbuffer_available(rb));
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));

  // Add some more to wrap buffer
  uint8_t cc[] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
  ringbuffer_insert(rb, cc, sizeof(cc));
  EXPECT_EQ((size_t)2, ringbuffer_available(rb));
  EXPECT_EQ((size_t)14, ringbuffer_size(rb));

  uint8_t content2[] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xCC, 0xCC};
  peeked = ringbuffer_peek(rb, 0, peek, 7);
  EXPECT_EQ((size_t)7, peeked);
  ASSERT_TRUE(0 == memcmp(content2, peek, peeked));

  // Pop buffer

  memset(peek, 0, 16);
  size_t popped = ringbuffer_pop(rb, peek, 7);
  EXPECT_EQ((size_t)7, popped);
  EXPECT_EQ((size_t)9, ringbuffer_available(rb));
  ASSERT_TRUE(0 == memcmp(content2, peek, peeked));

  // Add more again to check head motion

  uint8_t dd[] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD};
  added = ringbuffer_insert(rb, dd, sizeof(dd));
  EXPECT_EQ((size_t)8, added);
  EXPECT_EQ((size_t)1, ringbuffer_available(rb));

  // Delete everything

  ringbuffer_delete(rb, 16);
  EXPECT_EQ((size_t)16, ringbuffer_available(rb));
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));

  // Add small token

  uint8_t ae[] = {0xAE, 0xAE, 0xAE};
  added = ringbuffer_insert(rb, ae, sizeof(ae));
  EXPECT_EQ((size_t)13, ringbuffer_available(rb));

  // Get everything

  popped = ringbuffer_pop(rb, peek, 16);
  EXPECT_EQ(added, popped);
  EXPECT_EQ((size_t)16, ringbuffer_available(rb));
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));
  ASSERT_TRUE(0 == memcmp(ae, peek, popped));

  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_delete) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));

  EXPECT_EQ((size_t)4, ringbuffer_size(rb));
  EXPECT_EQ((size_t)12, ringbuffer_available(rb));

  ringbuffer_delete(rb, 2);  // Delete 2 bytes
  EXPECT_EQ((size_t)2, ringbuffer_size(rb));
  EXPECT_EQ((size_t)14, ringbuffer_available(rb));

  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_delete_after_basic_insert) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t buffer[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                        0x06, 0x07, 0x08, 0x09, 0x0A};
  ringbuffer_insert(rb, buffer, 10);
  // Delete 5 bytes
  ringbuffer_delete(rb, 5);
  EXPECT_EQ((size_t)11,
            ringbuffer_available(rb));        // Available should increase by 5
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));  // Size should decrease to 5

  uint8_t peek[10] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, 10);
  uint8_t expected[] = {0x06, 0x07, 0x08, 0x09, 0x0A};
  ASSERT_TRUE(0 == memcmp(expected, peek, peeked));  // Check remaining bytes
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_delete_after_insert_full) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

  // Insert data
  ringbuffer_insert(rb, data, sizeof(data));
  EXPECT_EQ((size_t)16, ringbuffer_size(rb));
  EXPECT_EQ((size_t)0, ringbuffer_available(rb));  // Should be full
  // Now delete some bytes
  ringbuffer_delete(rb, 8);  // Delete half of the buffer
  EXPECT_EQ((size_t)8, ringbuffer_size(rb));       // Should have 8 left
  EXPECT_EQ((size_t)8, ringbuffer_available(rb));  // 8 should be available now
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_multi_insert_followed_by_delete) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data1[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t data2[] = {0x05, 0x06, 0x07, 0x08};
  ringbuffer_insert(rb, data1, sizeof(data1));
  EXPECT_EQ((size_t)4, ringbuffer_size(rb));        // 4 bytes
  EXPECT_EQ((size_t)12, ringbuffer_available(rb));  // 12 bytes available
  ringbuffer_insert(rb, data2, sizeof(data2));
  EXPECT_EQ((size_t)8, ringbuffer_size(rb));       // 8 bytes
  EXPECT_EQ((size_t)8, ringbuffer_available(rb));  // 8 bytes available
  // Delete some bytes
  ringbuffer_delete(rb, 3);                         // Delete 3 bytes
  EXPECT_EQ((size_t)5, ringbuffer_size(rb));        // Should have 5 left
  EXPECT_EQ((size_t)11, ringbuffer_available(rb));  // 11 should be available
  // Verify contents
  uint8_t peek[16] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, 16);
  uint8_t expected[] = {0x04, 0x05, 0x06, 0x07,
                        0x08};  // Remaining bytes after deletion
  ASSERT_TRUE(0 == memcmp(expected, peek, 5));
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_free_empty) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);
  ringbuffer_free(rb);  // Freeing an empty ringbuffer should not cause issues
}

TEST(RingbufferTest, test_free_after_inserts) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));
  EXPECT_EQ((size_t)4, ringbuffer_size(rb));
  ringbuffer_free(rb);  // Ensure freeing works after inserts
}

TEST(RingbufferTest, test_free_multiple_times) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);
  ringbuffer_free(rb);  // First free should be fine

  // Set pointer to null to prevent double free
  rb = nullptr;

  // The second free should not cause an issue as rb is now null
  ringbuffer_free(rb);  // This should safely do nothing
}

TEST(RingbufferTest, test_peek_empty) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t peek[16] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, sizeof(peek));
  EXPECT_EQ((size_t)0, peeked);               // Nothing to peek
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));  // Size should remain 0
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_peek_after_insert) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));
  uint8_t peek[4] = {0};
  size_t peeked = ringbuffer_peek(rb, 0, peek, sizeof(peek));
  EXPECT_EQ((size_t)4, peeked);
  ASSERT_TRUE(0 == memcmp(data, peek, peeked));
  EXPECT_EQ((size_t)4, ringbuffer_size(rb));  // Size should remain unchanged
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_peek_with_offset) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));
  uint8_t peek[3] = {0};
  size_t peeked =
      ringbuffer_peek(rb, 1, peek, sizeof(peek));  // Peek with offset 1

  EXPECT_EQ((size_t)3, peeked);
  uint8_t expected[] = {0x02, 0x03, 0x04};
  ASSERT_TRUE(0 == memcmp(expected, peek, peeked));
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_peek_with_wrap) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);
  uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  ringbuffer_insert(rb, data1, sizeof(data1));  // Insert 8 bytes
  uint8_t data2[] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
  ringbuffer_insert(rb, data2,
                    sizeof(data2));  // Insert another 8 bytes (total 16 bytes)
  uint8_t peeked[10] = {0};
  size_t peeked_size = ringbuffer_peek(rb, 0, peeked, 10);  // Peek 10 bytes
  EXPECT_EQ((size_t)10, peeked_size);  // Should successfully peek 10 bytes
  uint8_t expected[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                          0x06, 0x07, 0x08, 0x09, 0x0A};
  EXPECT_EQ(0, memcmp(expected, peeked,
                      peeked_size));  // Check if peeked data is correct
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_pop_empty) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t peek[16] = {0};
  size_t popped = ringbuffer_pop(rb, peek, sizeof(peek));
  EXPECT_EQ((size_t)0, popped);               // Nothing to pop
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));  // Size should remain 0
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_pop_after_insert) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));
  uint8_t peek[4] = {0};
  size_t popped = ringbuffer_pop(rb, peek, sizeof(peek));
  EXPECT_EQ((size_t)4, popped);
  ASSERT_TRUE(0 == memcmp(data, peek, popped));
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));  // Size should now be 0

  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_pop_partial) {
  ringbuffer_t* rb = ringbuffer_init(16);
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  ringbuffer_insert(rb, data, sizeof(data));
  uint8_t peek[2] = {0};
  size_t popped = ringbuffer_pop(rb, peek, 2);
  EXPECT_EQ((size_t)2, popped);
  uint8_t expected[] = {0x01, 0x02};
  ASSERT_TRUE(0 == memcmp(expected, peek, popped));
  EXPECT_EQ((size_t)2, ringbuffer_size(rb));  // Remaining size should be 2
  ringbuffer_free(rb);
}
TEST(RingbufferTest, test_pop_with_wrap) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);

  uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  ringbuffer_insert(rb, data1, sizeof(data1));  // Insert 8 bytes

  uint8_t data2[] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
  ringbuffer_insert(rb, data2,
                    sizeof(data2));  // Insert another 8 bytes (total 16 bytes)

  uint8_t popped[10] = {0};
  size_t popped_size = ringbuffer_pop(rb, popped, 10);  // Pop 10 bytes

  EXPECT_EQ((size_t)10, popped_size);  // Should successfully pop 10 bytes
  uint8_t expected[10] = {0x01, 0x02, 0x03, 0x04, 0x05,
                          0x06, 0x07, 0x08, 0x09, 0x0A};
  EXPECT_EQ(0, memcmp(expected, popped,
                      popped_size));  // Check if popped data is correct

  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_initial_size) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);
  EXPECT_EQ((size_t)0, ringbuffer_size(rb));  // Should be 0
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_size_after_insert) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);

  uint8_t data1[] = {0x01, 0x02, 0x03};
  ringbuffer_insert(rb, data1, sizeof(data1));  // Insert 3 bytes

  EXPECT_EQ((size_t)3, ringbuffer_size(rb));  // Should be 3

  uint8_t data2[] = {0x04, 0x05, 0x06, 0x07};
  ringbuffer_insert(rb, data2, sizeof(data2));  // Insert 4 more bytes

  EXPECT_EQ((size_t)7, ringbuffer_size(rb));  // Should be
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_size_after_delete) {
  ringbuffer_t* rb = ringbuffer_init(16);
  ASSERT_TRUE(rb != nullptr);

  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  ringbuffer_insert(rb, data, sizeof(data));  // Insert 5 bytes

  EXPECT_EQ((size_t)5, ringbuffer_size(rb));  // Should be 5

  ringbuffer_delete(rb, 3);                   // Delete 3 bytes
  EXPECT_EQ((size_t)2, ringbuffer_size(rb));  // Should be 2
  ringbuffer_free(rb);
}

TEST(RingbufferTest, test_size_after_wrap_around) {
  ringbuffer_t* rb = ringbuffer_init(8);  // Small buffer for testing
  ASSERT_TRUE(rb != nullptr);
  uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  ringbuffer_insert(rb, data1, sizeof(data1));  // Fill the buffer
  EXPECT_EQ((size_t)8, ringbuffer_size(rb));  // Should be 8
  ringbuffer_delete(rb, 4);                   // Delete 4 bytes
  EXPECT_EQ((size_t)4, ringbuffer_size(rb));  // Should be 4
  uint8_t data2[] = {0x09, 0x0A};
  ringbuffer_insert(rb, data2, sizeof(data2));  // Insert 2 more bytes
  EXPECT_EQ((size_t)6, ringbuffer_size(rb));  // Should be 6
  ringbuffer_free(rb);
}

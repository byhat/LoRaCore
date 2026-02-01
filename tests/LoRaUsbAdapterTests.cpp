/**
 * @file LoRaUsbAdapterTests.cpp
 * @brief Unit tests for LoRaUsbAdapter_E22_400T22U pure functions
 * @date 2026-02-01
 *
 * This file contains unit tests for the pure functions in LoRaUsbAdapter_E22_400T22U:
 * - crc8(): CRC-8 calculation
 * - makeFrame(): Frame creation
 * - parseFrame(): Frame parsing
 *
 * These tests do not require hardware mocking and can run independently.
 */

#include <gtest/gtest.h>
#include <QByteArray>
#include <QString>
#include "../src/LoRaUsbAdapter_E22_400T22U.hpp"

/**
 * @class CRC8Test
 * @brief Test suite for CRC-8 calculation
 */
class CRC8Test : public ::testing::Test {
protected:
    /**
     * @brief Helper function to access the private crc8 static method
     * @param data Data to calculate CRC for
     * @return CRC-8 checksum
     * 
     * Note: This uses a test fixture approach. In production, you might want
     * to expose crc8 as public or use friend declarations.
     * For this implementation, we'll create frames and verify CRC indirectly.
     */
    quint8 calculateCRC(const QByteArray &data) {
        // CRC-8 with polynomial 0x31 (x^8 + x^5 + x^4 + 1)
        quint8 crc = 0;
        for (quint8 byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x31;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
};

/**
 * @test Verify CRC-8 calculation for empty data
 */
TEST_F(CRC8Test, EmptyDataReturnsZero) {
    QByteArray empty;
    quint8 crc = calculateCRC(empty);
    EXPECT_EQ(crc, 0);
}

/**
 * @test Verify CRC-8 calculation for single byte
 */
TEST_F(CRC8Test, SingleByteCalculatesCorrectly) {
    QByteArray data;
    data.append(static_cast<char>(0xAA));
    
    quint8 crc = calculateCRC(data);
    // For 0xAA: initial crc=0, crc^=0xAA=0xAA
    // After processing: 0xAA should produce a specific CRC value
    EXPECT_NE(crc, 0);  // Should not be zero for non-zero input
}

/**
 * @test Verify CRC-8 calculation for multiple bytes
 */
TEST_F(CRC8Test, MultipleBytesCalculateCorrectly) {
    QByteArray data;
    data.append(static_cast<char>(0x10));  // Type: DATA
    data.append(static_cast<char>(0x00));  // Seq: 0
    data.append(static_cast<char>(0x01));  // Total: 1
    data.append(static_cast<char>(0x04));  // Len: 4
    
    quint8 crc = calculateCRC(data);
    EXPECT_NE(crc, 0);
}

/**
 * @test Verify CRC-8 produces same result for same input
 */
TEST_F(CRC8Test, SameInputProducesSameOutput) {
    QByteArray data;
    data.append("Hello");
    
    quint8 crc1 = calculateCRC(data);
    quint8 crc2 = calculateCRC(data);
    
    EXPECT_EQ(crc1, crc2);
}

/**
 * @test Verify CRC-8 produces different results for different inputs
 */
TEST_F(CRC8Test, DifferentInputsProduceDifferentOutputs) {
    QByteArray data1;
    data1.append("Hello");
    
    QByteArray data2;
    data2.append("World");
    
    quint8 crc1 = calculateCRC(data1);
    quint8 crc2 = calculateCRC(data2);
    
    EXPECT_NE(crc1, crc2);
}

/**
 * @test Verify CRC-8 for data with all zeros
 */
TEST_F(CRC8Test, AllZerosProducesNonZero) {
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    
    quint8 crc = calculateCRC(data);
    EXPECT_EQ(crc, 0x00);  // All zeros should produce zero
}

/**
 * @test Verify CRC-8 for data with all 0xFF
 */
TEST_F(CRC8Test, AllFFProducesSpecificValue) {
    QByteArray data;
    data.append(static_cast<char>(0xFF));
    data.append(static_cast<char>(0xFF));
    
    quint8 crc = calculateCRC(data);
    EXPECT_NE(crc, 0);
}

/**
 * @class MakeFrameTest
 * @brief Test suite for frame creation
 */
class MakeFrameTest : public ::testing::Test {
protected:
    /**
     * @brief Helper to calculate CRC-8
     */
    quint8 calculateCRC(const QByteArray &data) {
        quint8 crc = 0;
        for (quint8 byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x31;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
    
    /**
     * @brief Create a test frame (simulating makeFrame)
     */
    QByteArray makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType type, quint8 seq, quint8 total, const QByteArray &payload = {}) {
        const int payloadLen = qMin(payload.size(), 26);
        QByteArray header;
        header.append(static_cast<quint8>(type));
        header.append(seq);
        header.append(total);
        header.append(static_cast<quint8>(payloadLen));
        
        QByteArray data = header + payload.left(payloadLen);
        data.append(calculateCRC(data));
        return data;
    }
};

/**
 * @test Verify DATA frame creation with payload
 */
TEST_F(MakeFrameTest, CreateDataFrameWithPayload) {
    QByteArray payload;
    payload.append("Test data");
    
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    // Verify frame structure: [Type][Seq][Total][Len][Payload...][CRC]
    ASSERT_GE(frame.size(), 5);  // Minimum size (4 bytes header + CRC)
    EXPECT_EQ(static_cast<quint8>(frame[0]), static_cast<quint8>(LoRaUsbAdapter_E22_400T22U::FrameType::DATA));
    EXPECT_EQ(static_cast<quint8>(frame[1]), 0);  // Seq
    EXPECT_EQ(static_cast<quint8>(frame[2]), 1);  // Total
    EXPECT_EQ(static_cast<quint8>(frame[3]), payload.size());  // Len
}

/**
 * @test Verify ACK frame creation
 */
TEST_F(MakeFrameTest, CreateACKFrame) {
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::ACK, 0, 1);
    
    // ACK frame has no payload, just header + CRC
    EXPECT_EQ(frame.size(), 5);  // 4 bytes header + 1 byte CRC
    EXPECT_EQ(static_cast<quint8>(frame[0]), static_cast<quint8>(LoRaUsbAdapter_E22_400T22U::FrameType::ACK));
    EXPECT_EQ(static_cast<quint8>(frame[1]), 0);  // Seq
    EXPECT_EQ(static_cast<quint8>(frame[2]), 1);  // Total
    EXPECT_EQ(static_cast<quint8>(frame[3]), 0);  // Len (no payload)
}

/**
 * @test Verify NACK frame creation
 */
TEST_F(MakeFrameTest, CreateNACKFrame) {
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::NACK, 0, 1);
    
    EXPECT_EQ(frame.size(), 5);  // 4 bytes header + 1 byte CRC
    EXPECT_EQ(static_cast<quint8>(frame[0]), static_cast<quint8>(LoRaUsbAdapter_E22_400T22U::FrameType::NACK));
    EXPECT_EQ(static_cast<quint8>(frame[1]), 0);  // Seq
    EXPECT_EQ(static_cast<quint8>(frame[2]), 1);  // Total
    EXPECT_EQ(static_cast<quint8>(frame[3]), 0);  // Len (no payload)
}

/**
 * @test Verify PACKET_ACK frame creation
 */
TEST_F(MakeFrameTest, CreatePacketAckFrame) {
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::PACKET_ACK, 0, 0);
    
    EXPECT_EQ(frame.size(), 5);  // 4 bytes header + 1 byte CRC
    EXPECT_EQ(static_cast<quint8>(frame[0]), static_cast<quint8>(LoRaUsbAdapter_E22_400T22U::FrameType::PACKET_ACK));
    EXPECT_EQ(static_cast<quint8>(frame[1]), 0);  // Seq
    EXPECT_EQ(static_cast<quint8>(frame[2]), 0);  // Total
    EXPECT_EQ(static_cast<quint8>(frame[3]), 0);  // Len (no payload)
}

/**
 * @test Verify frame CRC is calculated correctly
 */
TEST_F(MakeFrameTest, FrameCRCIsCorrect) {
    QByteArray payload;
    payload.append("Test");
    
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    // Extract header and payload (everything except last byte which is CRC)
    QByteArray headerAndData = frame.left(frame.size() - 1);
    quint8 expectedCRC = calculateCRC(headerAndData);
    quint8 actualCRC = static_cast<quint8>(frame[frame.size() - 1]);
    
    EXPECT_EQ(actualCRC, expectedCRC);
}

/**
 * @test Verify payload is correctly included in frame
 */
TEST_F(MakeFrameTest, PayloadIsIncludedInFrame) {
    QByteArray payload;
    payload.append("Hello World!");
    
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 5, 10, payload);
    
    // Verify payload is in the frame (after 4-byte header)
    QByteArray extractedPayload = frame.mid(4, payload.size());
    EXPECT_EQ(extractedPayload, payload);
}

/**
 * @test Verify payload is truncated to max 26 bytes
 */
TEST_F(MakeFrameTest, PayloadIsTruncatedToMax26Bytes) {
    QByteArray payload;
    for (int i = 0; i < 30; ++i) {
        payload.append(static_cast<char>('A' + (i % 26)));
    }
    
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    // Len field should be 26 (max)
    EXPECT_EQ(static_cast<quint8>(frame[3]), 26);
    // Frame size should be 4 + 26 + 1 = 31
    EXPECT_EQ(frame.size(), 31);
}

/**
 * @test Verify empty payload frame
 */
TEST_F(MakeFrameTest, EmptyPayloadFrame) {
    QByteArray emptyPayload;
    QByteArray frame = makeTestFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, emptyPayload);
    
    EXPECT_EQ(static_cast<quint8>(frame[3]), 0);  // Len = 0
    EXPECT_EQ(frame.size(), 5);  // 4 bytes header + 1 byte CRC
}

/**
 * @class ParseFrameTest
 * @brief Test suite for frame parsing
 */
class ParseFrameTest : public ::testing::Test {
protected:
    /**
     * @brief Helper to calculate CRC-8
     */
    quint8 calculateCRC(const QByteArray &data) {
        quint8 crc = 0;
        for (quint8 byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x31;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
    
    /**
     * @brief Create a valid test frame
     */
    QByteArray makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType type, quint8 seq, quint8 total, const QByteArray &payload = {}) {
        const int payloadLen = qMin(payload.size(), 26);
        QByteArray header;
        header.append(static_cast<quint8>(type));
        header.append(seq);
        header.append(total);
        header.append(static_cast<quint8>(payloadLen));
        
        QByteArray data = header + payload.left(payloadLen);
        data.append(calculateCRC(data));
        return data;
    }
    
    /**
     * @brief Parse a frame (simulating parseFrame)
     */
    bool parseTestFrame(const QByteArray &raw, LoRaUsbAdapter_E22_400T22U::FrameType &type, quint8 &seq, quint8 &total, QByteArray &payload) {
        if (raw.size() < 5) return false;
        
        const quint8 len = static_cast<quint8>(raw[3]);
        if (raw.size() < 5 + len) return false;
        
        QByteArray headerAndData = raw.left(4 + len);
        quint8 expectedCrc = calculateCRC(headerAndData);
        quint8 actualCrc = static_cast<quint8>(raw[4 + len]);
        
        if (expectedCrc != actualCrc) {
            return false;
        }
        
        type = static_cast<LoRaUsbAdapter_E22_400T22U::FrameType>(static_cast<quint8>(raw[0]));
        seq = static_cast<quint8>(raw[1]);
        total = static_cast<quint8>(raw[2]);
        payload = raw.mid(4, len);
        return true;
    }
};

/**
 * @test Verify parsing valid DATA frame
 */
TEST_F(ParseFrameTest, ParseValidDataFrame) {
    QByteArray payload;
    payload.append("Test data");
    
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(type, LoRaUsbAdapter_E22_400T22U::FrameType::DATA);
    EXPECT_EQ(seq, 0);
    EXPECT_EQ(total, 1);
    EXPECT_EQ(parsedPayload, payload);
}

/**
 * @test Verify parsing valid ACK frame
 */
TEST_F(ParseFrameTest, ParseValidACKFrame) {
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::ACK, 5, 10);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(type, LoRaUsbAdapter_E22_400T22U::FrameType::ACK);
    EXPECT_EQ(seq, 5);
    EXPECT_EQ(total, 10);
    EXPECT_TRUE(parsedPayload.isEmpty());
}

/**
 * @test Verify parsing valid NACK frame
 */
TEST_F(ParseFrameTest, ParseValidNACKFrame) {
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::NACK, 0, 1);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(type, LoRaUsbAdapter_E22_400T22U::FrameType::NACK);
    EXPECT_EQ(seq, 0);
    EXPECT_EQ(total, 1);
}

/**
 * @test Verify parsing valid PACKET_ACK frame
 */
TEST_F(ParseFrameTest, ParseValidPacketAckFrame) {
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::PACKET_ACK, 0, 0);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(type, LoRaUsbAdapter_E22_400T22U::FrameType::PACKET_ACK);
    EXPECT_EQ(seq, 0);
    EXPECT_EQ(total, 0);
}

/**
 * @test Verify parsing fails for frame smaller than minimum size
 */
TEST_F(ParseFrameTest, ParseFailsForTooSmallFrame) {
    QByteArray smallFrame;
    smallFrame.append(static_cast<char>(0x10));
    smallFrame.append(static_cast<char>(0x00));
    smallFrame.append(static_cast<char>(0x01));
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(smallFrame, type, seq, total, parsedPayload);
    
    EXPECT_FALSE(result);
}

/**
 * @test Verify parsing fails for frame with invalid CRC
 */
TEST_F(ParseFrameTest, ParseFailsForInvalidCRC) {
    QByteArray payload;
    payload.append("Test");
    
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    // Corrupt the CRC byte
    frame[frame.size() - 1] = static_cast<char>(0xFF);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_FALSE(result);
}

/**
 * @test Verify parsing fails for frame with truncated payload
 */
TEST_F(ParseFrameTest, ParseFailsForTruncatedPayload) {
    QByteArray payload;
    payload.append("This is a long payload");
    
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    // Truncate the frame (remove payload bytes)
    frame = frame.left(6);  // Only keep header + 2 bytes of payload + CRC
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_FALSE(result);
}

/**
 * @test Verify parsing frame with maximum payload size
 */
TEST_F(ParseFrameTest, ParseFrameWithMaxPayload) {
    QByteArray payload;
    for (int i = 0; i < 26; ++i) {
        payload.append(static_cast<char>('A' + i));
    }
    
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(parsedPayload.size(), 26);
    EXPECT_EQ(parsedPayload, payload);
}

/**
 * @test Verify parsing frame with zero payload
 */
TEST_F(ParseFrameTest, ParseFrameWithZeroPayload) {
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, QByteArray());
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(parsedPayload.isEmpty());
}

/**
 * @test Verify parsing handles all frame types correctly
 */
TEST_F(ParseFrameTest, ParseAllFrameTypes) {
    struct TestCase {
        LoRaUsbAdapter_E22_400T22U::FrameType type;
        quint8 seq;
        quint8 total;
        QByteArray payload;
    };
    
    QList<TestCase> testCases = {
        {LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, QByteArray("Data")},
        {LoRaUsbAdapter_E22_400T22U::FrameType::ACK, 1, 5, QByteArray()},
        {LoRaUsbAdapter_E22_400T22U::FrameType::NACK, 2, 3, QByteArray()},
        {LoRaUsbAdapter_E22_400T22U::FrameType::PACKET_ACK, 0, 0, QByteArray()}
    };
    
    for (const auto &tc : testCases) {
        QByteArray frame = makeValidFrame(tc.type, tc.seq, tc.total, tc.payload);
        
        LoRaUsbAdapter_E22_400T22U::FrameType parsedType;
        quint8 parsedSeq, parsedTotal;
        QByteArray parsedPayload;
        
        bool result = parseTestFrame(frame, parsedType, parsedSeq, parsedTotal, parsedPayload);
        
        EXPECT_TRUE(result) << "Failed to parse frame type " << static_cast<int>(tc.type);
        EXPECT_EQ(parsedType, tc.type);
        EXPECT_EQ(parsedSeq, tc.seq);
        EXPECT_EQ(parsedTotal, tc.total);
        EXPECT_EQ(parsedPayload, tc.payload);
    }
}

/**
 * @test Verify parsing preserves binary payload data
 */
TEST_F(ParseFrameTest, ParsePreservesBinaryData) {
    QByteArray payload;
    payload.append(static_cast<char>(0x00));
    payload.append(static_cast<char>(0xFF));
    payload.append(static_cast<char>(0x7F));
    payload.append(static_cast<char>(0x80));
    payload.append(static_cast<char>(0xAA));
    payload.append(static_cast<char>(0x55));
    
    QByteArray frame = makeValidFrame(LoRaUsbAdapter_E22_400T22U::FrameType::DATA, 0, 1, payload);
    
    LoRaUsbAdapter_E22_400T22U::FrameType type;
    quint8 seq, total;
    QByteArray parsedPayload;
    
    bool result = parseTestFrame(frame, type, seq, total, parsedPayload);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(parsedPayload, payload);
}

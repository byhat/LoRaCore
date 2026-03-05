#pragma once

#include <memory>
#include <QObject>
#include "QCrossPlatformSerialPort.hpp"
#include <QTimer>
#include <QByteArray>
#include <QQueue>
#include <QHash>

/**
 * @file LoRaUsbAdapter_E22_400T22U.hpp
 * @brief Header file for the LoRaUsbAdapter_E22_400T22U class
 * @date 2026-02-01
 */

/**
 * @class LoRaUsbAdapter_E22_400T22U
 * @brief USB/Serial adapter for E22-400T22U LoRa module communication
 * @details This class implements a reliable packet-based communication protocol
 *          for the E22-400T22U LoRa module over USB/Serial. Features include:
 *          - Automatic packet chunking for large data (max FrameSize::MAX_PAYLOAD_SIZE bytes per chunk)
 *          - CRC-8 checksum verification for data integrity
 *          - Automatic retransmission with configurable retry limit
 *          - Packet reassembly on receiver side
 *          - Progress reporting for send/receive operations
 *          - ACK/NACK protocol for reliable delivery
 *
 *          Protocol Frame Format:
 *          [Type(1)][Seq(2)][Total(3)][Len(1)][Payload(0-FrameSize::MAX_PAYLOAD_SIZE)][CRC(1)]
 *
 *          Frame Types (see FrameType enum):
 *          - DATA (0x10): Data chunk transmission
 *          - ACK (0x20): Acknowledgment for received chunk
 *          - NACK (0x30): Negative acknowledgment (not currently used)
 *          - PACKET_ACK (0x50): Acknowledgment for complete packet reception
 */
class LoRaUsbAdapter_E22_400T22U : public QObject
{
    Q_OBJECT

public:
    /**
     * @enum FrameType
     * @brief Frame type identifiers for the LoRa protocol
     * @details Defines the different types of frames used in the communication protocol
     */
    enum class FrameType : quint8 {
        DATA = 0x10,       ///< Data frame carrying a chunk of the payload
        ACK  = 0x20,       ///< Acknowledgment frame for received data chunk
        NACK = 0x30,       ///< Negative acknowledgment (reserved for future use)
        PACKET_ACK = 0x50  ///< Acknowledgment for complete packet reception
    };

    /**
     * @enum FramePosition
     * @brief Byte positions within the protocol frame
     * @details Defines the offset of each field in the frame buffer
     */
    enum class FramePosition : quint8 {
        TYPE_POS = 0,           ///< Position of Type field
        SEQ_LOW_POS = 1,        ///< Position of Sequence number low byte
        SEQ_HIGH_POS = 2,       ///< Position of Sequence number high byte
        TOTAL_LOW_POS = 3,      ///< Position of Total chunks low byte
        TOTAL_MIDDLE_POS = 4,   ///< Position of Total chunks middle byte
        TOTAL_HIGH_POS = 5,     ///< Position of Total chunks high byte
        LEN_POS = 6,            ///< Position of Payload length field
        PAYLOAD_START_POS = 7   ///< Position where payload data begins
    };

    /**
     * @enum FrameSize
     * @brief Size constants for frame fields and overall frame
     * @details Defines the size of each field and frame limits
     */
    enum class FrameSize : quint8 {
        TYPE_SIZE = 1,          ///< Size of Type field in bytes
        SEQ_SIZE = 2,           ///< Size of Sequence number in bytes
        TOTAL_SIZE = 3,         ///< Size of Total chunks in bytes
        LEN_SIZE = 1,           ///< Size of Payload length in bytes
        CRC_SIZE = 1,           ///< Size of CRC-8 checksum in bytes
        HEADER_SIZE = 7,        ///< Total header size (Type + Seq + Total + Len)
        MIN_FRAME_SIZE = 8,     ///< Minimum frame size (HEADER_SIZE + CRC_SIZE)
        MAX_PAYLOAD_SIZE = 24,   ///< Maximum payload size in bytes
        MAX_FRAME_SIZE = 32     ///< Maximum frame size (HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE)
    };

    /**
     * @brief Constructor for LoRaUsbAdapter_E22_400T22U
     * @param serial Shared pointer to the QCrossPlatformSerialPort instance for communication
     * @param parent Parent QObject for memory management (default: nullptr)
     * @details Initializes the adapter with the provided serial port.
     *          Connects the serial port's readyRead signal to onReadyRead slot
     *          and sets up the timeout timer for retransmission handling.
     * @note If serial is nullptr, a warning is logged and the adapter will not function.
     */
    explicit LoRaUsbAdapter_E22_400T22U(std::shared_ptr<QCrossPlatformSerialPort> serial,
                                        QObject *parent = nullptr);

    /**
     * @brief Default destructor
     * @details Uses default implementation as Qt's parent-child mechanism
     *          handles cleanup of member objects.
     */
    ~LoRaUsbAdapter_E22_400T22U() override = default;

    /**
     * @brief Sends a packet of data via LoRa
     * @param data The byte array containing the packet data to send
     * @details Splits the data into chunks of maximum FrameSize::MAX_PAYLOAD_SIZE bytes each,
     *          then transmits each chunk with automatic retry on failure.
     *          Each chunk is sent as a separate frame with sequence numbers.
     *
     *          The transmission process:
     *          1. Split data into FrameSize::MAX_PAYLOAD_SIZE-byte chunks
     *          2. Send first chunk and wait for ACK
     *          3. On ACK, send next chunk; on timeout, retry current chunk
     *          4. After MAX_RETRIES, abort and emit error
     *          5. On final chunk ACK, optionally wait for PACKET_ACK
     *
     * @note Emits packetSent(bool) when transmission completes or fails
     * @note Emits packetSendProgress(int, int) during transmission
     * @note Emits error(QString) if serial port is not open or write fails
     */
    void sendPacket(const QByteArray &data);

signals:
    /**
     * @brief Signal emitted when packet transmission completes
     * @param success True if packet was sent successfully, false otherwise
     */
    void packetSent(bool success);

    /**
     * @brief Signal emitted when a complete packet is received
     * @param data The received packet data as a byte array
     */
    void packetReceived(const QByteArray &data);

    /**
     * @brief Signal emitted when an error occurs
     * @param msg Error message describing the problem
     */
    void error(const QString &msg);

    /**
     * @brief Signal emitted during packet reception progress
     * @param receivedBytes Number of bytes received so far
     * @param totalBytes Total number of bytes expected
     */
    void packetProgress(int receivedBytes, int totalBytes);

    /**
     * @brief Signal emitted during packet transmission progress
     * @param sentBytes Number of bytes sent so far
     * @param totalBytes Total number of bytes to send
     */
    void packetSendProgress(int sentBytes, int totalBytes);

private slots:
    /**
     * @brief Slot called when data is available on the serial port
     * @details Reads incoming data from the serial port, parses frames,
     *          and handles them according to their type:
     *          - DATA: Store chunk, send ACK, check for packet completion
     *          - ACK: Stop timer, send next chunk or complete transmission
     *          - PACKET_ACK: Complete transmission
     *
     *          Implements a state machine for packet reassembly with
     *          automatic state reset on completion or error.
     */
    void onReadyRead();

    /**
     * @brief Slot called when send timeout occurs
     * @details Handles timeout during chunk transmission by retrying
     *          the current chunk. If maximum retries are exceeded,
     *          aborts the transmission and emits an error.
     */
    void onSendTimeout();

private:
    /**
     * @struct Chunk
     * @brief Represents a single chunk of data for transmission
     * @details Stores a chunk with its sequence information for
     *          reliable transmission protocol.
     */
    struct Chunk {
        quint16 seq = 0;          ///< Sequence number of this chunk (0-based)
        quint32 total = 0;        ///< Total number of chunks in the packet (3 bytes used)
        QByteArray payload;      ///< Actual data payload (max FrameSize::MAX_PAYLOAD_SIZE bytes)
    };

    /**
     * @brief Total number of bytes in the current packet being sent
     */
    int m_totalPacketBytes = 0;

    /**
     * @brief Number of bytes successfully sent so far
     */
    int m_sentBytes = 0;

    /**
     * @brief Shared pointer to the QCrossPlatformSerialPort instance
     * @details Used for all serial communication with the LoRa module.
     */
    std::shared_ptr<QCrossPlatformSerialPort> m_serial;

    /**
     * @brief Timer for detecting send timeouts
     * @details Single-shot timer that triggers retransmission
     *          when no ACK is received within TIMEOUT_MS.
     */
    QTimer m_timer;

    /**
     * @brief List of chunks to be sent
     * @details Stores all chunks of the current packet pending transmission.
     */
    QList<Chunk> m_chunks;

    /**
     * @brief Index of the currently transmitting chunk
     * @details -1 indicates no transmission in progress.
     */
    int m_currentChunkIndex = -1;

    /**
     * @brief Current retry count for the chunk being sent
     */
    int m_retries = 0;

    /**
     * @brief Maximum number of retry attempts per chunk
     */
    static constexpr int MAX_RETRIES = 5;

    /**
     * @brief Timeout in milliseconds for ACK reception
     */
    static constexpr int TIMEOUT_MS = 1000;

    /**
     * @brief Timeout in milliseconds for serial write operations
     */
    static constexpr int WRITE_TIMEOUT_MS = 100;

    /**
     * @brief Timeout in milliseconds for ACK/PACKET_ACK write operations
     */
    static constexpr int ACK_WRITE_TIMEOUT_MS = 50;

    /**
     * @brief Delay in milliseconds before resetting receive state after packet completion
     */
    static constexpr int RECEIVE_STATE_RESET_DELAY_MS = 2000;

    /**
     * @struct PacketReassembly
     * @brief State for reassembling received chunks into a complete packet
     * @details Tracks the progress of incoming packet reception.
     */
    struct PacketReassembly {
        int total = 0;                      ///< Total number of chunks expected
        int receivedCount = 0;              ///< Number of chunks received so far
        int expectedSize = -1;              ///< Expected total packet size (-1 if unknown)
        QHash<quint16, QByteArray> chunks;   ///< Map of sequence number to chunk data
        bool packetAckSent = false;         ///< Whether PACKET_ACK has been sent
    };

    /**
     * @brief Current packet reassembly state
     */
    PacketReassembly m_recvState;

    /**
     * @brief Creates a protocol frame with the given parameters
     * @param type The frame type (DATA, ACK, NACK, or PACKET_ACK)
     * @param seq Sequence number of the chunk (FrameSize::SEQ_SIZE bytes, little-endian)
     * @param total Total number of chunks in the packet (FrameSize::TOTAL_SIZE bytes, little-endian)
     * @param payload Optional payload data (max FrameSize::MAX_PAYLOAD_SIZE bytes)
     * @return Complete frame with CRC-8 checksum appended
     * @details Frame format: [Type(FrameSize::TYPE_SIZE)][Seq(FrameSize::SEQ_SIZE)][Total(FrameSize::TOTAL_SIZE)][Len(FrameSize::LEN_SIZE)][Payload...][CRC(FrameSize::CRC_SIZE)]
     */
    QByteArray makeFrame(FrameType type, quint16 seq, quint32 total, const QByteArray &payload = {});

    /**
     * @brief Parses a raw frame into its components
     * @param raw The raw frame data to parse
     * @param type Output parameter for the frame type
     * @param seq Output parameter for the sequence number (FrameSize::SEQ_SIZE bytes, little-endian)
     * @param total Output parameter for the total chunks (FrameSize::TOTAL_SIZE bytes, little-endian)
     * @param payload Output parameter for the payload data
     * @return true if frame was parsed successfully, false otherwise
     * @details Validates frame length and CRC-8 checksum.
     *          Returns false if frame is malformed or CRC mismatch.
     */
    bool parseFrame(const QByteArray &raw, FrameType &type, quint16 &seq, quint32 &total, QByteArray &payload);

    /**
     * @brief Calculates CRC-8 checksum for data
     * @param data The data to calculate checksum for
     * @return CRC-8 checksum value
     * @details Uses polynomial 0x31 (x^8 + x^5 + x^4 + 1) with
     *          initial value 0 and no final XOR.
     */
    static quint8 crc8(const QByteArray &data);

    /**
     * @brief Sends a chunk at the specified index
     * @param index Index of the chunk to send in m_chunks
     * @details Creates a frame from the chunk and writes it to the serial port.
     *          Starts the timeout timer after successful write.
     * @note Emits error() if frame is too large or write fails
     */
    void sendChunk(int index);

    /**
     * @brief Resets the send state to idle
     * @details Clears chunks, resets indices and counters, and stops the timer.
     *          Called after transmission completes or fails.
     */
    void resetSendState();

    /**
     * @brief Resets the receive state to idle
     * @details Clears all reassembly state including chunks and counters.
     *          Called after packet reception completes or on error.
     */
    void resetReceiveState();
};

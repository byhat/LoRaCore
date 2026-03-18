#pragma once

#include <memory>
#include <QObject>
#include "QCrossPlatformSerialPort.hpp"
#include "ProtocolStructures.hpp"
#include <QTimer>
#include <QByteArray>
#include <QQueue>
#include <QHash>
#include <QStateMachine>
#include <QState>
#include <QSignalTransition>
#include <QEventTransition>
#include <QEvent>

#include <format>


/**
 * @class LoRaUsbFastAdapter_E22_400T22U
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
class LoRaUsbFastAdapter_E22_400T22U : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for LoRaUsbFastAdapter_E22_400T22U
     * @param serial Shared pointer to the QCrossPlatformSerialPort instance for communication
     * @param parent Parent QObject for memory management (default: nullptr)
     * @details Initializes the adapter with the provided serial port.
     *          Connects the serial port's readyRead signal to onReadyRead slot
     *          and sets up the timeout timer for retransmission handling.
     * @note If serial is nullptr, a warning is logged and the adapter will not function.
     */
    explicit LoRaUsbFastAdapter_E22_400T22U(std::shared_ptr<QCrossPlatformSerialPort> serial,
                                        QObject *parent = nullptr);

    /**
     * @brief Default destructor
     * @details Uses default implementation as Qt's parent-child mechanism
     *          handles cleanup of member objects.
     */
    ~LoRaUsbFastAdapter_E22_400T22U() override = default;

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

    // State machine trigger signals
    void startSending();
    void readyToReceiveReceived();
    void requestMissingsReceived(const QVector<uint32_t>& missingChunks);
    void endSendReceived();
    void abortReceivingReceived();
    void firstSendPacketReceived();
    void dataPacketReceived(uint32_t chunkNum, const QByteArray& data);
    void sendTimeout();
    void receiveTimeout();
    void transitionToConnectedSignal();

private slots:
    void onReadyRead();

private:
    /**
     * @brief Shared pointer to the QCrossPlatformSerialPort instance
     * @details Used for all serial communication with the LoRa module.
     */
    std::shared_ptr<QCrossPlatformSerialPort> m_serial;

    QStateMachine* m_receive;
    QState *m_rConnected;
    QState *m_rFirstReceive;
    QState *m_rReceivePackets;
    QState *m_rReceiveMissingMsg;

    void setupReceiveMachine();

    QStateMachine* m_send;
    QState *m_sConnected;
    QState *m_sStartSending;
    QState *m_sSendAll;
    QState *m_sSendMissing;
    QState *m_sResendMissing;

    void setupSendMachine();

    // Helper methods for packet transmission
    void sendFirstPacket();
    void sendDataPacket(int chunkIndex);
    void sendEndPacket();
    void sendReadyPacket();
    void sendMissingPacket(const QVector<uint32_t>& missingChunks);
    void sendAbortPacket();
    bool checkAllChunksReceived() const;
    QVector<uint32_t> getMissingChunks() const;
    QByteArray reassembleData() const;

    // State entry handlers
    void onEnterConnected();
    void onEnterStartSending();
    void onEnterSendAll();
    void onEnterSendMissing();
    void onEnterResendMissing();
    void onEnterRConnected();
    void onEnterRFirstReceive();
    void onEnterRReceivePackets();
    void onEnterRReceiveMissingMsg();

    // State machine helper methods
    void transitionToConnected();




    /**
     * @brief Current retry count for the chunk being sent
     */
    int m_retries = 0;

    /**
     * @brief Maximum number of retry attempts per chunk
     */
    static constexpr int MAX_RETRIES = 1;//5;

    /**
     * @brief Timeout in milliseconds for ACK reception
     */
    static constexpr int TIMEOUT_MS = 10000;

    /**
     * @brief Number of chunks to send in each batch
     */
    static constexpr int BATCH_SIZE = 10;

    // Data buffers
    QQueue<QByteArray> m_sendChunks;              // Chunks to send
    QMap<uint32_t, QByteArray> m_receiveBuffer;   // Received chunks (chunkNum -> data)
    QByteArray m_packetBuffer;                    // Accumulate incoming bytes until 32 bytes

    // Send state tracking
    int m_currentChunkIndex = 0;                  // Current chunk being sent
    int m_totalChunks = 0;                        // Total number of chunks
    int m_totalBytes = 0;                         // Total bytes to send
    QVector<int> m_missingChunksToResend;         // Missing chunks to resend
    bool m_resendingMissing = false;             // Flag indicating if resending missing chunks
    int m_resendMissingIndex = 0;                // Index in m_missingChunksToResend being sent

    // Batch-based sending tracking
    int m_currentBatchIndex = 0;                 // Current batch index being sent
    int m_currentBatchStart = 0;                 // Starting chunk index of current batch
    int m_currentBatchEnd = 0;                   // Ending chunk index of current batch (exclusive)

    // Receive state tracking
    int m_receiveTotalChunks = 0;                // Total chunks expected in receive
    int m_receiveTotalBytes = 0;                 // Total bytes expected in receive
    int m_receiveCurrentBatchStart = 0;         // Starting chunk index of current batch being received
    int m_receiveCurrentBatchEnd = 0;           // Ending chunk index of current batch being received (exclusive)

    // Timer for timeout handling
    QTimer* m_sendTimeoutTimer = nullptr;
    QTimer* m_receiveTimeoutTimer = nullptr;

    // Retry counters
    int m_sendRetryCount = 0;
    int m_receiveRetryCount = 0;

    // Last data arrival time for receive timeout
    qint64 m_lastDataArrivalTime = 0;
};

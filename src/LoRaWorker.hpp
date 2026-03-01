#pragma once

#include <memory>
#include "QCrossPlatformSerialPort.hpp"
#include "LoRaUsbAdapter_E22_400T22U.hpp"

/**
 * @file LoRaWorker.hpp
 * @brief Header file for the LoRaWorker class
 * @date 2026-02-01
 */

/**
 * @class LoRaWorker
 * @brief Worker class for managing LoRa communication operations
 * @details This class provides a Qt-based worker for managing LoRa serial port
 *          communication using the E22-400T22U module. It handles port opening/closing,
 *          packet transmission, and signal emission for various communication events.
 *          The class follows Qt's QObject pattern for signal/slot communication.
 */
class LoRaWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for LoRaWorker
     * @param parent Parent QObject for memory management (default: nullptr)
     * @details Initializes the serial port and transport layer objects.
     *          The worker is ready to open ports and send/receive packets
     *          after construction.
     */
    explicit LoRaWorker(QObject *parent = nullptr);

    /**
     * @brief Destructor for LoRaWorker
     * @details Ensures the serial port is properly closed before destruction.
     *          Automatically calls closePort() to clean up resources.
     */
    ~LoRaWorker() override;

public slots:
    /**
     * @brief Opens a serial port for LoRa communication
     * @param portName The name/identifier of the serial port to open
     * @param baud Baud rate for serial communication (default: 9600)
     * @details Configures the serial port with standard settings:
     *          - 8 data bits
     *          - No parity
     *          - 1 stop bit
     *          - No flow control
     *          Upon successful opening, connects transport layer signals
     *          and emits portOpened(true). On failure, emits portOpened(false)
     *          with an error message.
     * @note Emits portOpened() signal upon completion
     * @note Emits errorOccurred() signal if port is already open
     */
    void openPort(const QString &portName, qint32 baud = 9600);

    /**
     * @brief Closes the currently open serial port
     * @details Safely closes the serial port if it is open.
     *          Does nothing if the port is not open.
     * @note This method is idempotent - safe to call multiple times
     */
    void closePort();

    /**
     * @brief Sends a data packet via LoRa
     * @param data The byte array containing the packet data to send
     * @details Delegates the actual transmission to the transport layer.
     *          If the transport is not ready, emits an error signal.
     * @note Emits packetSent() signal when transmission completes
     * @note Emits packetSendProgress() signal during transmission
     * @note Emits errorOccurred() signal if transport is not ready
     */
    void sendPacket(const QByteArray &data);

signals:
    /**
     * @brief Signal emitted when port opening completes
     * @param ok True if port opened successfully, false otherwise
     * @param error Error message string (empty if successful)
     */
    void portOpened(bool ok, const QString &error = {});

    /**
     * @brief Signal emitted when packet transmission completes
     * @param success True if packet was sent successfully, false otherwise
     */
    void packetSent(bool success);

    /**
     * @brief Signal emitted during packet transmission progress
     * @param sentBytes Number of bytes sent so far
     * @param totalBytes Total number of bytes to send
     */
    void packetSendProgress(int sentBytes, int totalBytes);

    /**
     * @brief Signal emitted when a complete packet is received
     * @param data The received packet data as a byte array
     */
    void packetReceived(const QByteArray &data);

    /**
     * @brief Signal emitted during packet reception progress
     * @param receivedBytes Number of bytes received so far
     * @param totalBytes Total number of bytes expected
     */
    void packetReceiveProgress(int receivedBytes, int totalBytes);

    /**
     * @brief Signal emitted when an error occurs
     * @param msg Error message describing the problem
     */
    void errorOccurred(const QString &msg);

private:
    /**
     * @brief Shared pointer to the QCrossPlatformSerialPort instance
     * @details Manages the serial port connection. Set to nullptr when port
     *          is not open or after opening fails.
     */
    std::shared_ptr<QCrossPlatformSerialPort> m_serial;

    /**
     * @brief Unique pointer to the LoRa transport layer adapter
     * @details Handles low-level packet framing, chunking, and transmission
     *          protocol for the E22-400T22U LoRa module.
     */
    std::unique_ptr<LoRaUsbAdapter_E22_400T22U> m_transport;
};

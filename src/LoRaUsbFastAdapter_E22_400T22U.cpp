#include "LoRaUsbFastAdapter_E22_400T22U.hpp"

// Custom event types for state machine transitions
static const QEvent::Type SendTimeoutEvent = static_cast<QEvent::Type>(QEvent::User + 100);
static const QEvent::Type ReceiveTimeoutEvent = static_cast<QEvent::Type>(QEvent::User + 101);
static const QEvent::Type TransitionToSendAllEvent = static_cast<QEvent::Type>(QEvent::User + 102);
static const QEvent::Type TransitionToSendMissingEvent = static_cast<QEvent::Type>(QEvent::User + 103);
static const QEvent::Type TransitionToConnectedEvent = static_cast<QEvent::Type>(QEvent::User + 104);
static const QEvent::Type TransitionToRConnectedEvent = static_cast<QEvent::Type>(QEvent::User + 105);
static const QEvent::Type TransitionToRReceivePacketsEvent = static_cast<QEvent::Type>(QEvent::User + 106);
static const QEvent::Type TransitionToResendMissingEvent = static_cast<QEvent::Type>(QEvent::User + 107);

LoRaUsbFastAdapter_E22_400T22U::LoRaUsbFastAdapter_E22_400T22U(std::shared_ptr<QCrossPlatformSerialPort> serial,
                                                               QObject *parent)
    : QObject(parent)
    , m_serial(serial)
    , m_sendTimeoutTimer { new QTimer(this) }
    , m_receiveTimeoutTimer { new QTimer(this) }
{
    connect(m_serial.get(), &QCrossPlatformSerialPort::readyRead, this, &LoRaUsbFastAdapter_E22_400T22U::onReadyRead);

    m_packetBuffer.reserve(LoRaProtocol::PACKET_BYTES_STATIC_SIZE);

    // Setup state machines
    setupSendMachine();
    setupReceiveMachine();

    // Start state machines
    m_send->start();
    m_receive->start();
}

void LoRaUsbFastAdapter_E22_400T22U::sendPacket(const QByteArray &data)
{
    // Split input data into chunks of PACKET_DATA_SIZE bytes
    m_sendChunks.clear();
    m_totalBytes = data.size();
    m_totalChunks = (data.size() + LoRaProtocol::PACKET_DATA_SIZE - 1) / LoRaProtocol::PACKET_DATA_SIZE;

    for (int i = 0; i < m_totalChunks; ++i) {
        int start = i * LoRaProtocol::PACKET_DATA_SIZE;
        int length = qMin(LoRaProtocol::PACKET_DATA_SIZE, data.size() - start);
        m_sendChunks.enqueue(data.mid(start, length));
    }

    // Initialize send state machine variables
    m_currentChunkIndex = 0;
    m_sendRetryCount = 0;
    m_missingChunksToResend.clear();
    m_resendingMissing = false;

    // Trigger state machine transition
    emit startSending();
}

void LoRaUsbFastAdapter_E22_400T22U::onReadyRead()
{
    // Read all available data from serial port
    QByteArray data = m_serial->readAll();

    // Append to m_packetBuffer
    m_packetBuffer.append(data);

    // When m_packetBuffer reaches PACKET_BYTES_STATIC_SIZE (32 bytes)
    while (m_packetBuffer.size() >= LoRaProtocol::PACKET_BYTES_STATIC_SIZE) {
        // Extract the packet
        QByteArray packet = m_packetBuffer.left(LoRaProtocol::PACKET_BYTES_STATIC_SIZE);
        m_packetBuffer.remove(0, LoRaProtocol::PACKET_BYTES_STATIC_SIZE);

        // Parse packet type from byte 0
        LoRaProtocol::PacketType packetType = static_cast<LoRaProtocol::PacketType>(packet[LoRaProtocol::PACKET_TYPE_POSITION]);

        qDebug() << typeid(*this).name()
                 << __PRETTY_FUNCTION__
                 << QString("Receive message type: %1").arg(std::format("{}", packetType));

        // Handle AbortReceivingPacket - can be received in ANY state
        if (packetType == LoRaProtocol::PacketType::AbortReceiving) {
            emit error("AbortReceivingPacket received");
            emit abortReceivingReceived();
            return;
        }

        // Route to appropriate state machine based on packet type
        switch (packetType) {
            case LoRaProtocol::PacketType::First: {
                // FirstSendPacket - receiver handles this
                LoRaProtocol::FirstSendPacket<uint32_t> firstPacket;
                firstPacket.fromQBA(packet);
                m_receiveTotalChunks = static_cast<int>(firstPacket.getNumOfChunks());
                m_receiveTotalBytes = static_cast<int>(firstPacket.getNumOfBytes());
                emit firstSendPacketReceived(m_receiveTotalChunks, m_receiveTotalBytes);
                break;
            }
            case LoRaProtocol::PacketType::Data: {
                // DataSendPacket - receiver handles this
                LoRaProtocol::DataSendPacket<uint32_t> dataPacket;
                dataPacket.fromQBA(packet);
                uint32_t chunkNum = dataPacket.getNumOfChank();
                QByteArray chunkData = dataPacket.getData();
                // Store in receive buffer
                m_receiveBuffer[chunkNum] = chunkData;
                // Emit progress signal
                int receivedBytes = m_receiveBuffer.size() * LoRaProtocol::PACKET_DATA_SIZE;
                emit packetProgress(receivedBytes, m_receiveTotalBytes);
                emit dataPacketReceived(chunkNum, chunkData);
                break;
            }
            case LoRaProtocol::PacketType::RequestMissings: {
                // RequestMissingsPacket - sender handles this
                LoRaProtocol::RequestMissingsPacket<uint32_t> missingPacket;
                missingPacket.fromQBA(packet);
                std::vector<uint32_t> missingChunksVec = missingPacket.getMissingChunks();
                QVector<uint32_t> missingChunks;
                for (const auto& chunk : missingChunksVec) {
                    missingChunks.append(chunk);
                }
                emit requestMissingsReceived(missingChunks);
                break;
            }
            case LoRaProtocol::PacketType::EndSend: {
                // EndSendPacket - receiver handles this
                emit endSendReceived();
                break;
            }
            case LoRaProtocol::PacketType::ReadyToReceive: {
                // ReadyToReceivePacket - sender handles this
                // Note: In the new continuous send flow, all chunks are sent in onEnterSendAll()
                // without waiting for ReadyToReceivePacket. This packet is received from the receiver
                // after the FirstSendPacket, but we don't need to wait for it to send data chunks.
                m_sendTimeoutTimer->stop();
                emit readyToReceiveReceived();
                break;
            }
            case LoRaProtocol::PacketType::AbortReceiving: {
                // Already handled above
                break;
            }
        }
    }
}

void LoRaUsbFastAdapter_E22_400T22U::setupReceiveMachine()
{
    // Create receive state machine
    m_receive = new QStateMachine(this);

    // Create states
    m_rConnected = new QState(m_receive);
    m_rFirstReceive = new QState(m_receive);
    m_rReceivePackets = new QState(m_receive);
    m_rReceiveMissingMsg = new QState(m_receive);

    // Set initial state
    m_receive->setInitialState(m_rConnected);

    // Set up state entry handlers
    connect(m_rConnected, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterRConnected);
    connect(m_rFirstReceive, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterRFirstReceive);
    connect(m_rReceivePackets, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterRReceivePackets);
    connect(m_rReceiveMissingMsg, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterRReceiveMissingMsg);

    // Set up state exit handlers
    connect(m_rFirstReceive, &QState::exited, this, [this]() {
        m_receiveTimeoutTimer->stop();
    });
    connect(m_rReceivePackets, &QState::exited, this, [this]() {
        m_receiveTimeoutTimer->stop();
    });
    connect(m_rReceiveMissingMsg, &QState::exited, this, [this]() {
        m_receiveTimeoutTimer->stop();
    });

    // m_rConnected -> m_rFirstReceive (when FirstSendPacket received)
    m_rConnected->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::firstSendPacketReceived, m_rFirstReceive);

    // m_rFirstReceive -> m_rReceivePackets (when DataSendPacket received)
    m_rFirstReceive->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::dataPacketReceived, m_rReceivePackets);

    // m_rFirstReceive -> m_rConnected (when timeout and retry count >= MAX_RETRIES)
    // Use a custom signal for timeout handling
    connect(m_receiveTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_receive->configuration().contains(m_rFirstReceive)) {
            emit error("Timeout in first receive state, aborting");
            sendAbortPacket();
            m_receiveTimeoutTimer->stop();  // Stop timer immediately to prevent repeated aborts
            m_receive->postEvent(new QEvent(TransitionToRConnectedEvent));
        }
    });

    // m_rReceivePackets -> m_rReceivePackets (DataSendPacket received - stay in state)
    m_rReceivePackets->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::dataPacketReceived, m_rReceivePackets);

    // m_rReceivePackets -> m_rReceiveMissingMsg (when EndSendPacket received)
    m_rReceivePackets->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::endSendReceived, m_rReceiveMissingMsg);

    // m_rReceivePackets -> m_rConnected (when timeout and retry count >= MAX_RETRIES)
    connect(m_receiveTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_receive->configuration().contains(m_rReceivePackets)) {
            if (m_receiveRetryCount >= MAX_RETRIES) {
                emit error("Timeout in receive packets state, max retries reached");
                sendAbortPacket();
                m_receiveTimeoutTimer->stop();  // Stop timer immediately to prevent repeated aborts
                m_receive->postEvent(new QEvent(TransitionToRConnectedEvent));
            } else {
                m_receiveRetryCount++;
                m_receiveTimeoutTimer->start(TIMEOUT_MS);
            }
        }
    });

    // m_rReceiveMissingMsg -> m_rConnected (when all chunks received or timeout)
    connect(m_receiveTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_receive->configuration().contains(m_rReceiveMissingMsg)) {
            emit error("Timeout in receive missing message state");
            sendAbortPacket();
            m_receiveTimeoutTimer->stop();  // Stop timer immediately to prevent repeated aborts
            m_receive->postEvent(new QEvent(TransitionToRConnectedEvent));
        }
    });

    // Any state -> m_rConnected (when AbortReceivingPacket received)
    m_rFirstReceive->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_rConnected);
    m_rReceivePackets->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_rConnected);
    m_rReceiveMissingMsg->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_rConnected);
    m_rReceiveMissingMsg->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::packetReceived, m_rConnected);

    // Add custom event transitions
    m_rFirstReceive->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::receiveTimeout, m_rConnected);
    m_rReceivePackets->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::receiveTimeout, m_rConnected);
    m_rReceiveMissingMsg->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::receiveTimeout, m_rConnected);
}

void LoRaUsbFastAdapter_E22_400T22U::setupSendMachine()
{
    // Create send state machine
    m_send = new QStateMachine(this);

    // Create states
    m_sConnected = new QState(m_send);
    m_sStartSending = new QState(m_send);
    m_sSendAll = new QState(m_send);
    m_sSendMissing = new QState(m_send);
    m_sResendMissing = new QState(m_send);

    // Set initial state
    m_send->setInitialState(m_sConnected);

    // Set up state entry handlers
    connect(m_sConnected, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterConnected);
    connect(m_sStartSending, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterStartSending);
    connect(m_sSendAll, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterSendAll);
    connect(m_sSendMissing, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterSendMissing);
    connect(m_sResendMissing, &QState::entered, this, &LoRaUsbFastAdapter_E22_400T22U::onEnterResendMissing);

    // Set up state exit handlers
    connect(m_sStartSending, &QState::exited, this, [this]() {
        m_sendTimeoutTimer->stop();
    });
    // Note: m_sSendAll no longer uses the timeout timer, so no stop needed on exit
    connect(m_sSendMissing, &QState::exited, this, [this]() {
        m_sendTimeoutTimer->stop();
    });
    connect(m_sResendMissing, &QState::exited, this, [this]() {
        m_sendTimeoutTimer->stop();
    });

    // m_sConnected -> m_sStartSending (when startSending is emitted)
    m_sConnected->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::startSending, m_sStartSending);

    // m_sStartSending -> m_sSendAll (when ReadyToReceivePacket received)
    m_sStartSending->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::readyToReceiveReceived, m_sSendAll);

    // m_sStartSending -> m_sConnected (when timeout and retry count >= MAX_RETRIES)
    connect(m_sendTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_send->configuration().contains(m_sStartSending)) {
            if (m_sendRetryCount >= MAX_RETRIES) {
                emit error("Timeout in start sending state, max retries reached");
                emit packetSent(false);
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("Emitting transitionToConnectedSignal - timeout in start sending");
                emit transitionToConnectedSignal();
            } else {
                m_sendRetryCount++;
                sendFirstPacket();
                m_sendTimeoutTimer->start(TIMEOUT_MS);
            }
        }
    });


    // m_sSendMissing -> m_sConnected (when RequestMissingsPacket with all zeros received)
    // m_sSendMissing -> m_sSendAll (when RequestMissingsPacket with missing chunks received)
    // These are handled by connecting to requestMissingsReceived signal

    // m_sSendMissing -> m_sConnected (when timeout and retry count >= MAX_RETRIES)
    connect(m_sendTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_send->configuration().contains(m_sSendMissing)) {
            if (m_sendRetryCount >= MAX_RETRIES) {
                emit error("Timeout in send missing state, max retries reached");
                emit packetSent(false);
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("Emitting transitionToConnectedSignal - timeout in send missing");
                emit transitionToConnectedSignal();
            } else {
                m_sendRetryCount++;
                sendEndPacket();
                m_sendTimeoutTimer->start(TIMEOUT_MS);
            }
        }
    });

    // m_sResendMissing -> m_sSendMissing (when all missing chunks sent)
    // m_sResendMissing -> m_sResendMissing (more missing chunks to send)
    // This is handled in onEnterResendMissing

    // m_sResendMissing -> m_sConnected (when timeout and retry count >= MAX_RETRIES)
    connect(m_sendTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_send->configuration().contains(m_sResendMissing)) {
            if (m_sendRetryCount >= MAX_RETRIES) {
                emit error("Timeout in resend missing state, max retries reached");
                emit packetSent(false);
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("Emitting transitionToConnectedSignal - timeout in resend missing");
                emit transitionToConnectedSignal();
            } else {
                m_sendRetryCount++;
                // Resend current missing chunk (don't increment index)
                sendDataPacket(m_missingChunksToResend[m_resendMissingIndex]);
                m_sendTimeoutTimer->start(TIMEOUT_MS);
            }
        }
    });

    // Any state -> m_sConnected (when AbortReceivingPacket received)
    m_sStartSending->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_sConnected);
    m_sSendAll->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_sConnected);
    m_sSendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_sConnected);
    m_sResendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::abortReceivingReceived, m_sConnected);

    // Connect requestMissingsReceived signal to handle transitions from m_sSendAll and m_sSendMissing
    connect(this, &LoRaUsbFastAdapter_E22_400T22U::requestMissingsReceived, this, [this](const QVector<uint32_t>& missingChunks) {
        // Handle in SendAll state (RequestMissings may arrive before delayed transition to SendMissing)
        if (m_send->configuration().contains(m_sSendAll)) {
            // Check if vector is empty (no missing packets) or ALL elements are zero (all data received)
            bool allReceived = false;
            if (missingChunks.isEmpty()) {
                // Empty vector means no missing packets
                allReceived = true;
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("RequestMissings received with empty vector - transitioning to Connected");
            } else {
                // Check if ALL elements are zero (all data received)
                allReceived = true;
                for (const auto& chunk : missingChunks) {
                    if (chunk != 0) {
                        allReceived = false;
                        break;
                    }
                }
                if (allReceived) {
                    qDebug() << typeid(*this).name()
                             << __PRETTY_FUNCTION__
                             << QString("RequestMissings received with all zeros - transitioning to Connected");
                }
            }
            if (allReceived) {
                // All data received, transition to Connected
                emit packetSent(true);
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("Emitting transitionToConnectedSignal - all data received in SendAll");
                emit transitionToConnectedSignal();
            } else {
                // Missing chunks, need to resend them
                m_missingChunksToResend.clear();
                for (const auto& chunk : missingChunks) {
                    if (chunk != 0) {
                        m_missingChunksToResend.append(static_cast<int>(chunk));
                    }
                }
                // Set up for resending missing chunks
                m_resendMissingIndex = 0;
                m_resendingMissing = true;
                // Transition to ResendMissing state
                m_send->postEvent(new QEvent(TransitionToResendMissingEvent));
            }
        }
        // Also handle in SendMissing state (normal flow)
        else if (m_send->configuration().contains(m_sSendMissing)) {
            // Check if vector is empty (no missing packets) or ALL elements are zero (all data received)
            bool allReceived = false;
            if (missingChunks.isEmpty()) {
                // Empty vector means no missing packets
                allReceived = true;
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("RequestMissings received with empty vector - transitioning to Connected");
            } else {
                // Check if ALL elements are zero (all data received)
                allReceived = true;
                for (const auto& chunk : missingChunks) {
                    if (chunk != 0) {
                        allReceived = false;
                        break;
                    }
                }
                if (allReceived) {
                    qDebug() << typeid(*this).name()
                             << __PRETTY_FUNCTION__
                             << QString("RequestMissings received with all zeros - transitioning to Connected");
                }
            }
            if (allReceived) {
                // All data received, transition to Connected
                emit packetSent(true);
                qDebug() << typeid(*this).name()
                         << __PRETTY_FUNCTION__
                         << QString("Emitting transitionToConnectedSignal - all data received in SendMissing");
                emit transitionToConnectedSignal();
            } else {
                // Missing chunks, need to resend them
                m_missingChunksToResend.clear();
                for (const auto& chunk : missingChunks) {
                    if (chunk != 0) {
                        m_missingChunksToResend.append(static_cast<int>(chunk));
                    }
                }
                // Set up for resending missing chunks
                m_resendMissingIndex = 0;
                m_resendingMissing = true;
                // Transition to ResendMissing state
                m_send->postEvent(new QEvent(TransitionToResendMissingEvent));
            }
        }
    });

    // Add custom event transitions for internal state transitions
    m_sSendAll->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::sendTimeout, m_sConnected);
    m_sStartSending->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::sendTimeout, m_sConnected);
    m_sSendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::sendTimeout, m_sConnected);
    m_sResendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::sendTimeout, m_sConnected);

    // Add signal-based transitions to Connected state
    m_sStartSending->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::transitionToConnectedSignal, m_sConnected);
    m_sSendAll->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::transitionToConnectedSignal, m_sConnected);
    m_sSendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::transitionToConnectedSignal, m_sConnected);
    m_sResendMissing->addTransition(this, &LoRaUsbFastAdapter_E22_400T22U::transitionToConnectedSignal, m_sConnected);
}

// Send state entry handlers

void LoRaUsbFastAdapter_E22_400T22U::onEnterConnected()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Reset all variables, stop timer
    m_sendChunks.clear();
    m_receiveBuffer.clear();
    m_packetBuffer.clear();
    m_currentChunkIndex = 0;
    m_totalChunks = 0;
    m_totalBytes = 0;
    m_missingChunksToResend.clear();
    m_resendingMissing = false;
    m_sendRetryCount = 0;
    m_sendTimeoutTimer->stop();
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterStartSending()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Send FirstSendPacket with total chunks and total bytes, start timeout timer, reset retry count
    sendFirstPacket();
    m_sendRetryCount = 0;
    m_sendTimeoutTimer->start(TIMEOUT_MS);
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterSendAll()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
             << QString("Switched to mode %1, sending all chunks continuously").arg(__PRETTY_FUNCTION__);

    // Send ALL data packets in a continuous loop without waiting for ReadyToReceivePacket
    while (m_currentChunkIndex < m_sendChunks.size()) {
        sendDataPacket(m_currentChunkIndex);
        m_currentChunkIndex++;
    }

    // After all chunks sent, send EndSendPacket
    sendEndPacket();

    // Small delay to allow the state machine to process the transition event
    // before the RequestMissingsPacket is received and handled
    QTimer::singleShot(10, this, [this]() {
        m_send->postEvent(new QEvent(TransitionToSendMissingEvent));
    });
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterSendMissing()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Send EndSendPacket, start timeout timer, reset retry count
    sendEndPacket();
    m_sendRetryCount = 0;
    m_sendTimeoutTimer->start(TIMEOUT_MS);
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterResendMissing()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Send the current missing chunk from m_missingChunksToResend
    if (m_resendMissingIndex < m_missingChunksToResend.size()) {
        int chunkIndex = m_missingChunksToResend[m_resendMissingIndex];
        sendDataPacket(chunkIndex);
        m_sendRetryCount = 0;
        m_sendTimeoutTimer->start(TIMEOUT_MS);
        // Move to next missing chunk
        m_resendMissingIndex++;

        // Check if all missing chunks have been sent
        if (m_resendMissingIndex >= m_missingChunksToResend.size()) {
            // All missing chunks sent, transition back to SendMissing state
            m_resendingMissing = false;
            m_missingChunksToResend.clear();
            m_send->postEvent(new QEvent(TransitionToSendMissingEvent));
        }
    }
}

// Receive state entry handlers

void LoRaUsbFastAdapter_E22_400T22U::onEnterRConnected()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Clear m_receiveBuffer, m_packetBuffer, stop timer
    m_receiveBuffer.clear();
    m_packetBuffer.clear();
    m_receiveTimeoutTimer->stop();
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterRFirstReceive()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Send ReadyToReceivePacket, start timeout timer, reset retry count
    sendReadyPacket();
    m_receiveRetryCount = 0;
    m_receiveTimeoutTimer->start(TIMEOUT_MS);
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterRReceivePackets()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Start timeout timer
    // The actual packet storage is handled in onReadyRead via dataPacketReceived signal
    m_receiveTimeoutTimer->start(TIMEOUT_MS);
}

void LoRaUsbFastAdapter_E22_400T22U::onEnterRReceiveMissingMsg()
{
    qDebug() << typeid(*this).name()
    << __PRETTY_FUNCTION__
    << QString("Switched to mode %1").arg(__PRETTY_FUNCTION__);

    // Check if all chunks received, prepare RequestMissingsPacket, send it, start timeout timer, reset retry count
    QVector<uint32_t> missingChunks = getMissingChunks();
    if (missingChunks.isEmpty()) {
        // All chunks received, send RequestMissingsPacket with all zeros
        QVector<uint32_t> emptyMissing;
        for (int i = 0; i < LoRaProtocol::MISSING_CHUNK_NUMS; ++i) {
            emptyMissing.append(0);
        }
        sendMissingPacket(emptyMissing);
        // Emit packetReceived signal with reassembled data
        QByteArray receivedData = reassembleData();
        emit packetReceived(receivedData);
        // Transition to Connected
        m_receive->postEvent(new QEvent(TransitionToRConnectedEvent));

    } else {
        // Missing chunks, send RequestMissingsPacket with missing chunk numbers
        sendMissingPacket(missingChunks);
        m_receiveRetryCount = 0;
        m_receiveTimeoutTimer->start(TIMEOUT_MS);
        // Transition to ReceivePackets to receive missing chunks
        m_receive->postEvent(new QEvent(TransitionToRReceivePacketsEvent));
    }
}

// Helper methods for packet transmission

void LoRaUsbFastAdapter_E22_400T22U::sendFirstPacket()
{
    LoRaProtocol::FirstSendPacket<uint32_t> packet;
    packet.setNumOfChunks(m_totalChunks);
    packet.setNumOfBytes(m_totalBytes);
    QByteArray data = packet.toQBa();
    qint64 sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent first packet, writed %1 bytes").arg(sent);
}

void LoRaUsbFastAdapter_E22_400T22U::sendDataPacket(int chunkIndex)
{
    if (chunkIndex < 0 || chunkIndex >= m_sendChunks.size()) {
        return;
    }

    LoRaProtocol::DataSendPacket<uint32_t> packet;
    packet.setNumOfChank(static_cast<uint32_t>(chunkIndex));
    packet.setData(m_sendChunks[chunkIndex]);
    QByteArray data = packet.toQBa();
    auto sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent %1 chunk, writed %2 bytes").arg(chunkIndex).arg(sent);

    // Emit progress signal
    int sentBytes = chunkIndex * LoRaProtocol::PACKET_DATA_SIZE;
    emit packetSendProgress(sentBytes, m_totalBytes);
}

void LoRaUsbFastAdapter_E22_400T22U::sendEndPacket()
{
    LoRaProtocol::EndSendPacket<uint32_t> packet;
    packet.setNumOfChunks(m_totalChunks);
    QByteArray data = packet.toQBa();
    auto sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent end packet, total chunks %1, writed %2 bytes").arg(m_totalChunks).arg(sent);
}

void LoRaUsbFastAdapter_E22_400T22U::sendReadyPacket()
{
    LoRaProtocol::ReadyToReceivePacket<uint32_t> packet;
    QByteArray data = packet.toQBa();
    auto sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent ready receive packet, writed %1 bytes").arg(sent);
}

void LoRaUsbFastAdapter_E22_400T22U::sendMissingPacket(const QVector<uint32_t>& missingChunks)
{
    LoRaProtocol::RequestMissingsPacket<uint32_t> packet;
    std::vector<uint32_t> missingVec;
    for (const auto& chunk : missingChunks) {
        missingVec.push_back(chunk);
    }
    // Pad with zeros if less than MISSING_CHUNK_NUMS
    while (missingVec.size() < static_cast<size_t>(LoRaProtocol::MISSING_CHUNK_NUMS)) {
        missingVec.push_back(0);
    }
    packet.setMissingChunks(missingVec);
    QByteArray data = packet.toQBa();
    auto sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent missing packet, missing chunks count %1, writed %2 bytes").arg(missingChunks.size()).arg(sent);
}

void LoRaUsbFastAdapter_E22_400T22U::sendAbortPacket()
{
    LoRaProtocol::AbortReceivingPacket<uint32_t> packet;
    QByteArray data = packet.toQBa();
    auto sent = m_serial->write(data);
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Sent abort packet, writed %1 bytes").arg(sent);
}

bool LoRaUsbFastAdapter_E22_400T22U::checkAllChunksReceived() const
{
    return m_receiveBuffer.size() == m_receiveTotalChunks;
}

QVector<uint32_t> LoRaUsbFastAdapter_E22_400T22U::getMissingChunks() const
{
    QVector<uint32_t> missing;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_receiveTotalChunks); ++i) {
        if (!m_receiveBuffer.contains(i)) {
            missing.append(i);
        }
    }
    return missing;
}

QByteArray LoRaUsbFastAdapter_E22_400T22U::reassembleData() const
{
    QByteArray result;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_receiveTotalChunks); ++i) {
        if (m_receiveBuffer.contains(i)) {
            result.append(m_receiveBuffer[i]);
        }
    }
    // Trim to actual size (last chunk may have padding)
    result.resize(m_receiveTotalBytes);
    return result;
}

void LoRaUsbFastAdapter_E22_400T22U::handleSendTimeout()
{
    // Timeout handling is now done in the state machine transitions
}

void LoRaUsbFastAdapter_E22_400T22U::handleReceiveTimeout()
{
    // Timeout handling is now done in the state machine transitions
}

void LoRaUsbFastAdapter_E22_400T22U::transitionToConnected()
{
    // Transition to Connected state in send machine
    qDebug() << typeid(*this).name()
             << __PRETTY_FUNCTION__
             << QString("Emitting transitionToConnectedSignal");
    emit transitionToConnectedSignal();
}

void LoRaUsbFastAdapter_E22_400T22U::transitionToRConnected()
{
    // Transition to Connected state in receive machine
    m_receive->postEvent(new QEvent(TransitionToRConnectedEvent));
}

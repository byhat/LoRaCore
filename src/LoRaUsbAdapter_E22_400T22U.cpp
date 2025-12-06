#include "LoRaUsbAdapter_E22_400T22U.hpp"
#include <QDebug>

LoRaUsbAdapter_E22_400T22U::LoRaUsbAdapter_E22_400T22U(std::shared_ptr<QSerialPort> serial,
                                                       QObject *parent)
    : QObject(parent)
    , m_serial(serial)
{
    if (!m_serial) {
        qWarning() << "LoRaUsbAdapter_E22_400T22U: QSerialPort is null!";
        return;
    }

    connect(m_serial.get(), &QSerialPort::readyRead, this, &LoRaUsbAdapter_E22_400T22U::onReadyRead);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &LoRaUsbAdapter_E22_400T22U::onSendTimeout);
}

quint8 LoRaUsbAdapter_E22_400T22U::crc8(const QByteArray &data) {
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

QByteArray LoRaUsbAdapter_E22_400T22U::makeFrame(FrameType type, quint8 seq, quint8 total,
                                            const QByteArray &payload) {
    const int payloadLen = qMin(payload.size(), 26);
    QByteArray header;
    header.append(static_cast<quint8>(type));
    header.append(seq);
    header.append(total);
    header.append(static_cast<quint8>(payloadLen));

    QByteArray data = header + payload.left(payloadLen);
    data.append(crc8(data));
    return data;
}

bool LoRaUsbAdapter_E22_400T22U::parseFrame(const QByteArray &raw, FrameType &type,
                                       quint8 &seq, quint8 &total, QByteArray &payload) {
    if (raw.size() < 5) return false;

    const quint8 len = static_cast<quint8>(raw[3]);
    if (raw.size() < 5 + len) return false;

    QByteArray headerAndData = raw.left(4 + len);
    quint8 expectedCrc = crc8(headerAndData);
    quint8 actualCrc = static_cast<quint8>(raw[4 + len]);

    if (expectedCrc != actualCrc) {
        emit error("CRC mismatch");
        return false;
    }

    type = static_cast<FrameType>(static_cast<quint8>(raw[0]));
    seq = static_cast<quint8>(raw[1]);
    total = static_cast<quint8>(raw[2]);
    payload = raw.mid(4, len);
    return true;
}

void LoRaUsbAdapter_E22_400T22U::sendPacket(const QByteArray &data) {
    if (!m_serial || !m_serial->isOpen()) {
        emit error("Serial port not open");
        emit packetSent(false);
        return;
    }

    m_chunks.clear();
    const int chunkSize = 26;
    const int total = (data.size() + chunkSize - 1) / chunkSize;

    m_totalPacketBytes = data.size();
    for (int i = 0; i < total; ++i) {
        int start = i * chunkSize;
        int len = qMin(chunkSize, data.size() - start);
        m_chunks.append({static_cast<quint8>(i), static_cast<quint8>(total), data.mid(start, len)});
    }

    m_currentChunkIndex = -1;
    m_retries = 0;
    m_sentBytes = 0;
    sendChunk(0);
}

void LoRaUsbAdapter_E22_400T22U::sendChunk(int index) {
    if (index < 0 || index >= m_chunks.size()) return;

    m_currentChunkIndex = index;
    const auto &chunk = m_chunks[index];

    QByteArray frame = makeFrame(FrameType::DATA, chunk.seq, chunk.total, chunk.payload);
    if (frame.size() > 32) {
        emit error("Frame too large!");
        return;
    }

    qint64 written = m_serial->write(frame);
    if (written != frame.size()) {
        emit error("Serial write failed");
        emit packetSent(false);
        resetSendState();
        return;
    }
    m_serial->waitForBytesWritten(100);

    m_timer.start(TIMEOUT_MS);
}

void LoRaUsbAdapter_E22_400T22U::onSendTimeout() {
    if (m_currentChunkIndex < 0) return;

    m_retries++;
    if (m_retries > MAX_RETRIES) {
        emit error("Max retries exceeded");
        emit packetSent(false);
        resetSendState();
        return;
    }

    sendChunk(m_currentChunkIndex);
}

void LoRaUsbAdapter_E22_400T22U::onReadyRead() {
    static QByteArray buffer;
    buffer.append(m_serial->readAll());

    while (buffer.size() >= 5) {
        quint8 len = static_cast<quint8>(buffer[3]);
        int frameSize = 5 + len;
        if (buffer.size() < frameSize) break;

        QByteArray frame = buffer.left(frameSize);
        buffer = buffer.mid(frameSize);

        FrameType type;
        quint8 seq, total;
        QByteArray payload;
        if (!parseFrame(frame, type, seq, total, payload)) {
            continue;
        }

        switch (type) {
        case FrameType::DATA: {
            if (total == 0) {
                emit error("Invalid total=0 in DATA");
                break;
            }

            if (m_recvState.total == 0) {
                m_recvState.total = total;
                m_recvState.expectedSize = -1;
            } else if (m_recvState.total != total) {
                resetReceiveState();
                m_recvState.total = total;
                m_recvState.expectedSize = -1;
            }

            QByteArray ack = makeFrame(FrameType::ACK, seq, total);
            m_serial->write(ack);
            m_serial->waitForBytesWritten(50);

            if (!m_recvState.chunks.contains(seq)) {
                m_recvState.chunks[seq] = payload;
                m_recvState.receivedCount++;

                int receivedBytes = 0;
                for (const auto &ch : m_recvState.chunks) {
                    receivedBytes += ch.size();
                }

                if (m_recvState.expectedSize == -1) {
                    if (seq == total - 1) {
                        m_recvState.expectedSize = total * 26;
                    } else {
                        m_recvState.expectedSize = total * 26;
                    }
                }

                if (m_recvState.receivedCount == m_recvState.total) {
                    int exactSize = 0;
                    for (quint8 i = 0; i < total; ++i) {
                        if (m_recvState.chunks.contains(i)) {
                            exactSize += m_recvState.chunks[i].size();
                        }
                    }
                    m_recvState.expectedSize = exactSize;
                }

                int totalBytes = (m_recvState.expectedSize == -1) ? total * 26 : m_recvState.expectedSize;

                emit packetProgress(receivedBytes, totalBytes);
            }

            if (m_recvState.receivedCount == m_recvState.total && !m_recvState.packetAckSent) {
                QByteArray full;
                int exactSize = 0;
                for (quint8 i = 0; i < m_recvState.total; ++i) {
                    if (m_recvState.chunks.contains(i)) {
                        const QByteArray &ch = m_recvState.chunks[i];
                        full.append(ch);
                        exactSize += ch.size();
                    } else {
                        emit error("Missing chunk despite count match");
                        return;
                    }
                }

                m_recvState.expectedSize = exactSize;

                QByteArray packAck = makeFrame(FrameType::PACKET_ACK, 0, 0);
                m_serial->write(packAck);
                m_serial->waitForBytesWritten(50);
                m_recvState.packetAckSent = true;

                emit packetProgress(exactSize, exactSize);
                emit packetReceived(full);
                QTimer::singleShot(2000, this, &LoRaUsbAdapter_E22_400T22U::resetReceiveState);
            }
            break;
        }

        case FrameType::ACK: {
            if (m_currentChunkIndex >= 0 && seq == m_chunks[m_currentChunkIndex].seq) {
                m_timer.stop();
                m_retries = 0;

                if (m_currentChunkIndex >= 0 && m_currentChunkIndex < m_chunks.size()) {
                    m_sentBytes += m_chunks[m_currentChunkIndex].payload.size();
                    emit packetSendProgress(m_sentBytes, m_totalPacketBytes);
                }

                if (m_currentChunkIndex + 1 < m_chunks.size()) {
                    sendChunk(m_currentChunkIndex + 1);
                } else {
                    emit packetSendProgress(m_totalPacketBytes, m_totalPacketBytes);
                    emit packetSent(true);
                    resetSendState();
                }
            }
            break;
        }

        case FrameType::PACKET_ACK: {
            if (m_currentChunkIndex >= 0 && m_currentChunkIndex == m_chunks.size() - 1) {
                m_timer.stop();
                emit packetSendProgress(m_totalPacketBytes, m_totalPacketBytes);
                emit packetSent(true);
                resetSendState();
            }
            break;
        }

        default:
            emit error("Unknown frame type");
            break;
        }
    }
}

void LoRaUsbAdapter_E22_400T22U::resetSendState() {
    m_chunks.clear();
    m_currentChunkIndex = -1;
    m_retries = 0;
    m_timer.stop();
}

void LoRaUsbAdapter_E22_400T22U::resetReceiveState() {
    m_recvState = PacketReassembly{};
}

#pragma once

#include <memory>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>
#include <QQueue>
#include <QHash>

class LoRaUsbAdapter_E22_400T22U : public QObject
{
    Q_OBJECT

public:
    enum class FrameType : quint8 {
        DATA = 0x10,
        ACK  = 0x20,
        NACK = 0x30,
        PACKET_ACK = 0x50
    };

    explicit LoRaUsbAdapter_E22_400T22U(std::shared_ptr<QSerialPort> serial,
                                        QObject *parent = nullptr);
    ~LoRaUsbAdapter_E22_400T22U() override = default;

    void sendPacket(const QByteArray &data);

signals:
    void packetSent(bool success);
    void packetReceived(const QByteArray &data);
    void error(const QString &msg);
    void packetProgress(int receivedBytes, int totalBytes);
    void packetSendProgress(int sentBytes, int totalBytes);

private slots:
    void onReadyRead();
    void onSendTimeout();

private:
    struct Chunk {
        quint8 seq = 0;
        quint8 total = 0;
        QByteArray payload;
    };

    int m_totalPacketBytes = 0;
    int m_sentBytes = 0;

    std::shared_ptr<QSerialPort> m_serial;
    QTimer m_timer;

    QList<Chunk> m_chunks;
    int m_currentChunkIndex = -1;
    int m_retries = 0;
    static constexpr int MAX_RETRIES = 5;
    static constexpr int TIMEOUT_MS = 1000;

    struct PacketReassembly {
        int total = 0;
        int receivedCount = 0;
        int expectedSize = -1;
        QHash<quint8, QByteArray> chunks;
        bool packetAckSent = false;
    };
    PacketReassembly m_recvState;

    QByteArray makeFrame(FrameType type, quint8 seq, quint8 total, const QByteArray &payload = {});
    bool parseFrame(const QByteArray &raw, FrameType &type, quint8 &seq, quint8 &total, QByteArray &payload);
    static quint8 crc8(const QByteArray &data);
    void sendChunk(int index);
    void resetSendState();
    void resetReceiveState();
};

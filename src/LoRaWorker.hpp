#pragma once

#include <memory>
#include <QSerialPort>
#include "LoRaUsbAdapter_E22_400T22U.hpp"

class LoRaWorker : public QObject
{
    Q_OBJECT
public:
    explicit LoRaWorker(QObject *parent = nullptr);
    ~LoRaWorker() override;

public slots:
    void openPort(const QString &portName, qint32 baud = 9600);
    void closePort();
    void sendPacket(const QByteArray &data);

signals:
    void portOpened(bool ok, const QString &error = {});
    void packetSent(bool success);
    void packetSendProgress(int sentBytes, int totalBytes);
    void packetReceived(const QByteArray &data);
    void packetReceiveProgress(int receivedBytes, int totalBytes);
    void errorOccurred(const QString &msg);

private:
    std::shared_ptr<QSerialPort> m_serial;
    std::unique_ptr<LoRaUsbAdapter_E22_400T22U> m_transport;
};

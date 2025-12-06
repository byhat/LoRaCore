#include "LoRaWorker.hpp"

LoRaWorker::LoRaWorker(QObject *parent) :
    QObject(parent)
    , m_serial { new QSerialPort(this) }
    , m_transport { new LoRaUsbAdapter_E22_400T22U(m_serial, this) }
{
}

LoRaWorker::~LoRaWorker() {
    closePort();
}

void LoRaWorker::openPort(const QString &portName, qint32 baud) {
    if (m_serial == nullptr) {
        emit errorOccurred("Port already open");
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit portOpened(false, m_serial->errorString());
        m_serial = nullptr;
        return;
    }


    connect(m_transport.get(), &LoRaUsbAdapter_E22_400T22U::packetSent,
            this, &LoRaWorker::packetSent);
    connect(m_transport.get(), &LoRaUsbAdapter_E22_400T22U::packetReceived,
            this, &LoRaWorker::packetReceived);
    connect(m_transport.get(), &LoRaUsbAdapter_E22_400T22U::packetProgress,
            this, &LoRaWorker::packetReceiveProgress);
    connect(m_transport.get(), &LoRaUsbAdapter_E22_400T22U::packetSendProgress,
            this, &LoRaWorker::packetSendProgress);
    connect(m_transport.get(), &LoRaUsbAdapter_E22_400T22U::error,
            this, &LoRaWorker::errorOccurred);

    emit portOpened(true);
}

void LoRaWorker::closePort() {
    if (m_serial) {
        m_serial->close();
    }
}

void LoRaWorker::sendPacket(const QByteArray &data) {
    if (m_transport) {
        m_transport->sendPacket(data);
    } else {
        emit errorOccurred("Transport not ready");
    }
}

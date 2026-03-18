#include "LoRaWorker.hpp"

LoRaWorker::LoRaWorker(QObject *parent) :
    QObject(parent)
    , m_serial { new QCrossPlatformSerialPort(this) }
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
    m_serial->setDataBits(QCrossPlatformDataBits::Data8);
    m_serial->setParity(QCrossPlatformParity::NoParity);
    m_serial->setStopBits(QCrossPlatformStopBits::OneStop);
    m_serial->setFlowControl(QCrossPlatformFlowControl::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QString errorMsg;
        auto err = m_serial->error();
        if (err != QCrossPlatformSerialPortError::NoError) {
            errorMsg = QString("Serial port error: %1").arg(static_cast<int>(err));
        } else {
            errorMsg = "Failed to open serial port";
        }
        emit portOpened(false, errorMsg);
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

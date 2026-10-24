#include "LoRaWorker.hpp"

LoRaWorker::LoRaWorker(QObject *parent) :
    QObject(parent)
    , m_serial { std::make_shared<cs::SerialPort>() }
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

    m_serial->setPortName(portName.toStdString());
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(cs::DataBits::Data8);
    m_serial->setParity(cs::Parity::NoParity);
    m_serial->setStopBits(cs::StopBits::OneStop);
    m_serial->setFlowControl(cs::FlowControl::NoFlowControl);

    if (!m_serial->open(cs::OpenMode::ReadWrite)) {
        QString errorMsg = QString("Failed to open serial port: %1")
                               .arg(QString::fromStdString(m_serial->errorString()));
        emit portOpened(false, errorMsg);
        m_serial = nullptr;
        return;
    }

    // Drop bytes that piled up in the driver while the port was closed
    // (QSerialPort flushed on open; cs::SerialPort does not). Stale bytes
    // would desync the frame parser and stall reception until reconnect.
    m_serial->clear(cs::Directions::AllDirections);


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

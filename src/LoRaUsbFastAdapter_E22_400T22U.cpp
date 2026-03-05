#include "LoRaUsbFastAdapter_E22_400T22U.hpp"

LoRaUsbFastAdapter_E22_400T22U::LoRaUsbFastAdapter_E22_400T22U(std::shared_ptr<QCrossPlatformSerialPort> serial,
                                                               QObject *parent)
    : QObject(parent)
    , m_serial(serial)
{
    LoRaProtocol::FirstSendPacket<uint32_t> packet;
    packet.setNumOfChunks(925008);
    packet.setNumOfBytes(925004);
    QByteArray arr = packet.toQBa();
    packet.fromQBA(arr);

    QByteArray dat;
    dat.resize(LoRaProtocol::PACKET_DATA_SIZE-2, 22);
    LoRaProtocol::DataSendPacket<uint32_t> data;
    data.setNumOfChank(12);
    data.setData(dat);
    QByteArray data_arr = data.toQBa();
    data.fromQBA(data_arr);
    data_arr = data.getData();
    uint32_t num = data.getNumOfChank();

    LoRaProtocol::EndSendPacket<uint32_t> end;
    end.setNumOfChunks(2342354);
    QByteArray end_arr = end.toQBa();
    end.fromQBA(end_arr);

    LoRaProtocol::ReadyToReceivePacket<uint32_t> ready;
    QByteArray ready_arr = ready.toQBa();

    LoRaProtocol::AbortReceivingPacket<uint32_t> abort;
    QByteArray abort_arr = abort.toQBa();
}

void LoRaUsbFastAdapter_E22_400T22U::sendPacket(const QByteArray &data)
{
    if (condition)
        do1();
        do2();
}

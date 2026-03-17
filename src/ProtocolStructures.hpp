/**
 * @todo use qToLittleEndian
 */
#pragma once

#include <cstdint>
#include <QDebug>
#include <typeinfo>
#include <algorithm>

#include <QByteArray>

namespace LoRaProtocol {

/**
 * @enum FrameType
 * @brief Frame type identifiers for the LoRa protocol
 * @details Defines the different types of frames used in the communication protocol
 */
enum class PacketType : char {
    First,
    Data,
    RequestMissings,
    EndSend,
    ReadyToReceive,
    AbortReceiving
};

static constexpr qsizetype PACKET_BYTES_STATIC_SIZE = 32;
static constexpr qsizetype PACKET_TYPE_POSITION = 0;
static constexpr qsizetype PACKET_TYPE_SIZE = 1;

static constexpr qsizetype CHUNK_NUM_POSITION = PACKET_TYPE_POSITION + PACKET_TYPE_SIZE;
static constexpr qsizetype CHUNK_NUM_BYTES_SIZE = 3;

static constexpr qsizetype PACKETS_BYTES_SIZE_POSITION = CHUNK_NUM_POSITION + CHUNK_NUM_BYTES_SIZE;
static constexpr qsizetype PACKETS_BYTES_SIZE = CHUNK_NUM_BYTES_SIZE;

static constexpr qsizetype PACKET_DATA_SIZE = PACKET_BYTES_STATIC_SIZE - PACKET_TYPE_SIZE - CHUNK_NUM_BYTES_SIZE;
static constexpr qsizetype PACKET_DATA_HEADER_SIZE = PACKET_TYPE_SIZE + CHUNK_NUM_BYTES_SIZE;
static constexpr qsizetype PACKET_DATA_POSITION = CHUNK_NUM_POSITION + CHUNK_NUM_BYTES_SIZE;

static constexpr qsizetype MISSING_CHUNK_NUM_POSITION = PACKET_TYPE_POSITION + PACKET_TYPE_SIZE;
static constexpr qsizetype MISSING_CHUNK_NUMS = (PACKET_BYTES_STATIC_SIZE - PACKET_TYPE_SIZE) / CHUNK_NUM_BYTES_SIZE;


template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct RequestMissingsPacket
{
    void setMissingChunks(std::vector<T> num) {
        missingChunks = num;
    }

    std::vector<T> getMissingChunks() {
        return missingChunks;
    }

    QByteArray toQBa() {
        QByteArray arr;
        arr.resize(PACKET_BYTES_STATIC_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        qsizetype chunksToPack = std::min(static_cast<qsizetype>(missingChunks.size()), MISSING_CHUNK_NUMS);
        for (qsizetype i = 0; i < chunksToPack; i++) {
            T chunkNum = missingChunks[i];
            for (qsizetype j = 0; j < CHUNK_NUM_BYTES_SIZE; j++) {
                // Big-endian: most significant byte first
                char val = static_cast<char>((chunkNum >> ((CHUNK_NUM_BYTES_SIZE - 1 - j) * 8)) & 0xFF);
                arr[MISSING_CHUNK_NUM_POSITION + (i * CHUNK_NUM_BYTES_SIZE) + j] = val;
            }
        }
        // Remaining bytes are already zeroed by resize()

        return arr;
    }

    void fromQBA(const QByteArray &arr) {
        if (arr.size() != PACKET_BYTES_STATIC_SIZE) {
            qDebug() << typeid(*this).name()
            << __PRETTY_FUNCTION__
            << QString("data size must be %1 bytes").arg(PACKET_BYTES_STATIC_SIZE);

            return;
        }

        missingChunks.clear();

        for (qsizetype i = 0; i < MISSING_CHUNK_NUMS; i++) {
            T chunkNum = 0;
            for (qsizetype j = 0; j < CHUNK_NUM_BYTES_SIZE; j++) {
                // Big-endian: most significant byte first
                T val = static_cast<T>(static_cast<unsigned char>(
                    arr[MISSING_CHUNK_NUM_POSITION + (i * CHUNK_NUM_BYTES_SIZE) + j])
                                   );
                chunkNum |= val << ((CHUNK_NUM_BYTES_SIZE - 1 - j) * 8);
            }
            // Stop if we encounter a zero chunk (no more missing chunks)
            if (chunkNum == 0) {
                break;
            }
            missingChunks.push_back(chunkNum);
        }
    }

private:
    static constexpr PacketType type = PacketType::RequestMissings;
    std::vector<T> missingChunks;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct AbortReceivingPacket
{
    QByteArray toQBa() {
        QByteArray arr;
        arr.resize(PACKET_BYTES_STATIC_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        return arr;
    }

private:
    static constexpr PacketType type = PacketType::AbortReceiving;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct ReadyToReceivePacket
{
    QByteArray toQBa() {
        QByteArray arr;
        arr.resize(PACKET_BYTES_STATIC_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        return arr;
    }

private:
    static constexpr PacketType type = PacketType::ReadyToReceive;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct EndSendPacket
{
    void setNumOfChunks(T num) {
        numOfChunks = num;
    }

    T getNumOfChunks() {
        return numOfChunks;
    }

    QByteArray toQBa() {
        QByteArray arr;
        arr.resize(PACKET_BYTES_STATIC_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            char val = static_cast<char>(numOfChunks >> (i * 8) & 0xFF);
            arr[CHUNK_NUM_POSITION + i] = val;
        }

        return arr;
    }

    void fromQBA(const QByteArray &arr) {
        if (arr.size() > PACKET_BYTES_STATIC_SIZE) {
            qDebug() << typeid(*this).name()
            << __PRETTY_FUNCTION__
            << QString("data size must be %1 bytes").arg(PACKET_BYTES_STATIC_SIZE);

            return;
        }

        numOfChunks = 0;

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            T val = static_cast<T>(static_cast<unsigned char>(
                arr[CHUNK_NUM_POSITION + i])
                                   );
            numOfChunks |= val << (i * 8);
        }
    }

private:
    static constexpr PacketType type = PacketType::EndSend;
    uint32_t numOfChunks = 0;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct FirstSendPacket {
    void setNumOfChunks(T num) {
        numOfChunks = num;
    }

    T getNumOfChunks() {
        return numOfChunks;
    }

    void setNumOfBytes(T num) {
        numOfBytes = num;
    }

    T getNumOfBytes() {
        return numOfBytes;
    }

    QByteArray toQBa() {
        QByteArray arr;
        arr.resize(PACKET_BYTES_STATIC_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            char val = static_cast<char>(numOfChunks >> (i * 8) & 0xFF);
            arr[CHUNK_NUM_POSITION + i] = val;
        }

        for (qsizetype i {0}; i < PACKETS_BYTES_SIZE; i++) {
            char val = static_cast<char>(numOfBytes >> (i * 8) & 0xFF);
            arr[PACKETS_BYTES_SIZE_POSITION + i] = val;
        }

        return arr;
    }

    void fromQBA(const QByteArray &arr) {
        if (arr.size() > PACKET_BYTES_STATIC_SIZE) {
            qDebug() << typeid(*this).name()
            << __PRETTY_FUNCTION__
            << QString("data size must be %1 bytes").arg(PACKET_BYTES_STATIC_SIZE);

            return;
        }

        numOfChunks = 0;

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            T val = static_cast<T>(static_cast<unsigned char>(
                                    arr[CHUNK_NUM_POSITION + i])
                                  );
            numOfChunks |= val << (i * 8);
        }

        numOfBytes = 0;

        for (qsizetype i {0}; i < PACKETS_BYTES_SIZE; i++) {
            T val = static_cast<T>(static_cast<unsigned char>(
                arr[PACKETS_BYTES_SIZE_POSITION + i])
                                   );
            numOfBytes |= val << (i * 8);
        }
    }

private:
    static constexpr PacketType type = PacketType::First;
    T numOfChunks = 0;
    T numOfBytes  = 0;
};

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct DataSendPacket {
    void setNumOfChank(T num) {
        numOfChank = num;
    }

    T getNumOfChank() {
        return numOfChank;
    }

    QByteArray getData() {
        return data;
    }

    std::optional<bool> setData(const QByteArray &arr) {
        if (arr.size() > PACKET_DATA_SIZE) {
            qDebug() << typeid(*this).name()
                     << __PRETTY_FUNCTION__
                     << QString("data size must be %1 bytes").arg(PACKET_DATA_SIZE);

            return std::nullopt;
        }

        data.insert(0, arr);
        data.resize(PACKET_DATA_SIZE, 0);

        return true;
    }

    QByteArray toQBa() const {
        QByteArray arr;
        arr.resize(PACKET_DATA_HEADER_SIZE, 0);
        arr[PACKET_TYPE_POSITION] = static_cast<char>(type);

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            char val = static_cast<char>(numOfChank >> (i * 8) & 0xFF);
            arr[CHUNK_NUM_POSITION + i] = val;
        }

        arr.insert(PACKET_DATA_POSITION, data);

        return arr;
    }

    void fromQBA(const QByteArray &arr) {
        numOfChank = 0;

        for (qsizetype i {0}; i < CHUNK_NUM_BYTES_SIZE; i++) {
            decltype(numOfChank) val = static_cast<decltype(numOfChank)>(
                static_cast<unsigned char>(
                    arr[CHUNK_NUM_POSITION + i])
                );
            numOfChank |= val << (i * 8);
        }

        data = arr.mid(PACKET_DATA_POSITION);
    }
private:
    static constexpr PacketType type = PacketType::Data;
    uint32_t numOfChank = 0;
    QByteArray data;
};


};

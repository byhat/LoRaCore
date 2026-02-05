# 📡 LoRaCore

<div align="center">

**Reliable LoRa communication library for Ebyte E22-400T22U USB adapter**

[![CI](https://github.com/byhat/LoRaCore/workflows/CI/badge.svg)](https://github.com/byhat/LoRaCore/actions)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)
[![Qt6](https://img.shields.io/badge/Qt-6-green)](https://www.qt.io)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)]()

</div>

---

## 📖 About

**LoRaCore** is a modern C++/Qt6 library that provides reliable data transmission over the **Ebyte E22-400T22U** LoRa USB adapter. It handles the complexities of LoRa communication with automatic packet fragmentation, acknowledgment protocols, and integrity checking.

### What It Does

LoRaCore abstracts away the low-level details of LoRa communication, allowing you to send and receive data of any size through a simple Qt signal/slot interface. The library automatically manages:

- Packet fragmentation for payloads larger than the radio's MTU
- Automatic retransmission on failure
- Data integrity verification

### Key Features

- ✨ **Automatic packet fragmentation** – Send packets of any size (≤26 bytes per chunk)
- 🔄 **ACK/NACK retransmission protocol** – Reliable delivery with up to 5 retry attempts
- ✅ **CRC-16 integrity checking** – Dallas/Maxim CRC for frame verification
- 🎯 **Qt6 signal/slot interface** – Seamless integration with Qt applications
- 🧪 **Unit tests with gtest** – Comprehensive test coverage

### Use Cases

- Remote sensor networks
- IoT device communication
- Long-range telemetry
- Industrial automation
- Amateur radio projects

---

## 📑 Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Installation](#-installation)
- [Usage](#-usage)
- [Testing](#-testing)
- [API Documentation](#-api-documentation)
- [Contributing](#-contributing)
- [License](#-license)
- [Acknowledgments](#-acknowledgments)

---

## ✨ Features

### Automatic Packet Fragmentation

The library automatically splits large packets into chunks of ≤26 bytes (the E22 module's maximum transmission unit) and reassembles them on the receiving end.

```cpp
// Send 1KB of data - automatically fragmented
QByteArray largeData(1024, 'A');
worker->sendData(largeData);
```

### ACK/NACK Retransmission Protocol

- **Chunk-level ACK** – Each fragment is acknowledged individually
- **Packet-level ACK** – Full packet confirmation after successful reassembly
- **Automatic retry** – Up to 5 attempts with 1-second timeout

### CRC-16 Integrity Checking

Every frame includes a CRC-16 checksum to ensure data integrity during transmission.

### Qt6 Signal/Slot Interface

Designed for Qt applications with asynchronous signal/slot communication:

```cpp
connect(worker, &LoRaWorker::dataReceived, this, [](const QByteArray& data) {
    qDebug() << "Received:" << data;
});
```

### Unit Tests with Google Test

Comprehensive test suite covering core functionality:

- Adapter protocol tests
- Worker signal/slot tests
- Fragmentation and reassembly tests

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Your Application                     │
└────────────────────────────┬────────────────────────────────┘
                             │ Qt Signals/Slots
┌────────────────────────────▼────────────────────────────────┐
│                       LoRaWorker                            │
│  • High-level interface                                     │
│  • Port management                                          │
│  • Signal emission                                          │
└────────────────────────────┬────────────────────────────────┘
                             │ Direct calls
┌────────────────────────────▼────────────────────────────────┐
│              LoRaUsbAdapter_E22_400T22U                     │
│  • Protocol implementation                                  │
│  • Frame formatting/parsing                                 │
│  • ACK/NACK handling                                        │
│  • Fragmentation/reassembly                                 │
└────────────────────────────┬────────────────────────────────┘
                             │ USB Serial
┌────────────────────────────▼────────────────────────────────┐
│              Ebyte E22-400T22U Module                       │
└─────────────────────────────────────────────────────────────┘
```

### Core Classes

| Class | Description |
|-------|-------------|
| [`LoRaWorker`](src/LoRaWorker.hpp) | High-level interface managing serial port communication and emitting Qt signals for received data |
| [`LoRaUsbAdapter_E22_400T22U`](src/LoRaUsbAdapter_E22_400T22U.hpp) | Protocol implementation handling framing, ACK/NACK, fragmentation, and CRC |

---

## 📦 Installation

### Prerequisites

- **CMake** ≥ 3.19
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **Qt6** (Core + SerialPort modules)
- **Google Test** (automatically fetched by CMake)

#### Installing Qt6

<details>
<summary>Click to expand Qt6 installation instructions</summary>

**Ubuntu/Debian:**
```bash
sudo apt install qt6-base-dev qt6-serialport-dev
```

**macOS (Homebrew):**
```bash
brew install qt@6
```

**Windows:**
Download from [Qt Official Website](https://www.qt.io/download-qt-installer)

</details>

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/LoRaCore.git
cd LoRaCore

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .

# (Optional) Install system-wide
sudo cmake --install .
```

### Dependencies

- **Qt6::Core** – Core Qt functionality
- **Qt6::SerialPort** – Serial port communication
- **Google Test** – Unit testing framework (auto-downloaded)

---

## 🚀 Usage

### Basic Example

```cpp
#include <QCoreApplication>
#include <QSerialPortInfo>
#include "LoRaWorker.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Create worker instance
    LoRaWorker worker;

    // Connect to data received signal
    QObject::connect(&worker, &LoRaWorker::dataReceived,
                     [](const QByteArray &data) {
        qDebug() << "Received data:" << data.toHex();
    });

    // Open the USB adapter port
    QString portName = "/dev/ttyUSB0";  // Adjust for your system
    if (worker.openPort(portName)) {
        qDebug() << "Port opened successfully";

        // Send data
        QByteArray message = "Hello LoRa!";
        worker.sendData(message);
    }

    return app.exec();
}
```

### CMake Integration

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

add_executable(my_app main.cpp)

target_link_libraries(my_app
    PRIVATE
        LoRaCore
        Qt6::Core
        Qt6::SerialPort
)

target_include_directories(my_app PRIVATE /path/to/LoRaCore/src)
```

---

## 🧪 Testing

### Build and Run Tests

```bash
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build .

# Run all tests
ctest --output-on-failure

# Or run the test executable directly
./LoRaCoreTests
```

### Test Coverage

The test suite covers:

- ✅ Frame formatting and parsing
- ✅ CRC-16 calculation and verification
- ✅ Packet fragmentation and reassembly
- ✅ ACK/NACK protocol behavior
- ✅ Signal emission on data reception

---

## 📚 API Documentation

### LoRaWorker

High-level interface for LoRa communication.

#### Public Methods

| Method | Description |
|--------|-------------|
| `bool openPort(const QString& portName)` | Opens the serial port connection |
| `void closePort()` | Closes the current connection |
| `bool isOpen() const` | Returns true if port is open |
| `void sendData(const QByteArray& data)` | Sends data over LoRa |

#### Signals

| Signal | Description |
|--------|-------------|
| `void dataReceived(const QByteArray& data)` | Emitted when complete data is received |
| `void errorOccurred(const QString& error)` | Emitted on communication errors |

### LoRaUsbAdapter_E22_400T22U

Low-level protocol implementation.

#### Public Methods

| Method | Description |
|--------|-------------|
| `void sendPacket(const QByteArray& data)` | Sends a packet with fragmentation |
| `void processIncomingData(const QByteArray& data)` | Processes raw serial data |

For detailed API documentation, see the header files:
- [`src/LoRaWorker.hpp`](src/LoRaWorker.hpp)
- [`src/LoRaUsbAdapter_E22_400T22U.hpp`](src/LoRaUsbAdapter_E22_400T22U.hpp)

---

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

### Code Style

- Follow Qt coding conventions
- Use 4 spaces for indentation
- Include comments for complex logic
- Write unit tests for new features

### Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Reporting Issues

Please use the [GitHub Issues](https://github.com/yourusername/LoRaCore/issues) page to report bugs or request features.

---

## 📄 License

This project is licensed under the GNU General Public License v3.0 – see the [LICENSE](LICENSE) file for details.

```
LoRaCore - Reliable LoRa communication library
Copyright (C) 2024

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

---

## 🙏 Acknowledgments

- **[Ebyte](https://www.ebyte.com/en/)** – For the E22-400T22U LoRa module
- **[Qt Framework](https://www.qt.io/)** – For the excellent cross-platform framework
- **[Google Test](https://github.com/google/googletest)** – For the testing framework

---
[⬆ Back to Top](#-loracore)

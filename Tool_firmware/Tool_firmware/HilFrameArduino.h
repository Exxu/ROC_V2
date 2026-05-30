#ifndef HIL_FRAME_ARDUINO_H
#define HIL_FRAME_ARDUINO_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Arduino-compatible version of the HIL Frame protocol.
// Compatible with the PC-side Frame implementation:
//   header = 0x7E
//   end    = 0x7E
//   escape = 0x7D
//   escaped byte = byte ^ 0x20
//   checksum = Fletcher-16 mod 255 over raw payload, appended little-endian.
//
// This class avoids std::vector, boost, dynamic allocation, mutexes and other
// dependencies that are not appropriate for an Arduino Nano / ATmega328P.

template <size_t MAX_PAYLOAD = 64>
class HilFrameArduino
{
public:
  static const uint8_t HEADER = 0x7E;
  static const uint8_t END    = 0x7E;
  static const uint8_t ESCAPE = 0x7D;

  static const size_t MAX_DECODED_WITH_CHECKSUM = MAX_PAYLOAD + 2;
  static const size_t MAX_ENCODED = 1 + 2 * (MAX_PAYLOAD + 2) + 1;

  HilFrameArduino()
  {
    clear();
  }

  void clear()
  {
    data_size_ = 0;
    tx_size_ = 0;
    read_index_ = 0;
  }

  uint8_t* data()
  {
    return data_;
  }

  const uint8_t* data() const
  {
    return data_;
  }

  size_t size() const
  {
    return data_size_;
  }

  const uint8_t* buffer() const
  {
    return tx_buffer_;
  }

  size_t bufferSize() const
  {
    return tx_size_;
  }

  bool setData(const uint8_t* input, size_t n)
  {
    if (input == nullptr && n > 0) {
      return false;
    }
    if (n > MAX_PAYLOAD) {
      return false;
    }
    if (n > 0) {
      memcpy(data_, input, n);
    }
    data_size_ = n;
    read_index_ = 0;
    return true;
  }

  bool addByte(uint8_t value)
  {
    if (data_size_ + 1 > MAX_PAYLOAD) {
      return false;
    }
    data_[data_size_++] = value;
    return true;
  }

  bool addUInt16(uint16_t value)
  {
    if (data_size_ + 2 > MAX_PAYLOAD) {
      return false;
    }
    data_[data_size_++] = static_cast<uint8_t>(value & 0xFF);
    data_[data_size_++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    return true;
  }

  bool addInt32(int32_t value)
  {
    if (data_size_ + 4 > MAX_PAYLOAD) {
      return false;
    }
    uint32_t u = static_cast<uint32_t>(value);
    data_[data_size_++] = static_cast<uint8_t>(u & 0xFF);
    data_[data_size_++] = static_cast<uint8_t>((u >> 8) & 0xFF);
    data_[data_size_++] = static_cast<uint8_t>((u >> 16) & 0xFF);
    data_[data_size_++] = static_cast<uint8_t>((u >> 24) & 0xFF);
    return true;
  }

  bool addFloat(float value)
  {
    if (data_size_ + 4 > MAX_PAYLOAD) {
      return false;
    }
    uint8_t bytes[4];
    memcpy(bytes, &value, 4);
    // Arduino Nano/ATmega328P is little-endian; keep explicit copy for clarity.
    data_[data_size_++] = bytes[0];
    data_[data_size_++] = bytes[1];
    data_[data_size_++] = bytes[2];
    data_[data_size_++] = bytes[3];
    return true;
  }

  bool addBytes(const uint8_t* input, size_t n)
  {
    if (input == nullptr && n > 0) {
      return false;
    }
    if (data_size_ + n > MAX_PAYLOAD) {
      return false;
    }
    if (n > 0) {
      memcpy(data_ + data_size_, input, n);
      data_size_ += n;
    }
    return true;
  }

  bool getByte(uint8_t& value)
  {
    if (read_index_ + 1 > data_size_) {
      return false;
    }
    value = data_[read_index_++];
    return true;
  }

  bool getUInt16(uint16_t& value)
  {
    if (read_index_ + 2 > data_size_) {
      return false;
    }
    value = static_cast<uint16_t>(data_[read_index_]) |
            (static_cast<uint16_t>(data_[read_index_ + 1]) << 8);
    read_index_ += 2;
    return true;
  }

  bool getInt32(int32_t& value)
  {
    if (read_index_ + 4 > data_size_) {
      return false;
    }
    uint32_t u = static_cast<uint32_t>(data_[read_index_]) |
                 (static_cast<uint32_t>(data_[read_index_ + 1]) << 8) |
                 (static_cast<uint32_t>(data_[read_index_ + 2]) << 16) |
                 (static_cast<uint32_t>(data_[read_index_ + 3]) << 24);
    value = static_cast<int32_t>(u);
    read_index_ += 4;
    return true;
  }

  bool getFloat(float& value)
  {
    if (read_index_ + 4 > data_size_) {
      return false;
    }
    uint8_t bytes[4] = {
      data_[read_index_],
      data_[read_index_ + 1],
      data_[read_index_ + 2],
      data_[read_index_ + 3]
    };
    memcpy(&value, bytes, 4);
    read_index_ += 4;
    return true;
  }

  void resetReadIndex()
  {
    read_index_ = 0;
  }

  uint16_t checksum(const uint8_t* input, size_t n) const
  {
    if (input == nullptr || n == 0) {
      return 0;
    }

    uint16_t sum1 = 0;
    uint16_t sum2 = 0;

    for (size_t i = 0; i < n; ++i) {
      sum1 = static_cast<uint16_t>((sum1 + input[i]) % 255);
      sum2 = static_cast<uint16_t>((sum2 + sum1) % 255);
    }

    return static_cast<uint16_t>((sum2 << 8) | sum1);
  }

  bool build()
  {
    tx_size_ = 0;

    if (data_size_ == 0) {
      return false;
    }

    if (!appendTxRaw(HEADER)) {
      return false;
    }

    for (size_t i = 0; i < data_size_; ++i) {
      if (!appendEscaped(data_[i])) {
        return false;
      }
    }

    const uint16_t cksum = checksum(data_, data_size_);
    const uint8_t cksum_low = static_cast<uint8_t>(cksum & 0xFF);
    const uint8_t cksum_high = static_cast<uint8_t>((cksum >> 8) & 0xFF);

    if (!appendEscaped(cksum_low)) {
      return false;
    }
    if (!appendEscaped(cksum_high)) {
      return false;
    }

    return appendTxRaw(END);
  }

  size_t writeTo(Stream& stream)
  {
    if (!build()) {
      return 0;
    }
    const size_t written = stream.write(tx_buffer_, tx_size_);
    stream.flush();
    return written;
  }

  bool readFrom(Stream& stream, uint32_t timeout_ms = 1000)
  {
    clear();

    uint8_t decoded[MAX_DECODED_WITH_CHECKSUM];
    size_t decoded_size = 0;
    bool in_frame = false;
    bool escape_next = false;

    const uint32_t start_time = millis();

    while (static_cast<uint32_t>(millis() - start_time) < timeout_ms) {
      if (!stream.available()) {
        continue;
      }

      const int raw = stream.read();
      if (raw < 0) {
        continue;
      }

      const uint8_t byte = static_cast<uint8_t>(raw);

      if (!in_frame) {
        if (byte == HEADER) {
          in_frame = true;
          escape_next = false;
          decoded_size = 0;
        }
        continue;
      }

      if (escape_next) {
        if (decoded_size >= MAX_DECODED_WITH_CHECKSUM) {
          clear();
          return false;
        }
        decoded[decoded_size++] = static_cast<uint8_t>(byte ^ 0x20);
        escape_next = false;
        continue;
      }

      if (byte == ESCAPE) {
        escape_next = true;
        continue;
      }

      if (byte == END) {
        if (decoded_size < 2) {
          clear();
          return false;
        }

        const size_t payload_size = decoded_size - 2;
        if (payload_size > MAX_PAYLOAD) {
          clear();
          return false;
        }

        const uint16_t received_cksum =
          static_cast<uint16_t>(decoded[payload_size]) |
          (static_cast<uint16_t>(decoded[payload_size + 1]) << 8);

        const uint16_t computed_cksum = checksum(decoded, payload_size);

        if (received_cksum != computed_cksum) {
          clear();
          return false;
        }

        if (payload_size > 0) {
          memcpy(data_, decoded, payload_size);
        }
        data_size_ = payload_size;
        read_index_ = 0;
        return true;
      }

      if (decoded_size >= MAX_DECODED_WITH_CHECKSUM) {
        clear();
        return false;
      }
      decoded[decoded_size++] = byte;
    }

    clear();
    return false;
  }

private:
  bool appendTxRaw(uint8_t byte)
  {
    if (tx_size_ >= MAX_ENCODED) {
      return false;
    }
    tx_buffer_[tx_size_++] = byte;
    return true;
  }

  bool appendEscaped(uint8_t byte)
  {
    if (byte == HEADER || byte == END || byte == ESCAPE) {
      return appendTxRaw(ESCAPE) && appendTxRaw(static_cast<uint8_t>(byte ^ 0x20));
    }
    return appendTxRaw(byte);
  }

  uint8_t data_[MAX_PAYLOAD];
  uint8_t tx_buffer_[MAX_ENCODED];
  size_t data_size_;
  size_t tx_size_;
  size_t read_index_;
};

#endif  // HIL_FRAME_ARDUINO_H

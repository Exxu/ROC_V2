#include "roc_radio_tool_bridge/hil_frame.hpp"

#include <cstring>

namespace roc_radio_tool_bridge
{

void HilFrame::clear()
{
  payload_.clear();
  encoded_.clear();
}

bool HilFrame::addFloat(float value)
{
  appendLittleEndianFloat(payload_, value);
  return true;
}

bool HilFrame::getFloat(std::size_t index, float& value) const
{
  const std::size_t offset = index * sizeof(float);
  return readLittleEndianFloat(payload_, offset, value);
}

const std::vector<uint8_t>& HilFrame::payload() const
{
  return payload_;
}

const std::vector<uint8_t>& HilFrame::encoded() const
{
  return encoded_;
}

bool HilFrame::build()
{
  if (payload_.empty())
    return false;

  encoded_.clear();
  encoded_.reserve(1 + 2 * (payload_.size() + 2) + 1);

  encoded_.push_back(kDelimiter);

  for (const uint8_t byte : payload_)
    appendEscaped(encoded_, byte);

  const uint16_t crc = checksum(payload_.data(), payload_.size());
  appendEscaped(encoded_, static_cast<uint8_t>(crc & 0xFF));
  appendEscaped(encoded_, static_cast<uint8_t>((crc >> 8) & 0xFF));

  encoded_.push_back(kDelimiter);
  return true;
}

bool HilFrame::decodeFromBytes(const std::vector<uint8_t>& encoded_without_delimiters)
{
  std::vector<uint8_t> decoded;
  decoded.reserve(encoded_without_delimiters.size());

  bool escape_next = false;
  for (const uint8_t byte : encoded_without_delimiters)
  {
    if (escape_next)
    {
      decoded.push_back(static_cast<uint8_t>(byte ^ 0x20));
      escape_next = false;
      continue;
    }

    if (byte == kEscape)
    {
      escape_next = true;
      continue;
    }

    decoded.push_back(byte);
  }

  if (escape_next || decoded.size() < 2)
    return false;

  const std::size_t payload_size = decoded.size() - 2;
  const uint16_t received_crc = static_cast<uint16_t>(decoded[payload_size]) |
                                (static_cast<uint16_t>(decoded[payload_size + 1]) << 8);
  const uint16_t computed_crc = checksum(decoded.data(), payload_size);

  if (received_crc != computed_crc)
    return false;

  payload_.assign(decoded.begin(), decoded.begin() + static_cast<std::ptrdiff_t>(payload_size));
  encoded_.clear();
  return true;
}

uint16_t HilFrame::checksum(const uint8_t* data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return 0;

  uint16_t sum1 = 0;
  uint16_t sum2 = 0;

  for (std::size_t i = 0; i < size; ++i)
  {
    sum1 = static_cast<uint16_t>((sum1 + data[i]) % 255);
    sum2 = static_cast<uint16_t>((sum2 + sum1) % 255);
  }

  return static_cast<uint16_t>((sum2 << 8) | sum1);
}

void HilFrame::appendLittleEndianFloat(std::vector<uint8_t>& out, float value)
{
  uint8_t bytes[sizeof(float)]{};
  std::memcpy(bytes, &value, sizeof(float));

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  out.push_back(bytes[3]);
  out.push_back(bytes[2]);
  out.push_back(bytes[1]);
  out.push_back(bytes[0]);
#else
  out.insert(out.end(), bytes, bytes + sizeof(float));
#endif
}

bool HilFrame::readLittleEndianFloat(const std::vector<uint8_t>& in, std::size_t offset, float& value)
{
  if (offset + sizeof(float) > in.size())
    return false;

  uint8_t bytes[sizeof(float)]{};

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  bytes[0] = in[offset + 3];
  bytes[1] = in[offset + 2];
  bytes[2] = in[offset + 1];
  bytes[3] = in[offset + 0];
#else
  bytes[0] = in[offset + 0];
  bytes[1] = in[offset + 1];
  bytes[2] = in[offset + 2];
  bytes[3] = in[offset + 3];
#endif

  std::memcpy(&value, bytes, sizeof(float));
  return true;
}

void HilFrame::appendEscaped(std::vector<uint8_t>& out, uint8_t byte)
{
  if (byte == kDelimiter || byte == kEscape)
  {
    out.push_back(kEscape);
    out.push_back(static_cast<uint8_t>(byte ^ 0x20));
    return;
  }

  out.push_back(byte);
}

}  // namespace roc_radio_tool_bridge

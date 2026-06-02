#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace roc_radio_tool_bridge
{

class HilFrame
{
public:
  static constexpr uint8_t kDelimiter = 0x7E;
  static constexpr uint8_t kEscape = 0x7D;

  void clear();

  bool addFloat(float value);
  bool getFloat(std::size_t index, float& value) const;

  const std::vector<uint8_t>& payload() const;
  const std::vector<uint8_t>& encoded() const;

  bool build();
  bool decodeFromBytes(const std::vector<uint8_t>& encoded_without_delimiters);

  static uint16_t checksum(const uint8_t* data, std::size_t size);

private:
  static void appendLittleEndianFloat(std::vector<uint8_t>& out, float value);
  static bool readLittleEndianFloat(const std::vector<uint8_t>& in, std::size_t offset, float& value);
  static void appendEscaped(std::vector<uint8_t>& out, uint8_t byte);

  std::vector<uint8_t> payload_;
  std::vector<uint8_t> encoded_;
};

}  // namespace roc_radio_tool_bridge

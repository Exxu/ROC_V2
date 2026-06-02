#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace roc_radio_tool_bridge
{

struct MavlinkMessage
{
  uint32_t message_id = 0;
  uint8_t system_id = 0;
  uint8_t component_id = 0;
  std::vector<uint8_t> payload;
};

class MavlinkMinimalEndpoint
{
public:
  MavlinkMinimalEndpoint();
  ~MavlinkMinimalEndpoint();

  MavlinkMinimalEndpoint(const MavlinkMinimalEndpoint&) = delete;
  MavlinkMinimalEndpoint& operator=(const MavlinkMinimalEndpoint&) = delete;

  bool openEndpoint(const std::string& bind_ip,
                    uint16_t bind_port,
                    const std::string& request_ip,
                    uint16_t request_port);

  void closeEndpoint();
  bool isOpen() const;

  bool receiveMessage(MavlinkMessage& message, unsigned int timeout_ms);

  bool sendSetMessageInterval(uint32_t mavlink_message_id,
                              float rate_hz,
                              uint8_t target_system,
                              uint8_t target_component);

private:
  static uint16_t crcCalculate(const uint8_t* data, std::size_t length);
  static void crcAccumulate(uint8_t data, uint16_t& crc);
  static uint8_t crcExtraForMessage(uint32_t message_id);

  bool parseDatagram(const uint8_t* data, std::size_t size, MavlinkMessage& message) const;
  bool parseV1Frame(const uint8_t* data, std::size_t size, MavlinkMessage& message) const;
  bool parseV2Frame(const uint8_t* data, std::size_t size, MavlinkMessage& message) const;

  static void appendFloatLE(std::vector<uint8_t>& out, float value);
  static void appendUInt16LE(std::vector<uint8_t>& out, uint16_t value);
  static void appendUInt32LE(std::vector<uint8_t>& out, uint32_t value);

  int socket_fd_;
  uint8_t sequence_;
  std::string request_ip_;
  uint16_t request_port_;
};

bool extractRcChannelPwm(const MavlinkMessage& message, int channel, uint16_t& pwm_us);

}  // namespace roc_radio_tool_bridge

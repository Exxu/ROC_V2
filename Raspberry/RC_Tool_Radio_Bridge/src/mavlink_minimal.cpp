#include "roc_radio_tool_bridge/mavlink_minimal.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace roc_radio_tool_bridge
{
namespace
{
constexpr uint8_t kMavlinkV1Stx = 0xFE;
constexpr uint8_t kMavlinkV2Stx = 0xFD;
constexpr uint32_t kMsgIdRcChannels = 65;
constexpr uint32_t kMsgIdCommandLong = 76;
constexpr uint16_t kMavCmdSetMessageInterval = 511;
constexpr uint8_t kCompanionSystemId = 255;
constexpr uint8_t kCompanionComponentId = 190;
constexpr uint8_t kMavlinkV2SignedFlag = 0x01;
}

MavlinkMinimalEndpoint::MavlinkMinimalEndpoint()
  : socket_fd_(-1), sequence_(0), request_port_(0)
{
}

MavlinkMinimalEndpoint::~MavlinkMinimalEndpoint()
{
  closeEndpoint();
}

bool MavlinkMinimalEndpoint::openEndpoint(const std::string& bind_ip,
                                          uint16_t bind_port,
                                          const std::string& request_ip,
                                          uint16_t request_port)
{
  closeEndpoint();

  socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd_ < 0)
  {
    std::cerr << "ERROR: cannot create UDP socket: " << std::strerror(errno) << std::endl;
    return false;
  }

  int reuse = 1;
  ::setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in bind_addr{};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(bind_port);
  if (::inet_pton(AF_INET, bind_ip.c_str(), &bind_addr.sin_addr) != 1)
  {
    std::cerr << "ERROR: invalid MAVLink bind IP: " << bind_ip << std::endl;
    closeEndpoint();
    return false;
  }

  if (::bind(socket_fd_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0)
  {
    std::cerr << "ERROR: cannot bind UDP " << bind_ip << ":" << bind_port
              << ": " << std::strerror(errno) << std::endl;
    closeEndpoint();
    return false;
  }

  request_ip_ = request_ip;
  request_port_ = request_port;
  return true;
}

void MavlinkMinimalEndpoint::closeEndpoint()
{
  if (socket_fd_ >= 0)
  {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

bool MavlinkMinimalEndpoint::isOpen() const
{
  return socket_fd_ >= 0;
}

bool MavlinkMinimalEndpoint::receiveMessage(MavlinkMessage& message, unsigned int timeout_ms)
{
  if (socket_fd_ < 0)
    return false;

  pollfd pfd{};
  pfd.fd = socket_fd_;
  pfd.events = POLLIN;

  const int ret = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
  if (ret <= 0)
    return false;

  uint8_t buffer[512]{};
  const ssize_t n = ::recv(socket_fd_, buffer, sizeof(buffer), 0);
  if (n <= 0)
    return false;

  return parseDatagram(buffer, static_cast<std::size_t>(n), message);
}

bool MavlinkMinimalEndpoint::sendSetMessageInterval(uint32_t mavlink_message_id,
                                                    float rate_hz,
                                                    uint8_t target_system,
                                                    uint8_t target_component)
{
  if (socket_fd_ < 0)
    return false;

  if (request_ip_.empty() || request_port_ == 0)
    return false;

  const float interval_us = (rate_hz > 0.0f) ? (1000000.0f / rate_hz) : -1.0f;

  std::vector<uint8_t> payload;
  payload.reserve(33);

  appendFloatLE(payload, static_cast<float>(mavlink_message_id));  // param1: message ID
  appendFloatLE(payload, interval_us);                             // param2: interval in us
  appendFloatLE(payload, 0.0f);
  appendFloatLE(payload, 0.0f);
  appendFloatLE(payload, 0.0f);
  appendFloatLE(payload, 0.0f);
  appendFloatLE(payload, 0.0f);
  appendUInt16LE(payload, kMavCmdSetMessageInterval);
  payload.push_back(target_system);
  payload.push_back(target_component);
  payload.push_back(0);  // confirmation

  std::vector<uint8_t> frame;
  frame.reserve(6 + payload.size() + 2);
  frame.push_back(kMavlinkV1Stx);
  frame.push_back(static_cast<uint8_t>(payload.size()));
  frame.push_back(sequence_++);
  frame.push_back(kCompanionSystemId);
  frame.push_back(kCompanionComponentId);
  frame.push_back(static_cast<uint8_t>(kMsgIdCommandLong));
  frame.insert(frame.end(), payload.begin(), payload.end());

  uint16_t crc = crcCalculate(frame.data() + 1, frame.size() - 1);
  crcAccumulate(crcExtraForMessage(kMsgIdCommandLong), crc);
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(request_port_);
  if (::inet_pton(AF_INET, request_ip_.c_str(), &dst.sin_addr) != 1)
  {
    std::cerr << "ERROR: invalid MAVLink request IP: " << request_ip_ << std::endl;
    return false;
  }

  const ssize_t sent = ::sendto(socket_fd_, frame.data(), frame.size(), 0,
                                reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
  return sent == static_cast<ssize_t>(frame.size());
}

uint16_t MavlinkMinimalEndpoint::crcCalculate(const uint8_t* data, std::size_t length)
{
  uint16_t crc = 0xFFFF;
  for (std::size_t i = 0; i < length; ++i)
    crcAccumulate(data[i], crc);
  return crc;
}

void MavlinkMinimalEndpoint::crcAccumulate(uint8_t data, uint16_t& crc)
{
  uint8_t tmp = data ^ static_cast<uint8_t>(crc & 0xFF);
  tmp ^= static_cast<uint8_t>(tmp << 4);
  crc = static_cast<uint16_t>((crc >> 8) ^ (static_cast<uint16_t>(tmp) << 8) ^
                              (static_cast<uint16_t>(tmp) << 3) ^ (static_cast<uint16_t>(tmp) >> 4));
}

uint8_t MavlinkMinimalEndpoint::crcExtraForMessage(uint32_t message_id)
{
  switch (message_id)
  {
    case kMsgIdRcChannels: return 118;
    case kMsgIdCommandLong: return 152;
    default: return 0;
  }
}

bool MavlinkMinimalEndpoint::parseDatagram(const uint8_t* data, std::size_t size, MavlinkMessage& message) const
{
  if (data == nullptr || size < 8)
    return false;

  for (std::size_t i = 0; i < size; ++i)
  {
    if (data[i] == kMavlinkV1Stx)
    {
      if (parseV1Frame(data + i, size - i, message))
        return true;
    }
    else if (data[i] == kMavlinkV2Stx)
    {
      if (parseV2Frame(data + i, size - i, message))
        return true;
    }
  }

  return false;
}

bool MavlinkMinimalEndpoint::parseV1Frame(const uint8_t* data, std::size_t size, MavlinkMessage& message) const
{
  if (size < 8 || data[0] != kMavlinkV1Stx)
    return false;

  const uint8_t len = data[1];
  const std::size_t frame_size = static_cast<std::size_t>(len) + 8;
  if (size < frame_size)
    return false;

  const uint32_t msgid = data[5];
  const uint16_t received_crc = static_cast<uint16_t>(data[6 + len]) |
                                (static_cast<uint16_t>(data[7 + len]) << 8);

  uint16_t crc = crcCalculate(data + 1, 5 + len);
  crcAccumulate(crcExtraForMessage(msgid), crc);
  if (crc != received_crc)
    return false;

  message.message_id = msgid;
  message.system_id = data[3];
  message.component_id = data[4];
  message.payload.assign(data + 6, data + 6 + len);
  return true;
}

bool MavlinkMinimalEndpoint::parseV2Frame(const uint8_t* data, std::size_t size, MavlinkMessage& message) const
{
  if (size < 12 || data[0] != kMavlinkV2Stx)
    return false;

  const uint8_t len = data[1];
  const uint8_t incompat_flags = data[2];
  const uint32_t msgid = static_cast<uint32_t>(data[7]) |
                         (static_cast<uint32_t>(data[8]) << 8) |
                         (static_cast<uint32_t>(data[9]) << 16);

  const std::size_t signature_size = (incompat_flags & kMavlinkV2SignedFlag) ? 13 : 0;
  const std::size_t frame_size = static_cast<std::size_t>(len) + 12 + signature_size;
  if (size < frame_size)
    return false;

  const uint16_t received_crc = static_cast<uint16_t>(data[10 + len]) |
                                (static_cast<uint16_t>(data[11 + len]) << 8);

  uint16_t crc = crcCalculate(data + 1, 9 + len);
  crcAccumulate(crcExtraForMessage(msgid), crc);
  if (crc != received_crc)
    return false;

  message.message_id = msgid;
  message.system_id = data[5];
  message.component_id = data[6];
  message.payload.assign(data + 10, data + 10 + len);
  return true;
}

void MavlinkMinimalEndpoint::appendFloatLE(std::vector<uint8_t>& out, float value)
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

void MavlinkMinimalEndpoint::appendUInt16LE(std::vector<uint8_t>& out, uint16_t value)
{
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void MavlinkMinimalEndpoint::appendUInt32LE(std::vector<uint8_t>& out, uint32_t value)
{
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

bool extractRcChannelPwm(const MavlinkMessage& message, int channel, uint16_t& pwm_us)
{
  if (message.message_id != kMsgIdRcChannels)
    return false;

  if (channel < 1 || channel > 18)
    return false;

  const std::size_t offset = 4U + static_cast<std::size_t>(channel - 1) * 2U;
  if (offset + 1U >= message.payload.size())
    return false;

  pwm_us = static_cast<uint16_t>(message.payload[offset]) |
           (static_cast<uint16_t>(message.payload[offset + 1U]) << 8);

  if (pwm_us == 0 || pwm_us == UINT16_MAX)
    return false;

  return true;
}

}  // namespace roc_radio_tool_bridge

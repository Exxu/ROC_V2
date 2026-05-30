#include "hil_communication/hil_udp.h"

#include <cstddef>
#include <vector>
#include <utility>

HilUdp::HilUdp(Udp* udp)
  : _udp(udp)
{
}

HilUdp::~HilUdp() = default;

HilUdp::HilUdp(HilUdp&& other) noexcept
  : _udp(std::move(other._udp))
{
}

HilUdp& HilUdp::operator=(HilUdp&& other) noexcept
{
  if (&other == this)
    return *this;

  std::lock_guard<std::mutex> lock(_mutex);
  _udp = std::move(other._udp);
  return *this;
}

const Udp* HilUdp::getUdp() const
{
  return _udp.get();
}

Udp* HilUdp::getUdp()
{
  return _udp.get();
}

bool HilUdp::readFrame(Frame* frame)
{
  std::lock_guard<std::mutex> lock(_mutex);

  if (frame == nullptr || _udp == nullptr || !_udp->isOpen())
    return false;

  std::vector<uint8_t> datagram;
  const std::size_t n = _udp->receiveBytes(&datagram);

  if (n < 2)
    return false;

  frame->clear();

  // UDP preserves datagram boundaries, so one datagram must contain one full encoded frame.
  frame->addBytes(datagram.data(), static_cast<i32>(datagram.size()));

  if (frame->buffer_size() < 2)
    return false;

  if (frame->buffer()[0] != frame->header())
    return false;

  if (frame->buffer()[frame->buffer_size() - 1] != frame->end())
    return false;

  return true;
}

bool HilUdp::sendFrame(Frame& frame)
{
  std::lock_guard<std::mutex> lock(_mutex);

  if (_udp == nullptr || !_udp->isOpen())
    return false;

  frame.build();

  if (frame.buffer() == nullptr || frame.buffer_size() <= 0)
    return false;

  const std::size_t expected = static_cast<std::size_t>(frame.buffer_size());
  const std::size_t sent = _udp->sendBytes(frame.buffer(), expected);

  return sent == expected;
}

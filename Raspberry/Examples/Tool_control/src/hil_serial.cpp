#include "hil_communication/hil_serial.h"

#include <cstddef>
#include <utility>
#include <chrono>

HilSerial::HilSerial(Serial* serial)
  : _serial(serial)
{
}

HilSerial::~HilSerial() = default;

HilSerial::HilSerial(HilSerial&& other) noexcept
  : _serial(std::move(other._serial))
{
}

HilSerial& HilSerial::operator=(HilSerial&& other) noexcept
{
  if (&other == this)
    return *this;

  std::lock_guard<std::mutex> lock(_mutex);
  _serial = std::move(other._serial);
  return *this;
}

const Serial* HilSerial::getSerial() const
{
  return _serial.get();
}

Serial* HilSerial::getSerial()
{
  return _serial.get();
}

bool HilSerial::readFrame(Frame* frame)
{
  std::lock_guard<std::mutex> lock(_mutex);

  if (frame == nullptr || _serial == nullptr || !_serial->isOpen())
    return false;

  frame->clear();

  const uint8_t header = frame->header();
  const uint8_t footer = frame->end();

  uint8_t byte = 0;

  // Wait for frame header. This is intentionally blocking.
  while (true)
  {
    if (_serial->readByte(&byte) != 1)
      return false;

    if (byte == header)
    {
      frame->addHeader();
      break;
    }
  }

  // Read until frame footer. This is intentionally blocking.
  while (true)
  {
    if (_serial->readByte(&byte) != 1)
      return false;

    if (byte == footer)
    {
      frame->addEnd();
      return true;
    }

    frame->addByte(byte);
  }
}


bool HilSerial::readFrameTimed(Frame* frame, unsigned int timeout_ms)
{
  std::lock_guard<std::mutex> lock(_mutex);

  if (frame == nullptr || _serial == nullptr || !_serial->isOpen())
    return false;

  frame->clear();

  const uint8_t header = frame->header();
  const uint8_t footer = frame->end();
  const auto start = std::chrono::steady_clock::now();

  bool inside_frame = false;
  uint8_t byte = 0;

  auto remainingTimeoutMs = [&]() -> unsigned int {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    if (elapsed_ms < 0)
      return timeout_ms;

    if (static_cast<unsigned long long>(elapsed_ms) >= timeout_ms)
      return 0U;

    return static_cast<unsigned int>(timeout_ms - static_cast<unsigned int>(elapsed_ms));
  };

  while (true)
  {
    const unsigned int remaining_ms = remainingTimeoutMs();
    if (remaining_ms == 0U)
      return false;

    if (_serial->readByteTimed(&byte, remaining_ms) != 1)
      return false;

    if (!inside_frame)
    {
      if (byte == header)
      {
        frame->clear();
        frame->addHeader();
        inside_frame = true;
      }
      continue;
    }

    if (byte == footer)
    {
      frame->addEnd();

      // HEADER and END are both 0x7E in this protocol. When two frames are
      // adjacent, the stream may contain ...0x7E 0x7E... . If the first 0x7E
      // was interpreted as a header, the second closes an empty frame. Ignore
      // that empty frame and reuse the second 0x7E as the next header.
      if (frame->buffer_size() <= 2)
      {
        frame->clear();
        frame->addHeader();
        inside_frame = true;
        continue;
      }

      return true;
    }

    frame->addByte(byte);
  }
}

bool HilSerial::sendFrame(Frame& frame)
{
  std::lock_guard<std::mutex> lock(_mutex);

  if (_serial == nullptr || !_serial->isOpen())
    return false;

  frame.build();

  if (frame.buffer() == nullptr || frame.buffer_size() <= 0)
    return false;

  const std::size_t expected = static_cast<std::size_t>(frame.buffer_size());
  const std::size_t sent = _serial->sendBytes(frame.buffer(), expected);

  return sent == expected;
}

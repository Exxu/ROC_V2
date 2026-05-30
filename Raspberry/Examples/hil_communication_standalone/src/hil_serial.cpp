#include "hil_communication/hil_serial.h"

#include <cstddef>
#include <utility>

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

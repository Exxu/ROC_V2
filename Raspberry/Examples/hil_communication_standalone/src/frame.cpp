#include "hil_communication/frame.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace
{
bool hostIsLittleEndian()
{
  const uint16_t value = 0x0001;
  return *reinterpret_cast<const uint8_t*>(&value) == 0x01;
}

template <typename T>
void appendLittleEndian(std::vector<u8>& dst, const T& value)
{
  static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

  u8 bytes[sizeof(T)];
  std::memcpy(bytes, &value, sizeof(T));

  if (hostIsLittleEndian())
  {
    dst.insert(dst.end(), bytes, bytes + sizeof(T));
  }
  else
  {
    dst.insert(dst.end(), std::reverse_iterator<u8*>(bytes + sizeof(T)), std::reverse_iterator<u8*>(bytes));
  }
}

template <typename T>
T readLittleEndian(const std::vector<u8>& src, i32& offset)
{
  static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

  T value{};
  if (offset < 0 || static_cast<std::size_t>(offset) + sizeof(T) > src.size())
  {
    assert(false && "Frame payload does not contain enough bytes");
    return value;
  }

  u8 bytes[sizeof(T)];
  std::copy(src.begin() + offset, src.begin() + offset + static_cast<i32>(sizeof(T)), bytes);

  if (!hostIsLittleEndian())
  {
    std::reverse(bytes, bytes + sizeof(T));
  }

  std::memcpy(&value, bytes, sizeof(T));
  offset += static_cast<i32>(sizeof(T));
  return value;
}

std::vector<u8> escapedCopy(const std::vector<u8>& input, u8 header, u8 end, u8 escape)
{
  std::vector<u8> out;
  out.reserve(input.size());

  for (u8 byte : input)
  {
    if (byte == header || byte == end || byte == escape)
    {
      out.push_back(escape);
      out.push_back(static_cast<u8>(byte ^ 0x20));
    }
    else
    {
      out.push_back(byte);
    }
  }

  return out;
}

std::vector<u8> unescapedCopy(const std::vector<u8>& input, u8 escape)
{
  std::vector<u8> out;
  out.reserve(input.size());

  for (std::size_t i = 0; i < input.size(); ++i)
  {
    const u8 byte = input[i];
    if (byte == escape)
    {
      if (i + 1 >= input.size())
      {
        // Malformed escape sequence; preserve the escape marker so checksum fails.
        out.push_back(byte);
        break;
      }

      ++i;
      out.push_back(static_cast<u8>(input[i] ^ 0x20));
    }
    else
    {
      out.push_back(byte);
    }
  }

  return out;
}
}  // namespace

Frame::Frame()
  : _header(0x7E),
    _end(0x7E),
    _escape(0x7D),
    _cksum(0),
    _current(0),
    _status(false),
    _complete(false)
{
}

u8 Frame::header()
{
  return _header;
}

u8 Frame::end()
{
  return _end;
}

u8 Frame::escape()
{
  return _escape;
}

i32 Frame::size()
{
  return static_cast<i32>(_data.size());
}

u8* Frame::buffer()
{
  return _buffer.empty() ? nullptr : _buffer.data();
}

i32 Frame::buffer_size()
{
  return static_cast<i32>(_buffer.size());
}

u8* Frame::data()
{
  return _data.empty() ? nullptr : _data.data();
}

void Frame::setData(u8* data, i32 size, i32 alloc_size, bool delData)
{
  (void)alloc_size;
  (void)delData;

  assert(data != nullptr);
  assert(size >= 0);

  if (data == nullptr || size <= 0)
  {
    _data.clear();
    return;
  }

  _data.assign(data, data + size);
  _current = 0;
  _status = false;
}

void Frame::setHeader(u8 header)
{
  _header = header;
}

void Frame::setEnd(u8 end)
{
  _end = end;
}

void Frame::setEscape(u8 escape)
{
  _escape = escape;
}

void Frame::setSize(i32 size)
{
  setBufferSize(size);
}

void Frame::setBufferSize(i32 size)
{
  assert(size >= 0);
  _buffer.resize(static_cast<std::size_t>(std::max<i32>(0, size)));
}

void Frame::setDataSize(i32 size)
{
  assert(size >= 0);
  _data.resize(static_cast<std::size_t>(std::max<i32>(0, size)));
}

void* Frame::realoc(void** ptr, std::size_t old_size, std::size_t new_size)
{
  (void)old_size;
  if (ptr == nullptr)
    return nullptr;

  void* new_ptr = std::realloc(*ptr, new_size);
  if (new_ptr != nullptr || new_size == 0)
    *ptr = new_ptr;
  return new_ptr;
}

void* Frame::realoc(void* ptr, std::size_t old_size, std::size_t new_size)
{
  (void)old_size;
  return std::realloc(ptr, new_size);
}

u16 Frame::checksum(u8* data, int count)
{
  if (data == nullptr || count <= 0)
    return 0;

  u16 sum1 = 0;
  u16 sum2 = 0;

  for (int i = 0; i < count; ++i)
  {
    sum1 = static_cast<u16>((sum1 + data[i]) % 255);
    sum2 = static_cast<u16>((sum2 + sum1) % 255);
  }

  return static_cast<u16>((sum2 << 8) | sum1);
}

void Frame::addByte(u8 byte)
{
  _buffer.push_back(byte);
}

void Frame::addHeader()
{
  _buffer.push_back(_header);
}

void Frame::addEnd()
{
  _buffer.push_back(_end);
  _complete = true;
}

void Frame::addBytes(u8* bytes, i32 size)
{
  addBytes2Buffer(bytes, size);
}

void Frame::addBytes2Data(u8* bytes, i32 size)
{
  assert(bytes != nullptr);
  assert(size >= 0);

  if (bytes == nullptr || size <= 0)
    return;

  _data.insert(_data.end(), bytes, bytes + size);
  _status = false;
}

void Frame::addFloat(float num)
{
  appendLittleEndian(_data, num);
  _status = false;
}

float Frame::getFloat()
{
  return readLittleEndian<float>(_data, _current);
}

void Frame::addDouble(double num)
{
  appendLittleEndian(_data, num);
  _status = false;
}

double Frame::getDouble()
{
  return readLittleEndian<double>(_data, _current);
}

void Frame::addInt(i32 num)
{
  appendLittleEndian(_data, num);
  _status = false;
}

i32 Frame::getInt()
{
  return readLittleEndian<i32>(_data, _current);
}

void Frame::addData2Buffer()
{
  if (!_data.empty())
    _buffer.insert(_buffer.end(), _data.begin(), _data.end());
}

void Frame::addBytes2Buffer(u8* bytes, i32 size)
{
  assert(bytes != nullptr);
  assert(size >= 0);

  if (bytes == nullptr || size <= 0)
    return;

  _buffer.insert(_buffer.end(), bytes, bytes + size);
}

bool Frame::isEscapable(u8 byte)
{
  return byte == _header || byte == _end || byte == _escape;
}

void Frame::addChecksum()
{
  _cksum = checksum(_data.data(), static_cast<int>(_data.size()));
  appendLittleEndian(_data, _cksum);
}

bool Frame::check()
{
  _status = (_cksum == checksum(_data.data(), static_cast<int>(_data.size())));
  return _status;
}

void Frame::build(u8* data, i32 size)
{
  setData(data, size);
  build();
}

void Frame::build()
{
  _buffer.clear();
  _complete = false;
  _status = false;
  _current = 0;

  if (_data.empty())
    return;

  _cksum = checksum(_data.data(), static_cast<int>(_data.size()));

  std::vector<u8> payload_with_checksum = _data;
  appendLittleEndian(payload_with_checksum, _cksum);

  const std::vector<u8> escaped = escapedCopy(payload_with_checksum, _header, _end, _escape);

  _buffer.reserve(escaped.size() + 2);
  _buffer.push_back(_header);
  _buffer.insert(_buffer.end(), escaped.begin(), escaped.end());
  _buffer.push_back(_end);

  _complete = true;
  _status = true;
}

bool Frame::unbuild()
{
  _status = false;
  _current = 0;

  if (_buffer.size() < 4)
    return false;

  if (_buffer.front() != _header || _buffer.back() != _end)
    return false;

  std::vector<u8> encoded(_buffer.begin() + 1, _buffer.end() - 1);
  std::vector<u8> decoded = unescapedCopy(encoded, _escape);

  if (decoded.size() < sizeof(u16))
    return false;

  const std::size_t checksum_offset = decoded.size() - sizeof(u16);

  u8 checksum_bytes[sizeof(u16)] = {decoded[checksum_offset], decoded[checksum_offset + 1]};
  if (!hostIsLittleEndian())
    std::reverse(checksum_bytes, checksum_bytes + sizeof(u16));
  std::memcpy(&_cksum, checksum_bytes, sizeof(u16));

  _data.assign(decoded.begin(), decoded.begin() + static_cast<std::ptrdiff_t>(checksum_offset));
  _complete = true;

  return check();
}

i32 Frame::insertEscape()
{
  _data = escapedCopy(_data, _header, _end, _escape);
  return static_cast<i32>(_data.size());
}

i32 Frame::insertEscape(u8** data, i32 size)
{
  if (data == nullptr || *data == nullptr || size <= 0)
    return 0;

  std::vector<u8> input(*data, *data + size);
  std::vector<u8> output = escapedCopy(input, _header, _end, _escape);

  u8* new_data = static_cast<u8*>(std::realloc(*data, output.size()));
  if (new_data == nullptr && !output.empty())
    return 0;

  *data = new_data;
  std::memcpy(*data, output.data(), output.size());
  return static_cast<i32>(output.size());
}

i32 Frame::removeEscape()
{
  _data = unescapedCopy(_data, _escape);
  return static_cast<i32>(_data.size());
}

i32 Frame::removeEscape(u8** data, i32 size)
{
  if (data == nullptr || *data == nullptr || size <= 0)
    return 0;

  std::vector<u8> input(*data, *data + size);
  std::vector<u8> output = unescapedCopy(input, _escape);

  u8* new_data = static_cast<u8*>(std::realloc(*data, output.size()));
  if (new_data == nullptr && !output.empty())
    return 0;

  *data = new_data;
  std::memcpy(*data, output.data(), output.size());
  return static_cast<i32>(output.size());
}

void Frame::copyBuffer2data()
{
  if (_buffer.size() < 2)
  {
    _data.clear();
    return;
  }

  _data.assign(_buffer.begin() + 1, _buffer.end() - 1);
  _current = 0;
}

void Frame::clear()
{
  _data.clear();
  _buffer.clear();
  _cksum = 0;
  _current = 0;
  _status = false;
  _complete = false;
}

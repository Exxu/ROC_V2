#ifndef SYNC_HIL_FRAME_H
#define SYNC_HIL_FRAME_H

#include <cstdint>
#include <cstddef>
#include <vector>

using u8 = uint8_t;
using i8 = int8_t;
using u16 = uint16_t;
using i16 = int16_t;
using i32 = int32_t;
using u32 = uint32_t;

class Frame
{
public:
  Frame();
  virtual ~Frame() = default;

  u8 header();
  u8 end();
  u8 escape();

  // Payload size in bytes. After unbuild(), this excludes header/footer/checksum.
  i32 size();

  // Encoded full frame buffer: header + escaped(payload + checksum) + end.
  u8* buffer();
  i32 buffer_size();

  // Decoded payload buffer.
  u8* data();

  void setData(u8* data, i32 size, i32 alloc_size = -1, bool delData = true);

  void setHeader(u8 header);
  void setEnd(u8 end);
  void setEscape(u8 escape);
  void setSize(i32 size);
  void setBufferSize(i32 size);
  void setDataSize(i32 size);

  // Kept only for source compatibility with the old class.
  void* realoc(void** ptr, std::size_t old_size, std::size_t new_size);
  void* realoc(void* ptr, std::size_t old_size, std::size_t new_size);

  u16 checksum(u8* data, int count);

  // These functions append to the encoded frame buffer. They are mainly used by FramedSerial::readFrame().
  void addEnd();
  void addHeader();
  void addByte(u8 byte);
  void addBytes(u8* bytes, i32 size);

  // These functions append/read typed values to/from the decoded payload.
  void addFloat(float n);
  float getFloat();
  void addDouble(double n);
  double getDouble();
  void addInt(i32 n);
  i32 getInt();

  void addData2Buffer();
  void addBytes2Buffer(u8* bytes, i32 size);
  void addBytes2Data(u8* bytes, i32 size);

  bool isEscapable(u8 byte);

  void addChecksum();
  bool check();

  void build(u8* data, i32 size);
  void build();
  bool unbuild();

  i32 insertEscape();
  i32 insertEscape(u8** data, i32 size);
  i32 removeEscape();
  i32 removeEscape(u8** data, i32 size);

  void copyBuffer2data();
  void clear();

private:
  u8 _header;
  u8 _end;
  u8 _escape;

  std::vector<u8> _data;    // decoded payload
  std::vector<u8> _buffer;  // encoded full frame

  u16 _cksum;
  i32 _current;
  bool _status;
  bool _complete;
};

#endif  // SYNC_HIL_FRAME_H
